/*
 * tuner.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <cinttypes>
#include <iostream>
#include <sstream>

#include "common.h"
#include "config.h"
#include "discover.h"
#include "log.h"
#include "poller.h"
#include "tuner.h"
#include "param.h"
#include "device.h"
#include <vdr/channels.h>
#include <repfunc.h>

cSatipTuner::cSatipTuner(cSatipDevice& deviceP, unsigned int packetLenP)
: cThread(cString::sprintf("SATIP#%d tuner", deviceP.GetId())),
  sleepM(),
  deviceM(deviceP),
  deviceIdM(deviceP.GetId()),
  rtspM(*this),
  rtpM(*this),
  rtcpM(*this),
  streamAddrM(""),
  streamParamM(""),
  lastAddrM(""),
  lastParamM(""),
  tnrParamM(""),
  streamPortM(SATIP_DEFAULT_RTSP_PORT),
  currentServerM(NULL, deviceP.GetId(), 0),
  nextServerM(NULL, deviceP.GetId(), 0),
  mutexM(),
  reConnectM(),
  keepAliveM(),
  statusUpdateM(),
  pidUpdateCacheM(),
  setupTimeoutM(-1),
  sessionM(""),
  currentStateM(tsIdle),
  internalStateM(),
  externalStateM(),
  timeoutM(eMinKeepAliveIntervalMs - eKeepAlivePreBufferMs),
  hasLockM(false),
  signalStrengthDBmM(0.0),
  signalStrengthM(-1),
  signalQualityM(-1),
  frontendIdM(-1),
  streamIdM(-1),
  pmtPidM(-1),
  addPidsM(),
  delPidsM(),
  pidsM()
{
  dbg_funcname("%s (, %d) [device %d]", __PRETTY_FUNCTION__, packetLenP, deviceIdM);

  // Open sockets
  int i = SatipConfig.GetPortRangeStart() ? SatipConfig.GetPortRangeStop() - SatipConfig.GetPortRangeStart() - 1 : 100;
  int port = SatipConfig.GetPortRangeStart();
  while (i-- > 0) {
        // RTP must use an even port number
        if (rtpM.Open(port) && (rtpM.Port() % 2 == 0) && rtcpM.Open(rtpM.Port() + 1))
           break;
        rtpM.Close();
        rtcpM.Close();
        if (SatipConfig.GetPortRangeStart())
           port += 2;
        }
  if ((rtpM.Port() <= 0) || (rtcpM.Port() <= 0)) {
     error("Cannot open required RTP/RTCP ports [device %d]", deviceIdM);
     }
  // Must be done after socket initialization!
  cSatipPoller::GetInstance()->Register(rtpM);
  cSatipPoller::GetInstance()->Register(rtcpM);

  // Start thread
  Start();
}

cSatipTuner::~cSatipTuner()
{
  dbg_funcname("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  // Stop thread
  sleepM.Signal();
  if (Running())
     Cancel(3);
  Close();
  currentStateM = tsIdle;
  internalStateM.Clear();
  externalStateM.Clear();

  // Close the listening sockets
  cSatipPoller::GetInstance()->Unregister(rtcpM);
  cSatipPoller::GetInstance()->Unregister(rtpM);
  rtcpM.Close();
  rtpM.Close();
}

void cSatipTuner::SetPowerSaveMode(bool On) {
  cMutexLock MutexLock(&mutexM);
  if (On and currentStateM > tsRelease) {
     dbg_funcname("%s closing device %d", __PRETTY_FUNCTION__, deviceIdM);
     RequestState(tsRelease, smExternal);
     }
}

void cSatipTuner::Action(void)
{
  dbg_funcname("%s Entering [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  bool lastIdleStatus = false;
  cTimeMs idleCheck(eIdleCheckTimeoutMs);
  cTimeMs tuning(eTuningTimeoutMs);
  reConnectM.Set(eConnectTimeoutMs);
  // Do the thread loop
  while (Running()) {
        UpdateCurrentState();
        switch (currentStateM) {
          case tsIdle:
               dbg_tunerstate("%s: tsIdle [device %d]", __PRETTY_FUNCTION__, deviceIdM);
               break;
          case tsRelease:
               dbg_tunerstate("%s: tsRelease [device %d]", __PRETTY_FUNCTION__, deviceIdM);
               Disconnect();
               RequestState(tsIdle, smInternal);
               break;
          case tsSet:
               dbg_tunerstate("%s: tsSet [device %d]", __PRETTY_FUNCTION__, deviceIdM);
               if (currentServerM.IsQuirk(cSatipServer::eSatipQuirkTearAndPlay))
                  Disconnect();
               if (Connect()) {
                  tuning.Set(eTuningTimeoutMs);
                  RequestState(tsTuned, smInternal);
                  UpdatePids(true);
                  }
               else
                  Disconnect();
               break;
          case tsTuned:
               dbg_tunerstate("%s: tsTuned [device %d]", __PRETTY_FUNCTION__, deviceIdM);
               deviceM.SetChannelTuned();
               reConnectM.Set(eConnectTimeoutMs);
               idleCheck.Set(eIdleCheckTimeoutMs);
               lastIdleStatus = false;
               // Read reception statistics via DESCRIBE and RTCP
               if (hasLockM || ReadReceptionStatus()) {
                  // Quirk for devices without valid reception data
                  if (currentServerM.IsQuirk(cSatipServer::eSatipQuirkForceLock)) {
                     hasLockM = true;
                     signalStrengthDBmM = eDefaultSignalStrengthDBm;
                     signalStrengthM = eDefaultSignalStrength;
                     signalQualityM = eDefaultSignalQuality;
                     }
                  if (hasLockM)
                     RequestState(tsLocked, smInternal);
                  }
               else if (tuning.TimedOut()) {
                  error("Tuning timeout - retuning [device %d]", deviceIdM);
                  RequestState(tsSet, smInternal);
                  }
               break;
          case tsLocked:
               dbg_tunerstate("%s: tsLocked [device %d]", __PRETTY_FUNCTION__, deviceIdM);
               if (!UpdatePids()) {
                  error("Pid update failed - retuning [device %d]", deviceIdM);
                  RequestState(tsSet, smInternal);
                  break;
                  }
               if (!KeepAlive()) {
                  error("Keep-alive failed - retuning [device %d]", deviceIdM);
                  RequestState(tsSet, smInternal);
                  break;
                  }
               if (reConnectM.TimedOut()) {
                  error("Connection timeout - retuning [device %d]", deviceIdM);
                  RequestState(tsSet, smInternal);
                  break;
                  }
               if (idleCheck.TimedOut()) {
                  bool currentIdleStatus = deviceM.IsIdle();
                  if (lastIdleStatus && currentIdleStatus) {
                     info("Idle timeout - releasing [device %d]", deviceIdM);
                     RequestState(tsRelease, smInternal);
                     }
                  lastIdleStatus = currentIdleStatus;
                  idleCheck.Set(eIdleCheckTimeoutMs);
                  break;
                  }
               Receive();
               break;
          default:
               error("Unknown tuner status %d [device %d]", currentStateM, deviceIdM);
               break;
          }
        if (!StateRequested())
           sleepM.Wait(eSleepTimeoutMs); // to avoid busy loop and reduce cpu load
        }
  dbg_funcname("%s Exiting [device %d]", __PRETTY_FUNCTION__, deviceIdM);
}

bool cSatipTuner::Open(void)
{
  cMutexLock MutexLock(&mutexM);
  dbg_funcname("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  // return always true
  return true;
}

bool cSatipTuner::Close(void)
{
  cMutexLock MutexLock(&mutexM);
  dbg_funcname("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  if (setupTimeoutM.TimedOut())
     RequestState(tsRelease, smExternal);

  // return always true
  return true;
}

bool cSatipTuner::Connect(void)
{
  cMutexLock MutexLock(&mutexM);
  dbg_funcname("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  if (!isempty(*streamAddrM)) {
     cString connectionUri = GetBaseUrl(*streamAddrM, streamPortM);
     tnrParamM = "";
     // Just retune
     if (streamIdM >= 0) {
        if (!strcmp(*streamParamM, *lastParamM) && hasLockM) {
           dbg_funcname("%s Identical parameters [device %d]", __PRETTY_FUNCTION__, deviceIdM);
           //return true; // fall through because detection does not work reliably
           }
        cString uri = cString::sprintf("%sstream=%d?%s", *connectionUri, streamIdM, *streamParamM);
        dbg_funcname("%s Retuning [device %d]", __PRETTY_FUNCTION__, deviceIdM);
        if (rtspM.Play(*uri)) {
           keepAliveM.Set(timeoutM);
           lastParamM = streamParamM;
           return true;
           }
        }
     else if (rtspM.SetInterface(nextServerM.IsValid() ? *nextServerM.GetSrcAddress() : NULL) && rtspM.Options(*connectionUri)) {
        cString uri = cString::sprintf("%s?%s", *connectionUri, *streamParamM);
        bool useTcp = SatipConfig.IsTransportModeRtpOverTcp() && nextServerM.IsValid() && nextServerM.IsQuirk(cSatipServer::eSatipQuirkRtpOverTcp);
        // Flush any old content
        //rtpM.Flush();
        //rtcpM.Flush();
        if (useTcp)
           dbg_funcname("%s Requesting TCP [device %d]", __PRETTY_FUNCTION__, deviceIdM);
        if (rtspM.Setup(*uri, rtpM.Port(), rtcpM.Port(), useTcp)) {
           lastParamM = streamParamM;
           keepAliveM.Set(timeoutM);
           if (nextServerM.IsValid()) {
              currentServerM = nextServerM;
              nextServerM.Reset();
              }
           lastAddrM = connectionUri;
           currentServerM.Attach();
           return true;
           }
        }
     rtspM.Reset();
     streamIdM = -1;
     error("Connect failed [device %d]", deviceIdM);
     }

  return false;
}

bool cSatipTuner::Disconnect(void)
{
  cMutexLock MutexLock(&mutexM);
  dbg_funcname("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  if (!isempty(*lastAddrM) && (streamIdM >= 0)) {
     cString uri = cString::sprintf("%sstream=%d", *lastAddrM, streamIdM);
     rtspM.Teardown(*uri);
     // some devices requires a teardown for TCP connection also
     rtspM.Reset();
     streamIdM = -1;
     }

  // Reset signal parameters
  hasLockM = false;
  signalStrengthDBmM = 0.0;
  signalStrengthM = -1;
  signalQualityM = -1;
  frontendIdM = -1;

  currentServerM.Detach();
  statusUpdateM.Set(0);
  timeoutM = eMinKeepAliveIntervalMs - eKeepAlivePreBufferMs;
  pmtPidM = -1;
  addPidsM.Clear();
  delPidsM.Clear();

  // return always true
  return true;
}

void cSatipTuner::ProcessVideoData(u_char *bufferP, int lengthP)
{
  dbg_funcname_ext("%s (, %d) [device %d]", __PRETTY_FUNCTION__, lengthP, deviceIdM);
  if (lengthP > 0) {
     uint64_t elapsed;
     cTimeMs processing(0);

     AddTunerStatistic(lengthP);
     elapsed = processing.Elapsed();
     if (elapsed > 1)
        dbg_rtp_perf("%s AddTunerStatistic() took %" PRIu64 " ms [device %d]", __PRETTY_FUNCTION__, elapsed, deviceIdM);

     processing.Set(0);
     deviceM.WriteData(bufferP, lengthP);
     elapsed = processing.Elapsed();
     if (elapsed > 1)
        dbg_rtp_perf("%s WriteData() took %" PRIu64 " ms [device %d]", __FUNCTION__, elapsed, deviceIdM);
     }
  reConnectM.Set(eConnectTimeoutMs);
}

void cSatipTuner::ProcessRtpData(u_char *bufferP, int lengthP)
{
  rtpM.Process(bufferP, lengthP);
}

void cSatipTuner::ProcessApplicationData(u_char *bufferP, int lengthP)
{
  dbg_funcname_ext("%s (%d) [device %d]", __PRETTY_FUNCTION__, lengthP, deviceIdM);
  reConnectM.Set(eConnectTimeoutMs);

  if (lengthP < 33) /* bare minimum. */
     return;

  char s[lengthP+1];
  memcpy(s, (char *)bufferP, lengthP);
  s[lengthP] = 0;

  char* ps = strstr(s, "ver=");
  auto payload = SplitStr(ps, ';');

  if (payload.size() < 3)
     return;

  // DVB-S2: ver=1.0;src=<srcID>;(..)
  // DVB-T2: ver=1.1;(..)
  // DVB-C2: ver=1.2;(..)

  bool isSat   = payload[0] == "ver=1.0";
  bool isTerr  = payload[0] == "ver=1.1";
  bool isCable = payload[0] == "ver=1.2";
  size_t next = 1;
  int srcID = -1;

  if (payload[next].find("src=") == 0)
     srcID = StrToInt(payload[next++].substr(4));

  // tuner=<feID>,<level>,<lock>,<quality>,(..)
  auto params = SplitStr(payload[next++].substr(6), ',');
  while(params.size() < 14)
     params.push_back("");

  dbg_rtcp("%s (%s) [device %d]", __PRETTY_FUNCTION__, ps, deviceIdM);

  // feID:
  frontendIdM = StrToInt(params[0]);

  // level: 0..255
  // 224 corresponds to -25dBm
  //  32 corresponds to -65dBm
  //   0 corresponds to 'no signal' (dBm not available)
  int level = StrToInt(params[1]);
  signalStrengthDBmM = (level > 0) ? 40.0 * (level - 32) / 192.0 - 65.0 : 0.0;
  // Scale value to 0-100
  signalStrengthM = (level >= 0) ? 0.5 + level * 100.0 / 255.0 : -1;

  // lock: "0" = not locked, "1" = locked
  hasLockM = params[2] == "1";

  // quality: 0..15, lowest value corresponds to highest error rate
  // The value 15 shall correspond to
  // -a BER lower than 2x10-4 after Viterbi for DVB-S
  // -a PER lower than 10-7 for DVB-S2
  int quality = StrToInt(params[3]);
  // Scale value to 0-100
  signalQualityM = (hasLockM && (quality >= 0)) ? 0.5 + (quality * 100.0 / 15.0) : 0;

  if (TP.size() == params.size()-4) {
     bool equal = std::equal(params.begin()+4, params.end(), TP.begin());
     if (equal)
        return;
     }
  TP = {params.begin()+4, params.end()};

  cChannel& channel = deviceM.currentChannel;
  std::stringstream ss;

  const bool DebugRtcp = false;

  if (isSat) {
     // <frequency>,<polarisation>,<system>,<type>,<pilots>,<roll_off>,<symbol_rate>,<fec_inner>
     int  Frequency    = lround(StrToFloat(params[4]));                  // <frequency>,
     char Polarisation = (UpperCase(params[5]))[0];                      // <polarisation>,
     int  System       = SatipToVdrParameter("&msys=" + params[6]);      // <system>,
     int  Type         = SatipToVdrParameter("&mtype=" + params[7]);     // <type>,
     int  Pilots       = SatipToVdrParameter("&plts=" + params[8]);      // <pilots>,
     int  RollOff      = SatipToVdrParameter("&ro=" + params[9]);        // <roll_off>,
     int  SymbolRate   = params[10].empty()? 0 : StrToInt(params[10]);   // <symbol_rate>,
     int  Fec          = SatipToVdrParameter("&fec=" + params[11]);      // <fec_inner>
     int  Source       = SrcIdToSource(srcID);

     if (Source < 0)
        Source = channel.Source();
     if (SymbolRate <= 0)
        SymbolRate = channel.Srate();
     ss << Polarisation;
     ss << 'C' << Fec;
     ss << 'M' << Type;
     if (System > 0) {
        ss << 'N' << Pilots;
        ss << 'O' << RollOff;
      //ss << 'P' << 999;
        }
     ss << 'S' << System;
     if (DebugRtcp) {
        std::cout << *(cSource::ToString(Source)) << ":"
                  << Frequency << ":" << ss.str() << ":" << SymbolRate << std::endl;
        }
     channel.SetTransponderData(Source, Frequency, SymbolRate, ss.str().c_str(), true);
     }
  else if (isTerr) {
     // <freq>,<bw>,<msys>,<tmode>,<mtype>,<gi>,<fec>,<plp>,<t2id>,<sm>
     int  Frequency    = lround(StrToFloat(params[4]));                  // <freq>,
     int  BandWidth    = SatipToVdrParameter("&bw=" + params[5]);        // <bw>,
     int  System       = SatipToVdrParameter("&msys=" + params[6]);      // <msys>,
     int  Transmission = SatipToVdrParameter("&tmode=" + params[7]);     // <tmode>,
     int  Type         = SatipToVdrParameter("&mtype=" + params[8]);     // <mtype>,
     int  Guard        = SatipToVdrParameter("&gi=" + params[9]);        // <gi>,
     int  Fec          = SatipToVdrParameter("&fec=" + params[10]);      // <fec>,
     int  Plp          = params[11].empty()? -1 : StrToInt(params[11]);  // <plp> (opt),
     int  T2id         = params[12].empty()? -1 : StrToInt(params[12]);  // <t2id> (opt),
     int  SM           = SatipToVdrParameter("&sm=" + params[next++]);   // <sm> (opt)

     ss << 'B' << BandWidth;
     ss << 'C' << Fec;
     ss << 'G' << Guard;
     ss << 'M' << Type;
     if (System > 0) {
        ss << 'P' << Plp;
        ss << 'Q' << T2id;
        }
     ss << 'S' << System;
     ss << 'T' << Transmission;
     if (System > 0) {
        ss << 'X' << SM;
        }

     if (DebugRtcp) {
        std::cout << Frequency << ":" << ss.str() << std::endl;
        }
     channel.SetTransponderData('T' << 24, Frequency, 0, ss.str().c_str(), true);
     }
  else if (isCable) {
     // <freq>,<bw>,<msys>,<mtype>,<sr>,<c2tft>,<ds>,<plp>,<specinv>
     int  Frequency    = lround(StrToFloat(params[4]));                  // <freq>,
   //int  BandWidth    = SatipToVdrParameter("&bw=" + params[5]);        // <bw> (opt),
   //int  System       = SatipToVdrParameter("&msys=" + params[6]);      // <msys>,
     int  Type         = SatipToVdrParameter("&mtype=" + params[7]);     // <mtype> (opt),
     int  SymbolRate   = params[8].empty()? 0 : StrToInt(params[8]);     // <sr> (opt),
   //int  C2tft        = params[9].empty()? 0 : StrToInt(params[9]);     // <c2tft> (opt),
   //int  DS           = params[10].empty()? 0 : StrToInt(params[10]);   // <ds> (opt),
   //int  Plp          = params[11].empty()? -1 : StrToInt(params[11]);  // <plp> (opt),
     int  Inversion    = params[12].empty()? 999 : StrToInt(params[12]); // <specinv> (opt),

     if (SymbolRate <= 0)
        SymbolRate = channel.Srate();
     ss << 'I' << Inversion;
     ss << 'M' << Type;

     // not used in VDR: BandWidth, System, C2tft, DS, Plp
     if (DebugRtcp) {
        std::cout << Frequency << ":" << ss.str() << ":" << SymbolRate << std::endl;
        }
     channel.SetTransponderData('C' << 24, Frequency, SymbolRate, ss.str().c_str(), true);
     }
}

