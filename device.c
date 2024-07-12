/*
 * device.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
#include <string>     // std::string
#include <vector>     // std::vector
#include <algorithm>  // std::min()
#include <vdr/menu.h> // cRecordControl

#include "config.h"
#include "discover.h"
#include "log.h"
#include "param.h"
#include "device.h"

std::vector<cSatipDevice*> SatipDevices;

cMutex cSatipDevice::SetChannelMtx = cMutex();

cSatipDevice::cSatipDevice(unsigned int DeviceIndex) :
  deviceIndex(DeviceIndex),
  bytesDelivered(0),
  dvrIsOpen(false),
  checkTsBufferM(false),
  currentChannel(),
  SectionFilterHandler(nullptr),
  ReadyTimeout(0),
  tunerLocked()
{
  size_t bufsize = SATIP_BUFFER_SIZE;
  bufsize -= (bufsize % TS_SIZE);
  info("Creating device CardIndex=%d DeviceNumber=%d [device %d]", CardIndex(), DeviceNumber(), deviceIndex);
  tsBuffer = new cRingBufferLinear(bufsize + 1, TS_SIZE);
  if (tsBuffer) {
     tsBuffer->SetTimeouts(10, 10);
     tsBuffer->SetIoThrottle();
     tuner = new cSatipTuner(*this, tsBuffer->Free());

     // Start section handler
     SectionFilterHandler = new cSatipSectionFilterHandler(deviceIndex, bufsize + 1);
     StartSectionHandler();
     }
}

cSatipDevice::~cSatipDevice() {
  dbg_funcname("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  // Release immediately any pending conditional wait
  tunerLocked.Broadcast();
  // Stop section handler
  if (SectionFilterHandler)
     StopSectionHandler();
  DELETE_POINTER(SectionFilterHandler);
  DELETE_POINTER(tuner);
  DELETE_POINTER(tsBuffer);
}

bool cSatipDevice::Initialize(int DeviceCount) {
  dbg_funcname("%s (%d)", __PRETTY_FUNCTION__, DeviceCount);
  DeviceCount = std::min(DeviceCount, SATIP_MAX_DEVICES);
  SatipDevices.reserve(DeviceCount);
  for(int i = 0; i < DeviceCount; i++)
     SatipDevices.push_back(new cSatipDevice(i));
  return true;
}

void cSatipDevice::Shutdown(void) {
  dbg_funcname("%s", __PRETTY_FUNCTION__);
  for(auto device:SatipDevices)
     device->CloseDvr();
}

size_t cSatipDevice::Count(void) {
  return SatipDevices.size();
}

cSatipDevice* cSatipDevice::GetSatipDevice(int cardIndex) {
  dbg_funcname_ext("%s (%d)", __PRETTY_FUNCTION__, cardIndex);
  for(auto device:SatipDevices)
     if (device->CardIndex() == cardIndex)
        return device;
  return nullptr;
}

cString cSatipDevice::GetSatipStatus(void)
{
  cString info = "";
  for (int i = 0; i < cDevice::NumDevices(); i++) {
      const cDevice *device = cDevice::GetDevice(i);
      if (device && strstr(device->DeviceType(), "SAT>IP")) {
         int timers = 0;
         bool live = (device == cDevice::ActualDevice());
         bool lock = device->HasLock();
         const cChannel *channel = device->GetCurrentlyTunedTransponder();
         LOCK_TIMERS_READ;
         for (const cTimer *timer = Timers->First(); timer; timer = Timers->Next(timer)) {
             if (timer->Recording()) {
                cRecordControl *control = cRecordControls::GetRecordControl(timer);
                if (control && control->Device() == device)
                   timers++;
                }
            }
         info = cString::sprintf("%sDevice: %s\n", *info, *device->DeviceName());
         if (lock)
            info = cString::sprintf("%sCardIndex: %d  HasLock: yes  Strength: %d  Quality: %d%s\n", *info, device->CardIndex(), device->SignalStrength(), device->SignalQuality(), live ? "  Live: yes" : "");
         else
            info = cString::sprintf("%sCardIndex: %d  HasLock: no\n", *info, device->CardIndex());
         if (channel) {
            if (channel->Number() > 0 && device->Receiving())
               info = cString::sprintf("%sTransponder: %d  Channel: %s\n", *info, channel->Transponder(), channel->Name());
            else
               info = cString::sprintf("%sTransponder: %d\n", *info, channel->Transponder());
            }
         if (timers)
            info = cString::sprintf("%sRecording: %d timer%s\n", *info, timers, (timers > 1) ? "s" : "");
         info = cString::sprintf("%s\n", *info);
         }
      }
  return isempty(*info) ? cString(tr("SAT>IP information not available!")) : info;
}

cString cSatipDevice::GetGeneralInformation(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  LOCK_CHANNELS_READ;
  return cString::sprintf("SAT>IP device: %d\nCardIndex: %d\nStream: %s\nSignal: %s\nStream bitrate: %s\n%sChannel: %s\n",
                          deviceIndex, CardIndex(),
                          tuner ? *tuner->GetInformation() : "",
                          tuner ? *tuner->GetSignalStatus() : "",
                          tuner ? *tuner->GetTunerStatistic() : "",
                          *GetBufferStatistic(),
                          *Channels->GetByNumber(cDevice::CurrentChannel())->ToText());
}

cString cSatipDevice::GetPidsInformation(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  return GetPidStatistic();
}

cString cSatipDevice::GetFiltersInformation(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  return cString::sprintf("Active section filters:\n%s", SectionFilterHandler ? *SectionFilterHandler->GetInformation() : "");
}

cString cSatipDevice::GetInformation(unsigned int pageP)
{
  // generate information string
  cString s;
  switch (pageP) {
    case SATIP_DEVICE_INFO_GENERAL:
         s = GetGeneralInformation();
         break;
    case SATIP_DEVICE_INFO_PIDS:
         s = GetPidsInformation();
         break;
    case SATIP_DEVICE_INFO_FILTERS:
         s = GetFiltersInformation();
         break;
    case SATIP_DEVICE_INFO_PROTOCOL:
         s = tuner ? *tuner->GetInformation() : "";
         break;
    case SATIP_DEVICE_INFO_BITRATE:
         s = tuner ? *tuner->GetTunerStatistic() : "";
         break;
    default:
         s = cString::sprintf("%s%s%s",
                              *GetGeneralInformation(),
                              *GetPidsInformation(),
                              *GetFiltersInformation());
         break;
    }
  return s;
}

bool cSatipDevice::Ready(void) {
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  return (cSatipDiscover::GetInstance()->GetServerCount() > 0) or (ReadyTimeout.Elapsed() > eReadyTimeoutMs);
}

cString cSatipDevice::DeviceType(void) const {
  return "SAT>IP";
}

cString cSatipDevice::DeviceName(void) const {
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  std::string result(*DeviceType());

  result += " " + std::to_string(deviceIndex) + " (";

  const char* s = "ACST";
  for(int i=0; s[i]; i++)
     if (ProvidesSource(s[i] << 24))
        result += s[i];

  result += ")";

  if (not serverString.empty())
     result += " " + serverString;

  return result.c_str();
}

bool cSatipDevice::AvoidRecording(void) const
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  return SatipConfig.IsOperatingModeLow();
}

bool cSatipDevice::SignalStats(int &Valid, double *Strength, double *Cnr, double *BerPre, double *BerPost, double *Per, int *Status) const
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  Valid = DTV_STAT_VALID_NONE;
  if (Strength && tuner && tuner->HasLock()) {
     *Strength =  tuner->SignalStrengthDBm();
     if (*Strength < -18.0) /* valid: -71.458 .. -18.541, invalid: 0.0 */
        Valid |= DTV_STAT_VALID_STRENGTH;
     }
  if (Status) {
     *Status = HasLock() ? (DTV_STAT_HAS_SIGNAL | DTV_STAT_HAS_CARRIER | DTV_STAT_HAS_VITERBI | DTV_STAT_HAS_SYNC | DTV_STAT_HAS_LOCK) : DTV_STAT_HAS_NONE;
     Valid |= DTV_STAT_VALID_STATUS;
     }
  return Valid != DTV_STAT_VALID_NONE;
}

