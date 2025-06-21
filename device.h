/*
 * device.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_DEVICE_H
#define __SATIP_DEVICE_H

#include <string>
#include <vdr/device.h>
#include "common.h"
#include "deviceif.h"
#include "tuner.h"
#include "sectionfilter.h"
#include "statistics.h"

class cSatipDevice : public cDevice, public cSatipPidStatistics, public cSatipBufferStatistics, public cSatipDeviceIf {
friend class cSatipTuner;
  // static ones
public:
  static bool Initialize(int DeviceCount);
  static void Shutdown(void);
  static size_t Count(void);
  static cSatipDevice* GetSatipDevice(int CardIndex);
  static cString GetSatipStatus(void);

  // private parts
private:
  enum {
    eReadyTimeoutMs  = 2000, // in milliseconds
    eTuningTimeoutMs = 1000  // in milliseconds
  };
  int deviceIndex;
  int bytesDelivered;
  bool dvrIsOpen;
  bool checkTsBufferM;
  std::string serverString;
  cChannel currentChannel;
  cRingBufferLinear *tsBuffer;
  cSatipTuner* tuner;
  cSatipSectionFilterHandler* SectionFilterHandler;
  cTimeMs ReadyTimeout;
  cCondVar tunerLocked;

  // constructor & destructor
public:
  explicit cSatipDevice(unsigned int DeviceIndex);
  virtual ~cSatipDevice();
  cString GetInformation(unsigned int pageP = SATIP_DEVICE_INFO_ALL);

  // copy and assignment constructors
private:
  static cMutex SetChannelMtx;
  cSatipDevice(const cSatipDevice&);
  cSatipDevice& operator=(const cSatipDevice&);

  // for statistics and general information
  cString GetGeneralInformation(void);
  cString GetPidsInformation(void);
  cString GetFiltersInformation(void);

  // for channel info
public:
  virtual bool Ready(void);
  virtual cString DeviceType(void) const;
  virtual cString DeviceName(void) const;
  virtual bool AvoidRecording(void) const;
  virtual bool SignalStats(int &Valid, double *Strength = NULL, double *Cnr = NULL, double *BerPre = NULL, double *BerPost = NULL, double *Per = NULL, int *Status = NULL) const;
  virtual int SignalStrength(void) const;
  virtual int SignalQuality(void) const;

  // for channel selection
public:
  virtual bool ProvidesSource(int sourceP) const;
  virtual bool ProvidesTransponder(const cChannel *channelP) const;
  virtual bool ProvidesChannel(const cChannel *channelP, int priorityP = -1, bool *needsDetachReceiversP = NULL) const;
  virtual bool ProvidesEIT(void) const;
  virtual int NumProvidedSystems(void) const;
  virtual const cChannel *GetCurrentlyTunedTransponder(void) const;
  virtual bool IsTunedToTransponder(const cChannel *Channel) const;
//virtual bool MaySwitchTransponder(const cChannel *Channel) const;
  virtual void SetPowerSaveMode(bool On);


protected:
  virtual bool SetChannelDevice(const cChannel* channel, bool liveView);

  // for recording
private:
  unsigned char* GetData(int *availableP = NULL, bool checkTsBuffer = false);
  void SkipData(int countP);

protected:
  virtual bool SetPid(cPidHandle *handleP, int typeP, bool onP);
  virtual bool OpenDvr(void);
  virtual void CloseDvr(void);
  virtual bool GetTSPacket(unsigned char*& dataP);

  // for section filtering
public:
  virtual int OpenFilter(unsigned short pidP, unsigned char tidP, unsigned char maskP);
  virtual void CloseFilter(int handleP);

  // for transponder lock
public:
  virtual bool HasLock(int timeout = 0) const;

  // for common interface
public:
  virtual bool HasInternalCam(void);

  // for internal device interface
public:
  virtual void WriteData(unsigned char* bufferP, int lengthP);
  virtual void SetChannelTuned(void);
  virtual int GetId(void);
  virtual int GetPmtPid(void);
  virtual int GetCISlot(void);
  virtual cString GetTnrParameterString(void);
  virtual bool IsIdle(void);
};

#endif // __SATIP_DEVICE_H