void cSatipTuner::ProcessRtcpData(u_char *bufferP, int lengthP)
{
  rtcpM.Process(bufferP, lengthP);
}

void cSatipTuner::SetStreamId(int streamIdP)
{
  cMutexLock MutexLock(&mutexM);
  dbg_funcname("%s (%d) [device %d]", __PRETTY_FUNCTION__, streamIdP, deviceIdM);
  streamIdM = streamIdP;
}

void cSatipTuner::SetSessionTimeout(const char *sessionP, int timeoutP)
{
  cMutexLock MutexLock(&mutexM);
  dbg_funcname("%s (%s, %d) [device %d]", __PRETTY_FUNCTION__, sessionP, timeoutP, deviceIdM);
  sessionM = sessionP;
  if (nextServerM.IsQuirk(cSatipServer::eSatipQuirkSessionId) && !isempty(*sessionM) && startswith(*sessionM, "0"))
     rtspM.SetSession(SkipZeroes(*sessionM));
  timeoutM = (timeoutP > eMinKeepAliveIntervalMs) ? timeoutP : eMinKeepAliveIntervalMs;
  timeoutM -= eKeepAlivePreBufferMs;
}

void cSatipTuner::SetupTransport(int rtpPortP, int rtcpPortP, const char *streamAddrP, const char *sourceAddrP)
{
  cMutexLock MutexLock(&mutexM);
  dbg_funcname("%s (%d, %d, %s, %s) [device %d]", __PRETTY_FUNCTION__, rtpPortP, rtcpPortP, streamAddrP, sourceAddrP, deviceIdM);
  bool multicast = !isempty(streamAddrP);
  // Adapt RTP to any transport media change
  if (multicast != rtpM.IsMulticast() || rtpPortP != rtpM.Port()) {
     cSatipPoller::GetInstance()->Unregister(rtpM);
     if (rtpPortP >= 0) {
        rtpM.Close();
        if (multicast)
           rtpM.OpenMulticast(rtpPortP, streamAddrP, sourceAddrP);
        else
           rtpM.Open(rtpPortP);
        cSatipPoller::GetInstance()->Register(rtpM);
        }
     }
  // Adapt RTCP to any transport media change
  if (multicast != rtcpM.IsMulticast() || rtcpPortP != rtcpM.Port()) {
     cSatipPoller::GetInstance()->Unregister(rtcpM);
     if (rtcpPortP >= 0) {
        rtcpM.Close();
        if (multicast)
           rtcpM.OpenMulticast(rtcpPortP, streamAddrP, sourceAddrP);
        else
           rtcpM.Open(rtcpPortP);
        cSatipPoller::GetInstance()->Register(rtcpM);
        }
     }
}

