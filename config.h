/*
 * config.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_CONFIG_H
#define __SATIP_CONFIG_H

#include <vdr/menuitems.h>
#include "common.h"

class cSatipConfig
{
private:
  unsigned int operatingModeM;
  unsigned int debugModeM;
  unsigned int ciExtensionM;
  unsigned int frontendReuseM;
  unsigned int powersave;
  unsigned int eitScanM;
  unsigned int useBytesM;
  unsigned int portRangeStartM;
  unsigned int portRangeStopM;
  unsigned int transportModeM;
  bool detachedModeM;
  bool disableServerQuirksM;
  bool useSingleModelServersM;
  int cicamsM[MAX_CICAM_COUNT];
  int disabledSourcesM[MAX_DISABLED_SOURCES_COUNT];
  int disabledFiltersM[SECTION_FILTER_TABLE_SIZE];
  size_t rtpRcvBufSizeM;

public:
  enum eOperatingMode {
    eOperatingModeOff = 0,
    eOperatingModeLow,
    eOperatingModeNormal,
    eOperatingModeHigh,
    eOperatingModeCount
  };
  enum eTransportMode {
    eTransportModeUnicast = 0,
    eTransportModeMulticast,
    eTransportModeRtpOverTcp,
    eTransportModeCount
  };
  enum eDebugMode {
    DbgNormal            = 0,
    DbgCallStack         = (1U << 0),
    DbgCurlDataFlow      = (1U << 1),
    DbgDataParsing       = (1U << 2),
    DbgTunerState        = (1U << 3),
    DbgRtspResponse      = (1U << 4),
    DbgRtpPerformance    = (1U << 5),
    DbgRtpPacket         = (1U << 6),
    DbgSectionFiltering  = (1U << 7),
    DbgChannelSwitching  = (1U << 8),
    DbgRtcp              = (1U << 9),
    DbgCommonInterface   = (1U << 10),
    DbgPids              = (1U << 11),
    DbgDiscovery         = (1U << 12),
    DbgReserved1         = (1U << 13),
    DbgToStdout          = (1U << 14),
    DbgCallStackExt      = (1U << 15),
    DbgToStderr          = (1U << 16),
    DbgModeMask          = 0x1FFFF
  };
  cSatipConfig();
  unsigned int GetOperatingMode(void) const { return operatingModeM; }
  bool IsOperatingModeOff(void) const { return (operatingModeM == eOperatingModeOff); }
  bool IsOperatingModeLow(void) const { return (operatingModeM == eOperatingModeLow); }
  bool IsOperatingModeNormal(void) const { return (operatingModeM == eOperatingModeNormal); }
  bool IsOperatingModeHigh(void) const { return (operatingModeM == eOperatingModeHigh); }
  void ToggleOperatingMode(void) { operatingModeM = (operatingModeM + 1) % eOperatingModeCount; }
  unsigned int GetDebugMode(void) const { return debugModeM; }
  bool IsDebugMode(eDebugMode modeP) const { return (debugModeM & modeP); }
  unsigned int GetCIExtension(void) const { return ciExtensionM; }
  unsigned int GetFrontendReuse(void) const { return frontendReuseM; }
  unsigned int GetPowerSave(void) const { return powersave; }
  int GetCICAM(unsigned int indexP) const;
  unsigned int GetEITScan(void) const { return eitScanM; }
  unsigned int GetUseBytes(void) const { return useBytesM; }
  unsigned int GetTransportMode(void) const { return transportModeM; }
  bool IsTransportModeUnicast(void) const { return (transportModeM == eTransportModeUnicast); }
  bool IsTransportModeRtpOverTcp(void) const { return (transportModeM == eTransportModeRtpOverTcp); }
  bool IsTransportModeMulticast(void) const { return (transportModeM == eTransportModeMulticast); }
  bool GetDetachedMode(void) const { return detachedModeM; }
  bool GetDisableServerQuirks(void) const { return disableServerQuirksM; }
  bool GetUseSingleModelServers(void) const { return useSingleModelServersM; }
  unsigned int GetDisabledSourcesCount(void) const;
  int GetDisabledSources(unsigned int indexP) const;
  unsigned int GetDisabledFiltersCount(void) const;
  int GetDisabledFilters(unsigned int indexP) const;
  unsigned int GetPortRangeStart(void) const { return portRangeStartM; }
  unsigned int GetPortRangeStop(void) const { return portRangeStopM; }
  size_t GetRtpRcvBufSize(void) const { return rtpRcvBufSizeM; }

  void SetOperatingMode(unsigned int operatingModeP) { operatingModeM = operatingModeP; }
  void SetDebugMode(unsigned int modeP) { debugModeM = (modeP & DbgModeMask); }
  void SetCIExtension(unsigned int onOffP) { ciExtensionM = onOffP; }
  void SetFrontendReuse(unsigned int onOffP) { frontendReuseM = onOffP; }
  void SetPowerSave(unsigned int On) { powersave = On; }
  void SetCICAM(unsigned int indexP, int cicamP);
  void SetEITScan(unsigned int onOffP) { eitScanM = onOffP; }
  void SetUseBytes(unsigned int onOffP) { useBytesM = onOffP; }
  void SetTransportMode(unsigned int transportModeP) { transportModeM = transportModeP; }
  void SetDetachedMode(bool onOffP) { detachedModeM = onOffP; }
  void SetDisableServerQuirks(bool onOffP) { disableServerQuirksM = onOffP; }
  void SetUseSingleModelServers(bool onOffP) { useSingleModelServersM = onOffP; }
  void SetDisabledSources(unsigned int indexP, int sourceP);
  void SetDisabledFilters(unsigned int indexP, int numberP);
  void SetPortRangeStart(unsigned int rangeStartP) { portRangeStartM = rangeStartP; }
  void SetPortRangeStop(unsigned int rangeStopP) { portRangeStopM = rangeStopP; }
  void SetRtpRcvBufSize(size_t sizeP) { rtpRcvBufSizeM = sizeP; }
};

extern cSatipConfig SatipConfig;

#endif // __SATIP_CONFIG_H
