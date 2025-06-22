/*
 * log.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#pragma once

#include "config.h"

#define _E_(x...) { if (SatipConfig.IsDebugMode(cSatipConfig::DbgToStdout)) { printf(x); printf("\n"); } else if (SatipConfig.IsDebugMode(cSatipConfig::DbgToStderr)) { fprintf(stderr,x); fprintf(stderr,"\n"); } else esyslog(x); }
#define _I_(x...) { if (SatipConfig.IsDebugMode(cSatipConfig::DbgToStdout)) { printf(x); printf("\n"); } else if (SatipConfig.IsDebugMode(cSatipConfig::DbgToStderr)) { fprintf(stderr,x); fprintf(stderr,"\n"); } else isyslog(x); }
#define _D_(x...) { if (SatipConfig.IsDebugMode(cSatipConfig::DbgToStdout)) { printf(x); printf("\n"); } else if (SatipConfig.IsDebugMode(cSatipConfig::DbgToStderr)) { fprintf(stderr,x); fprintf(stderr,"\n"); } else dsyslog(x); }

#define error(x...)   _E_("SATIP-ERROR: " x)
#define info(x...)    _I_("SATIP: " x)

#define dbg_funcname(x...)       if (SatipConfig.IsDebugMode(cSatipConfig::DbgCallStack))        _D_("SATIP: calling " x);
#define dbg_curlinfo(x...)       if (SatipConfig.IsDebugMode(cSatipConfig::DbgCurlDataFlow))     _D_("SATIP: CURLINFO: " x);
#define dbg_parsing(x...)        if (SatipConfig.IsDebugMode(cSatipConfig::DbgDataParsing))      _D_("SATIP: parsing: " x);
#define dbg_tunerstate(x...)     if (SatipConfig.IsDebugMode(cSatipConfig::DbgTunerState))       _D_("SATIP: tunerstate " x);
#define dbg_rtsp(x...)           if (SatipConfig.IsDebugMode(cSatipConfig::DbgRtspResponse))     _D_("SATIP: RTSP " x);
#define dbg_rtp_perf(x...)       if (SatipConfig.IsDebugMode(cSatipConfig::DbgRtpPerformance))   _D_("SATIP: RTP performance " x);
#define dbg_rtp_packet(x...)     if (SatipConfig.IsDebugMode(cSatipConfig::DbgRtpPacket))        _D_("SATIP: RTP " x);
#define dbg_sectionfilter(x...)  if (SatipConfig.IsDebugMode(cSatipConfig::DbgSectionFiltering)) _D_("SATIP: sectionfilter " x);
#define dbg_chan_switch(x...)    if (SatipConfig.IsDebugMode(cSatipConfig::DbgChannelSwitching)) _D_("SATIP: channel " x);
#define dbg_pwr_save(x...)       if (SatipConfig.IsDebugMode(cSatipConfig::DbgPowerSave))        _D_("SATIP: power save " x);
#define dbg_rtcp(x...)           if (SatipConfig.IsDebugMode(cSatipConfig::DbgRtcp))             _D_("SATIP: RTCP " x);
#define dbg_ci(x...)             if (SatipConfig.IsDebugMode(cSatipConfig::DbgCommonInterface))  _D_("SATIP: CI " x);
#define dbg_pids(x...)           if (SatipConfig.IsDebugMode(cSatipConfig::DbgPids))             _D_("SATIP: PIDS " x);
#define dbg_msearch(x...)        if (SatipConfig.IsDebugMode(cSatipConfig::DbgDiscovery))        _D_("SATIP: MSEARCH " x);
#define dbg_reserved1(x...)      if (SatipConfig.IsDebugMode(cSatipConfig::DbgReserved1))        _D_("SATIP: dbg_reserved1 " x);
#define dbg_funcname_ext(x...)   if (SatipConfig.IsDebugMode(cSatipConfig::DbgCallStackExt))     _D_("SATIP16: calling " x);