cString cSatipTuner::GetBaseUrl(const char *addressP, const int portP)
{
  dbg_funcname_ext("%s (%s, %d) [device %d]", __PRETTY_FUNCTION__, addressP, portP, deviceIdM);

  if (portP != SATIP_DEFAULT_RTSP_PORT)
     return cString::sprintf("rtsp://%s:%d/", addressP, portP);

  return cString::sprintf("rtsp://%s/", addressP);
}

int cSatipTuner::GetId(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return deviceIdM;
}

bool cSatipTuner::SetSource(cSatipServer *serverP, const int transponderP, const char *parameterP, const int indexP)
{
  dbg_funcname("%s (%d, %s, %d) [device %d]", __PRETTY_FUNCTION__, transponderP, parameterP, indexP, deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (serverP) {
     nextServerM.Set(serverP, transponderP);
     if (!isempty(*nextServerM.GetAddress()) && !isempty(parameterP)) {
        // Update stream address and parameter
        streamAddrM = rtspM.RtspUnescapeString(*nextServerM.GetAddress());
        streamParamM = rtspM.RtspUnescapeString(parameterP);
        streamPortM = nextServerM.GetPort();
        // Modify parameter if required
        if (nextServerM.IsQuirk(cSatipServer::eSatipQuirkForcePilot) && strstr(parameterP, "msys=dvbs2") && !strstr(parameterP, "plts="))
           streamParamM = rtspM.RtspUnescapeString(*cString::sprintf("%s&plts=on", parameterP));
        // Reconnect
        if (!isempty(*lastAddrM)) {
           cString connectionUri = GetBaseUrl(*streamAddrM, streamPortM);
           if (strcmp(*connectionUri, *lastAddrM))
              RequestState(tsRelease, smInternal);
           }
        RequestState(tsSet, smExternal);
        setupTimeoutM.Set(eSetupTimeoutMs);
        }
     }
  else {
     streamAddrM = "";
     streamParamM = "";
     }

  return true;
}

bool cSatipTuner::SetPid(int pidP, int typeP, bool onP)
{
  dbg_funcname_ext("%s (%d, %d, %d) [device %d]", __PRETTY_FUNCTION__, pidP, typeP, onP, deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (onP) {
     pidsM.AddPid(pidP);
     addPidsM.AddPid(pidP);
     delPidsM.RemovePid(pidP);
     }
  else {
     pidsM.RemovePid(pidP);
     delPidsM.AddPid(pidP);
     addPidsM.RemovePid(pidP);
     }
  dbg_pids("%s (%d, %d, %d) pids=%s [device %d]", __PRETTY_FUNCTION__, pidP, typeP, onP, *pidsM.ListPids(), deviceIdM);
  sleepM.Signal();

  return true;
}

bool cSatipTuner::UpdatePids(bool forceP)
{
  dbg_funcname_ext("%s (%d) tunerState=%s [device %d]", __PRETTY_FUNCTION__, forceP, TunerStateString(currentStateM), deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (((forceP && pidsM.Size()) || (pidUpdateCacheM.TimedOut() && (addPidsM.Size() || delPidsM.Size()))) &&
      !isempty(*streamAddrM) && (streamIdM >= 0)) {
     cString uri = cString::sprintf("%sstream=%d", *GetBaseUrl(*streamAddrM, streamPortM), streamIdM);
     bool useci = (SatipConfig.GetCIExtension() && currentServerM.HasCI());
     bool usedummy = currentServerM.IsQuirk(cSatipServer::eSatipQuirkPlayPids);
     bool paramadded = false;
     if (forceP || usedummy) {
        if (pidsM.Size()) {
           uri = cString::sprintf("%s%spids=%s", *uri, paramadded ? "&" : "?", *pidsM.ListPids());
           if (usedummy && (pidsM.Size() == 1) && (pidsM[0] < 0x20))
              uri = cString::sprintf("%s,%d", *uri, eDummyPid);
           paramadded = true;
           }
        }
     else {
        if (addPidsM.Size()) {
           uri = cString::sprintf("%s%saddpids=%s", *uri, paramadded ? "&" : "?", *addPidsM.ListPids());
           paramadded = true;
           }
        if (delPidsM.Size()) {
           uri = cString::sprintf("%s%sdelpids=%s", *uri, paramadded ? "&" : "?", *delPidsM.ListPids());
           paramadded = true;
           }
        }
     if (useci) {
        if (currentServerM.IsQuirk(cSatipServer::eSatipQuirkCiXpmt)) {
           // CI extension parameters:
           // - x_pmt : specifies the PMT of the service you want the CI to decode
           // - x_ci  : specfies which CI slot (1..n) to use
           //           value 0 releases the CI slot
           //           CI slot released automatically if the stream is released,
           //           but not when used retuning to another channel
           int pid = deviceM.GetPmtPid();
           if ((pid > 0) && (pid != pmtPidM)) {
              int slot = deviceM.GetCISlot();
              uri = cString::sprintf("%s%sx_pmt=%d", *uri, paramadded ? "&" : "?", pid);
              if (slot > 0)
                 uri = cString::sprintf("%s&x_ci=%d", *uri, slot);
              paramadded = true;
              }
           pmtPidM = pid;
           }
        else if (currentServerM.IsQuirk(cSatipServer::eSatipQuirkCiTnr)) {
           // CI extension parameters:
           // - tnr : specifies a channel config entry
           cString param = deviceM.GetTnrParameterString();
           if (!isempty(*param) && strcmp(*tnrParamM, *param) != 0) {
              uri = cString::sprintf("%s%stnr=%s", *uri, paramadded ? "&" : "?", *param);
              paramadded = true;
              }
           tnrParamM = param;
           }
        }
     if (paramadded) {
        pidUpdateCacheM.Set(ePidUpdateIntervalMs);
        if (!rtspM.Play(*uri))
           return false;
        }
     addPidsM.Clear();
     delPidsM.Clear();
     }

  return true;
}

bool cSatipTuner::Receive(void)
{
  dbg_funcname_ext("%s tunerState=%s [device %d]", __PRETTY_FUNCTION__, TunerStateString(currentStateM), deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (!isempty(*streamAddrM)) {
     cString uri = GetBaseUrl(*streamAddrM, streamPortM);
     if (!rtspM.Receive(*uri))
        return false;
     }

  return true;
}

bool cSatipTuner::KeepAlive(bool forceP)
{
  dbg_funcname_ext("%s (%d) tunerState=%s [device %d]", __PRETTY_FUNCTION__, forceP, TunerStateString(currentStateM), deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (keepAliveM.TimedOut()) {
     keepAliveM.Set(timeoutM);
     forceP = true;
     }
  if (forceP && !isempty(*streamAddrM)) {
     cString uri = GetBaseUrl(*streamAddrM, streamPortM);
     if (!rtspM.Options(*uri))
        return false;
     }

  return true;
}

bool cSatipTuner::ReadReceptionStatus(bool forceP)
{
  dbg_funcname_ext("%s (%d) tunerState=%s [device %d]", __PRETTY_FUNCTION__, forceP, TunerStateString(currentStateM), deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (statusUpdateM.TimedOut()) {
     statusUpdateM.Set(eStatusUpdateTimeoutMs);
     forceP = true;
     }
  if (forceP && !isempty(*streamAddrM) && (streamIdM >= 0)) {
     cString uri = cString::sprintf("%sstream=%d", *GetBaseUrl(*streamAddrM, streamPortM), streamIdM);
     if (rtspM.Describe(*uri))
        return true;
     }

  return false;
}

void cSatipTuner::UpdateCurrentState(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  cMutexLock MutexLock(&mutexM);
  eTunerState state = currentStateM;

  if (internalStateM.Size()) {
     state = internalStateM.At(0);
     internalStateM.Remove(0);
     }
  else if (externalStateM.Size()) {
     state = externalStateM.At(0);
     externalStateM.Remove(0);
     }

  if (currentStateM != state) {
     dbg_funcname("%s: Switching from %s to %s [device %d]", __PRETTY_FUNCTION__, TunerStateString(currentStateM), TunerStateString(state), deviceIdM);
     currentStateM = state;
     }
}

bool cSatipTuner::StateRequested(void)
{
  cMutexLock MutexLock(&mutexM);
  dbg_funcname_ext("%s current=%s internal=%d external=%d [device %d]", __PRETTY_FUNCTION__, TunerStateString(currentStateM), internalStateM.Size(), externalStateM.Size(), deviceIdM);

  return (internalStateM.Size() || externalStateM.Size());
}

bool cSatipTuner::RequestState(eTunerState stateP, eStateMode modeP)
{
  cMutexLock MutexLock(&mutexM);
  dbg_funcname("%s (%s, %s) current=%s internal=%d external=%d [device %d]", __PRETTY_FUNCTION__, TunerStateString(stateP), StateModeString(modeP), TunerStateString(currentStateM), internalStateM.Size(), externalStateM.Size(), deviceIdM);

  if (modeP == smExternal)
     externalStateM.Append(stateP);
  else if (modeP == smInternal) {
     eTunerState state = internalStateM.Size() ? internalStateM.At(internalStateM.Size() - 1) : currentStateM;

     // validate legal state changes
     switch (state) {
       case tsIdle:
            if (stateP == tsRelease)
               return false;
       case tsRelease:
       case tsSet:
       case tsLocked:
       case tsTuned:
       default:
            break;
       }

     internalStateM.Append(stateP);
     }
  else
     return false;

  return true;
}

const char *cSatipTuner::StateModeString(eStateMode modeP)
{
  switch (modeP) {
    case smInternal:
         return "smInternal";
    case smExternal:
         return "smExternal";
     default:
         break;
    }

   return "---";
}

const char *cSatipTuner::TunerStateString(eTunerState stateP)
{
  switch (stateP) {
    case tsIdle:
         return "tsIdle";
    case tsRelease:
         return "tsRelease";
    case tsSet:
         return "tsSet";
    case tsLocked:
         return "tsLocked";
    case tsTuned:
         return "tsTuned";
    default:
         break;
    }

  return "---";
}

int cSatipTuner::FrontendId(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return frontendIdM;
}

int cSatipTuner::SignalStrength(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return (currentStateM >= tsTuned) ? signalStrengthM : 0;
}

double cSatipTuner::SignalStrengthDBm(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return (currentStateM >= tsTuned) ? signalStrengthDBmM : 0;
}

int cSatipTuner::SignalQuality(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return (currentStateM >= tsTuned) ? signalQualityM : 0;
}

bool cSatipTuner::HasLock(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return (currentStateM >= tsTuned) && hasLockM;
}

cString cSatipTuner::GetSignalStatus(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  switch(currentStateM) {
     case tsTuned ... tsLocked:
        return cString::sprintf("lock=%d strength=%d quality=%d frontend=%d", HasLock(), SignalStrength(), SignalQuality(), FrontendId());
     default:
        return "lock=0 strength=0 quality=0 frontend=-1";
     }
}

cString cSatipTuner::GetInformation(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  switch(currentStateM) {
     case tsIdle ... tsSet:
        return cString::sprintf("no connection: %s", TunerStateString(currentStateM));
     default:
        return cString::sprintf("%s?%s (%s) [stream=%d]", *GetBaseUrl(*streamAddrM, streamPortM), *streamParamM, *rtspM.GetActiveMode(), streamIdM);
     }
}