int cSatipDevice::SignalStrength(void) const
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  return (tuner ? tuner->SignalStrength() : -1);
}

int cSatipDevice::SignalQuality(void) const
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  return (tuner ? tuner->SignalQuality() : -1);
}

bool cSatipDevice::ProvidesSource(int sourceP) const
{
  cSource *s = Sources.Get(sourceP);
  dbg_chan_switch("%s (%c) desc='%s' [device %d]", __PRETTY_FUNCTION__, cSource::ToChar(sourceP), s ? s->Description() : "", deviceIndex);
  if (SatipConfig.GetDetachedMode())
     return false;
  // source descriptions starting with '0' are disabled
  if (s && s->Description() && (*(s->Description()) == '0'))
     return false;
  if (!SatipConfig.IsOperatingModeOff() && !!cSatipDiscover::GetInstance()->GetServer(sourceP)) {
     int numDisabledSourcesM = SatipConfig.GetDisabledSourcesCount();
     for (int i = 0; i < numDisabledSourcesM; ++i) {
         if (sourceP == SatipConfig.GetDisabledSources(i))
            return false;
         }
     return true;
     }
  return false;
}

bool cSatipDevice::ProvidesTransponder(const cChannel *channelP) const
{
  dbg_chan_switch("%s (%d) transponder=%d source=%c [device %d]", __PRETTY_FUNCTION__, channelP ? channelP->Number() : -1, channelP ? channelP->Transponder() : -1, channelP ? cSource::ToChar(channelP->Source()) : '?', deviceIndex);
  if (!ProvidesSource(channelP->Source()))
     return false;
  return DeviceHooksProvidesTransponder(channelP);
}

bool cSatipDevice::ProvidesChannel(const cChannel *channelP, int priorityP, bool *needsDetachReceiversP) const
{
  bool result = false;
  bool hasPriority = (priorityP == IDLEPRIORITY) || (priorityP > this->Priority());
  bool needsDetachReceivers = false;

  dbg_chan_switch("%s (%d, %d, %d) [device %d]", __PRETTY_FUNCTION__, channelP ? channelP->Number() : -1, priorityP, !!needsDetachReceiversP, deviceIndex);

  if (channelP && ProvidesTransponder(channelP)) {
     result = hasPriority;
     if (priorityP > IDLEPRIORITY) {
        if (Receiving()) {
           if (IsTunedToTransponder(channelP)) {
              if (channelP->Vpid() && !HasPid(channelP->Vpid()) || channelP->Apid(0) && !HasPid(channelP->Apid(0)) || channelP->Dpid(0) && !HasPid(channelP->Dpid(0))) {
                 if (CamSlot() && channelP->Ca() >= CA_ENCRYPTED_MIN) {
                    if (CamSlot()->CanDecrypt(channelP))
                       result = true;
                    else
                       needsDetachReceivers = true;
                    }
                 else
                    result = true;
                 }
              else
                 result = !!SatipConfig.GetFrontendReuse();
              }
           else
              needsDetachReceivers = true;
           }
        }
     }
  if (needsDetachReceiversP)
     *needsDetachReceiversP = needsDetachReceivers;
  return result;
}

bool cSatipDevice::ProvidesEIT(void) const
{
  return (SatipConfig.GetEITScan()) && DeviceHooksProvidesEIT();
}

int cSatipDevice::NumProvidedSystems(void) const
{
  int count = cSatipDiscover::GetInstance()->NumProvidedSystems();
  // Tweak the count according to operation mode
  if (SatipConfig.IsOperatingModeLow())
     count = 15;
  else if (SatipConfig.IsOperatingModeHigh())
     count = 1;
  // Clamp the count between 1 and 15
  if (count > 15)
     count = 15;
  else if (count < 1)
     count = 1;
  return count;
}

const cChannel *cSatipDevice::GetCurrentlyTunedTransponder(void) const
{
  return &currentChannel;
}

bool cSatipDevice::IsTunedToTransponder(const cChannel *channelP) const
{
  if (tuner && !tuner->IsTuned())
     return false;
  if ((currentChannel.Source() != channelP->Source()) || (currentChannel.Transponder() != channelP->Transponder()))
     return false;
  return (strcmp(currentChannel.Parameters(), channelP->Parameters()) == 0);
}

//bool cSatipDevice::MaySwitchTransponder(const cChannel *channelP) const
//{
//  return cDevice::MaySwitchTransponder(channelP);
//}

void cSatipDevice::SetPowerSaveMode(bool On) {
  cMutexLock MutexLock(&SetChannelMtx);
  if (On) {
     if (tuner and tuner->IsTuned()) {
        dbg_chan_switch("%s closing device %d",  __PRETTY_FUNCTION__, deviceIndex);
        tuner->SetPowerSaveMode(On);
        currentChannel = cChannel();
        serverString.clear();
        tsBuffer->Clear();
        }
     }
}

bool cSatipDevice::SetChannelDevice(const cChannel* channel, bool liveView)
{
  cMutexLock MutexLock(&SetChannelMtx);  // Global lock to prevent any simultaneous zapping
  dbg_chan_switch("%s (%d, %d) [device %d]",
      __PRETTY_FUNCTION__, channel ? channel->Number() : -1, liveView, deviceIndex);

  if (tuner == nullptr) {
     dbg_chan_switch("%s [device %d] -> false (no tuner)", __PRETTY_FUNCTION__, deviceIndex);
     return false;
     }

  if (channel) {
     std::string params = GetTransponderUrlParameters(channel);
     if (params.empty()) {
        error("Unrecognized channel parameters: %s [device %d]", channel->Parameters(), deviceIndex);
        return false;
        }

     auto discover = cSatipDiscover::GetInstance();
     auto server = discover->AssignServer(deviceIndex,
                                          channel->Source(),
                                          channel->Transponder(),
                                          cDvbTransponderParameters(channel->Parameters()).System());

     if (!server) {
        dbg_chan_switch("%s No server for %s [device %d]",
            __PRETTY_FUNCTION__, *channel->ToText(), deviceIndex);
        return false;
        }

     serverString = *discover->GetServerString(server);

     if (tuner->SetSource(server, channel->Transponder(), params.c_str(), deviceIndex)) {
        currentChannel = *channel;
        // Wait for actual channel tuning to prevent simultaneous frontend allocation failures
        tunerLocked.TimedWait(SetChannelMtx, eTuningTimeoutMs);
        return true;
        }
     }
  else {
     tuner->SetSource(nullptr, 0, nullptr, deviceIndex);
     serverString.clear();
     }
  return true;
}

void cSatipDevice::SetChannelTuned(void)
{
  dbg_chan_switch("%s () [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  // Release immediately any pending conditional wait
  tunerLocked.Broadcast();
}

bool cSatipDevice::SetPid(cPidHandle *handleP, int typeP, bool onP)
{
  dbg_pids("%s (%d, %d, %d) [device %d]", __PRETTY_FUNCTION__, handleP ? handleP->pid : -1, typeP, onP, deviceIndex);
  if (tuner && handleP && handleP->pid >= 0 && handleP->pid <= 8191) {
     if (onP)
        return tuner->SetPid(handleP->pid, typeP, true);
     else if (!handleP->used && SectionFilterHandler && !SectionFilterHandler->Exists(handleP->pid))
        return tuner->SetPid(handleP->pid, typeP, false);
     }
  return true;
}

int cSatipDevice::OpenFilter(unsigned short pidP, unsigned char tidP, unsigned char maskP)
{
  dbg_pids("%s (%d, %02X, %02X) [device %d]", __PRETTY_FUNCTION__, pidP, tidP, maskP, deviceIndex);
  if (SectionFilterHandler) {
     int handle = SectionFilterHandler->Open(pidP, tidP, maskP);
     if (tuner && (handle >= 0))
        tuner->SetPid(pidP, ptOther, true);
     return handle;
     }
  return -1;
}

void cSatipDevice::CloseFilter(int handleP)
{
  if (SectionFilterHandler) {
     int pid = SectionFilterHandler->GetPid(handleP);
     dbg_pids("%s (%d) [device %d]", __PRETTY_FUNCTION__, pid, deviceIndex);
     if (tuner)
        tuner->SetPid(pid, ptOther, false);
     SectionFilterHandler->Close(handleP);
     }
}

bool cSatipDevice::OpenDvr(void) {
  dbg_chan_switch("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  bytesDelivered = 0;
  if (tuner && tsBuffer) {
     tsBuffer->Clear();
     tuner->Open();
     dvrIsOpen = true;
     }
  return dvrIsOpen;
}

void cSatipDevice::CloseDvr(void) {
  dbg_chan_switch("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  if (dvrIsOpen)
     tuner->Close();
  dvrIsOpen = false;
}

bool cSatipDevice::HasLock(int timeout) const {
  dbg_funcname_ext("%s (%d) [device %d]", __PRETTY_FUNCTION__, timeout, deviceIndex);
  static constexpr int interval = 100;

  while(timeout > 0) {
     if (not tuner)
        return false;
     if (tuner->HasLock())
        return true;
     cCondWait::SleepMs(interval);
     timeout -= interval;
     }
  return tuner && tuner->HasLock();
}

bool cSatipDevice::HasInternalCam(void)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  return SatipConfig.GetCIExtension();
}

void cSatipDevice::WriteData(unsigned char* bufferP, int lengthP)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  // Fill up TS buffer
  if (dvrIsOpen) {
     int len = tsBuffer->Put(bufferP, lengthP);
     if (len != lengthP)
        tsBuffer->ReportOverflow(lengthP - len);
     }
  // Filter the sections
  if (SectionFilterHandler)
     SectionFilterHandler->Write(bufferP, lengthP);
}

int cSatipDevice::GetId(void)
{
  return deviceIndex;
}

int cSatipDevice::GetPmtPid(void)
{
  int pid = currentChannel.Ca() ? ::GetPmtPid(currentChannel.Source(), currentChannel.Transponder(), currentChannel.Sid()) : 0;
  dbg_ci("%s pmtpid=%d source=%c transponder=%d sid=%d name=%s [device %d]", __PRETTY_FUNCTION__, pid, cSource::ToChar(currentChannel.Source()), currentChannel.Transponder(), currentChannel.Sid(), currentChannel.Name(), deviceIndex);
  return pid;
}

int cSatipDevice::GetCISlot(void)
{
  int slot = 0;
  int ca = 0;
  for (const int *id = currentChannel.Caids(); *id; ++id) {
      if (checkCASystem(SatipConfig.GetCICAM(0), *id)) {
         ca = *id;
         slot = 1;
         break;
         }
      else if (checkCASystem(SatipConfig.GetCICAM(1), *id)) {
         ca = *id;
         slot = 2;
         break;
         }
      }
  dbg_ci("%s slot=%d ca=%X name=%s [device %d]", __PRETTY_FUNCTION__, slot, ca, currentChannel.Name(), deviceIndex);
  return slot;
}

cString cSatipDevice::GetTnrParameterString(void)
{
   if (currentChannel.Ca())
      return GetTnrUrlParameters(&currentChannel).c_str();
   return NULL;
}

bool cSatipDevice::IsIdle(void)
{
  return !Receiving();
}

unsigned char* cSatipDevice::GetData(int *availableP, bool checkTsBuffer)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  if (dvrIsOpen) {
     int count = 0;
     if (bytesDelivered) {
        tsBuffer->Del(bytesDelivered);
        bytesDelivered = 0;
        }
     if (checkTsBuffer && tsBuffer->Available() < TS_SIZE)
        return NULL;
     auto p = tsBuffer->Get(count);
     if (p && count >= TS_SIZE) {
        if (*p != TS_SYNC_BYTE) {
           for (int i = 1; i < count; i++) {
               if (p[i] == TS_SYNC_BYTE) {
                  count = i;
                  break;
                  }
               }
           tsBuffer->Del(count);
           info("Skipped %d bytes to sync on TS packet", count);
           return NULL;
           }
        bytesDelivered = TS_SIZE;
        if (availableP)
           *availableP = count;
        // Update pid statistics
        AddPidStatistic(ts_pid(p), payload(p));
        return p;
        }
     }
  return NULL;
}

void cSatipDevice::SkipData(int countP)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  bytesDelivered = countP;
  // Update buffer statistics
  AddBufferStatistic(countP, tsBuffer->Available());
}

bool cSatipDevice::GetTSPacket(unsigned char*& dataP)
{
  dbg_funcname_ext("%s [device %d]", __PRETTY_FUNCTION__, deviceIndex);
  if (SatipConfig.GetDetachedMode())
     return false;
  if (tsBuffer) {
     if (cCamSlot *cs = CamSlot()) {
        if (cs->WantsTsData()) {
           int available;
           dataP = GetData(&available, checkTsBufferM);
           if (!dataP)
              available = 0;
           dataP = cs->Decrypt(dataP, available);
           SkipData(available);
           checkTsBufferM = dataP != NULL;
           return true;
           }
        }
     dataP = GetData();
     return true;
     }
  dataP = NULL;
  return true;
}
