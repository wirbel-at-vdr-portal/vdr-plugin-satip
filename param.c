/*
 * param.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <sstream>   // std::stringstream
#include <ctype.h>
#include <vdr/dvbdevice.h>
#include <repfunc.h>
#include "common.h"
#include "param.h"

// --- cSatipParameterMaps ----------------------------------------------------

struct tSatipParameterMap {
  int driverValue;
  const char* satipString;
  int vdrValue;
};

static const tSatipParameterMap SatipBandwidthValues[] = {
  { 5000000 , "&bw=5"    , 5    },
  { 6000000 , "&bw=6"    , 6    },
  { 7000000 , "&bw=7"    , 7    },
  { 8000000 , "&bw=8"    , 8    },
  { 10000000, "&bw=10"   , 10   },
  { 1712000 , "&bw=1.712", 1712 },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipPilotValues[] = {
  { PILOT_OFF , "&plts=off", 0   },
  { PILOT_ON  , "&plts=on" , 1   },
  { PILOT_AUTO, ""         , 999 },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipSisoMisoValues[] = {
  { 0 , "&sm=0", 0 },
  { 1 , "&sm=1", 1 },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipCodeRateValues[] = {
  { FEC_NONE, ""        , 0   },
  { FEC_1_2 , "&fec=12" , 12  },
  { FEC_2_3 , "&fec=23" , 23  },
  { FEC_3_4 , "&fec=34" , 34  },
  { FEC_3_5 , "&fec=35" , 35  },
  { FEC_4_5 , "&fec=45" , 45  },
  { FEC_5_6 , "&fec=56" , 56  },
  { FEC_6_7 , "&fec=67" , 67  },
  { FEC_7_8 , "&fec=78" , 78  },
  { FEC_8_9 , "&fec=89" , 89  },
  { FEC_9_10, "&fec=910", 910 },
  { FEC_AUTO, ""        , 999 },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipModulationValues[] = {
  { QPSK    , "&mtype=qpsk"  , 2   },
  { PSK_8   , "&mtype=8psk"  , 5   },
  { APSK_16 , "&mtype=16apsk", 6   },
  { APSK_32 , "&mtype=32apsk", 7   },
  { VSB_8   , "&mtype=8vsb"  , 10  },
  { VSB_16  , "&mtype=16vsb" , 11  },
  { QAM_16  , "&mtype=16qam" , 16  },
  { QAM_64  , "&mtype=64qam" , 64  },
  { QAM_128 , "&mtype=128qam", 128 },
  { QAM_256 , "&mtype=256qam", 256 },
  { QAM_AUTO, ""             , 999 },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipSystemValuesSat[] = {
  { 0, "&msys=dvbs" , 0 },
  { 1, "&msys=dvbs2", 1 },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipSystemValuesTerrestrial[] = {
  { 0, "&msys=dvbt" , 0 },
  { 1, "&msys=dvbt2", 1 },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipSystemValuesCable[] = {
  { 0, "&msys=dvbc" , 0 },
  { 1, "&msys=dvbc2", 1 },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipSystemValuesAtsc[] = {
  { 0, "&msys=atsc", 0 },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipTransmissionValues[] = {
  { TRANSMISSION_MODE_1K  , "&tmode=1k" , 1   },
  { TRANSMISSION_MODE_2K  , "&tmode=2k" , 2   },
  { TRANSMISSION_MODE_4K  , "&tmode=4k" , 4   },
  { TRANSMISSION_MODE_8K  , "&tmode=8k" , 8   },
  { TRANSMISSION_MODE_16K , "&tmode=16k", 16  },
  { TRANSMISSION_MODE_32K , "&tmode=32k", 32  },
  { TRANSMISSION_MODE_AUTO, ""          , 999 },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipGuardValues[] = {
  { GUARD_INTERVAL_1_4   , "&gi=14"   , 4     },
  { GUARD_INTERVAL_1_8   , "&gi=18"   , 8     },
  { GUARD_INTERVAL_1_16  , "&gi=116"  , 16    },
  { GUARD_INTERVAL_1_32  , "&gi=132"  , 32    },
  { GUARD_INTERVAL_1_128 , "&gi=1128" , 128   },
  { GUARD_INTERVAL_19_128, "&gi=19128", 19128 },
  { GUARD_INTERVAL_19_256, "&gi=19256", 19256 },
  { GUARD_INTERVAL_AUTO  , ""         , 999   },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipRollOffValues[] = {
  { ROLLOFF_AUTO, ""        , 0   },
  { ROLLOFF_20  , "&ro=0.20", 20  },
  { ROLLOFF_25  , "&ro=0.25", 25  },
  { ROLLOFF_35  , "&ro=0.35", 35  },
  { -1, nullptr, -1 }};

static const tSatipParameterMap SatipInversionValues[] = {
  { INVERSION_AUTO, ""          , 999 },
  { INVERSION_OFF , "&specinv=0", 0   },
  { INVERSION_ON  , "&specinv=1", 1   },
  { -1, nullptr, -1 }};

int SatipToVdrParameter(std::string param) {
  const tSatipParameterMap* map = nullptr;

  if (param.find("&bw=") == 0)
     map = SatipBandwidthValues;
  else if (param.find("&plts=") == 0)
     map = SatipPilotValues;
  else if (param.find("&sm=") == 0)
     map = SatipSisoMisoValues;
  else if (param.find("&fec=") == 0)
     map = SatipCodeRateValues;
  else if (param.find("&mtype=") == 0)
     map = SatipModulationValues;
  else if (param.find("&msys=dvbs") == 0)
     map = SatipSystemValuesSat;
  else if (param.find("&msys=dvbt") == 0)
     map = SatipSystemValuesTerrestrial;
  else if (param.find("&msys=dvbc") == 0)
     map = SatipSystemValuesCable;
  else if (param.find("&msys=atsc") == 0)
     map = SatipSystemValuesAtsc;
  else if (param.find("&tmode=") == 0)
     map = SatipTransmissionValues;
  else if (param.find("&gi=") == 0)
     map = SatipGuardValues;
  else if (param.find("&ro=") == 0)
     map = SatipRollOffValues;
  else if (param.find("&specinv=") == 0)
     map = SatipInversionValues;

  auto it = map;
  while(it && it->satipString) {
     if (it->satipString == param)
        return it->vdrValue;
     it++;
     }
  return 999;
}

std::string GetTransponderUrlParameters(const cChannel* channel) {
  std::string result;

  if (channel) {
     auto PrintUrlString = [](int value, const tSatipParameterMap* map) -> const char* {
        auto it = map;
        while(it && it->satipString) {
           if (it->driverValue == value)
              return it->satipString;
           it++;
           }
        return "";
        };

     auto PrintFloat = [](float& f) -> std::string {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.3f", f);
        std::string s((const char *) buf);
        ReplaceAll(s, ",", ".");
        return s;
        };

     auto check = [](std::string s, char Type, int delsys) {
        char Delivery[3] = { (char) ('1' + delsys), '*', 0 };
        return ((s.find(Type) != std::string::npos) and
                (s.find_first_of(Delivery) != std::string::npos));
        };

     std::stringstream ss;
     cDvbTransponderParameters dtp(channel->Parameters());
     int DataSlice = 0;
     int C2TuningFrequencyType = 0;
     char type = cSource::ToChar(channel->Source());
     cSource* source = Sources.Get(channel->Source());
     int src = (strchr("S", type) && source) ? atoi(source->Description()) : 1;
     int fe = std::max(channel->Rid() % 100, 0);
     float freq = channel->Frequency();
     while(freq > 20000.0f) // MHz
        freq /= 1000.0f;



     #define ST(s) if (check(s, type, dtp.System()))

     ST(" S 1") {
        /* comply with
         * SAT>IP Protocol Specification, Version 1.2.2 (08.01.2015)
         * p.43, '3.5.11 Query Syntax'
         */
        dtp.SetPilot(PILOT_OFF);
        dtp.SetModulation(QPSK);
        dtp.SetRollOff(ROLLOFF_35);
        }

     if (fe)          ss << "&fe="    << fe;
     ST("  S *")      ss << "&src="   << ((src > 0) && (src <= 255) ? src : 1);
     if (freq > 0.0f) ss << "&freq="  << PrintFloat(freq);
     ST("  S *")      ss << "&pol="   << (char) tolower(dtp.Polarization());
     ST("  S *")      ss << PrintUrlString(dtp.RollOff(), SatipRollOffValues);
     ST(" C  2")      ss << "&c2tft=" << C2TuningFrequencyType;
     ST("   T*")      ss << PrintUrlString(dtp.Bandwidth(),    SatipBandwidthValues);
     ST(" C  2")      ss << PrintUrlString(dtp.Bandwidth(),    SatipBandwidthValues);
     ST("  S *")      ss << PrintUrlString(dtp.System(),       SatipSystemValuesSat);
     ST(" C  *")      ss << PrintUrlString(dtp.System(),       SatipSystemValuesCable);
     ST("   T*")      ss << PrintUrlString(dtp.System(),       SatipSystemValuesTerrestrial);
     ST("A   *")      ss << PrintUrlString(dtp.System(),       SatipSystemValuesAtsc);
     ST("   T*")      ss << PrintUrlString(dtp.Transmission(), SatipTransmissionValues);
     ST("  S *")      ss << PrintUrlString(dtp.Modulation(),   SatipModulationValues);
     ST("   T*")      ss << PrintUrlString(dtp.Modulation(),   SatipModulationValues);
     ST(" C  1")      ss << PrintUrlString(dtp.Modulation(),   SatipModulationValues);
     ST("A   *")      ss << PrintUrlString(dtp.Modulation(),   SatipModulationValues);
     ST("  S *")      ss << PrintUrlString(dtp.Pilot(),        SatipPilotValues);
     ST("  S *")      ss << "&sr="    << channel->Srate();
     ST(" C  1")      ss << "&sr="    << channel->Srate();
     ST("   T*")      ss << PrintUrlString(dtp.Guard(),        SatipGuardValues);
     ST(" CST*")      ss << PrintUrlString(dtp.CoderateH(),    SatipCodeRateValues);
     ST(" C  2")      ss << "&ds="    << DataSlice;
     ST(" C T2")      ss << "&plp="   << dtp.StreamId();
     ST("   T2")      ss << "&t2id="  << dtp.T2SystemId();
     ST("   T2")      ss << PrintUrlString(dtp.SisoMiso(),     SatipSisoMisoValues);
     ST(" C  1")      ss << PrintUrlString(dtp.Inversion(),    SatipInversionValues);
     ST("A   *")      ss << PrintUrlString(dtp.Inversion(),    SatipInversionValues);
     #undef ST

     result = ss.str();
     if (result.size() > 0)
        result.erase(0,1); // skip leading '&'
     }
  return result;
}

std::string GetTnrUrlParameters(const cChannel* channel) {
  if (channel) {
     std::stringstream ss;
     cDvbTransponderParameters dtp(channel->Parameters());
     eTrackType track = cDevice::PrimaryDevice()->GetCurrentAudioTrack();

     // TunerType: Byte;
     if (channel->IsCable())
        ss << "0,"; // 0 = cable
     else if (channel->IsSat())
        ss << "1,"; // 1 = satellite
     else if (channel->IsTerr())
        ss << "2,"; // 2 = terrestrial
     else if (channel->IsAtsc())
        ss << "3,"; // 3 = atsc
     else
        ss << "1,"; // we don't known here; 4 = iptv, 5 = stream (URL, DVBViewer GE)

     // Frequency: DWord;
     //   DVB-S: MHz if < 1000000, kHz if >= 1000000
     //   DVB-T/C, ATSC: kHz
     //   IPTV: IP address Byte3.Byte2.Byte1.Byte0
     ss << channel->Frequency() / 1000 << ',';

     // Symbolrate: DWord;
     //   DVB S/C: in kSym/s
     //   DVB-T, ATSC: 0
     //   IPTV: Port
     if (channel->IsSat() or channel->IsCable())
        ss << channel->Srate() << ',';
     else
        ss << "0,";

     // LNB_LOF: Word;
     //   DVB-S: Local oscillator frequency of the LNB
     //   DVB-T/C, ATSC: 0
     //   IPTV: Byte0 and Byte1 of Source IP
     if (channel->IsSat())
        ss << Setup.LnbSLOF << ',';
     else
        ss << "0,";

     // Tone: Byte; 0 = off, 1 = 22 khz
     if ((channel->IsSat()) and (channel->Frequency() >= Setup.LnbSLOF))
        ss << "1,";
     else
        ss << "0,";

     // Polarity: Byte;
     switch(channel->Source() >> 24) {
        case 'S': // DVB-S polarity: 0 = H, 1 = V, 2 = L, 3 = R
           switch(tolower(dtp.Polarization())) {
              default:
                 /* fall through */
              case 'h':
                 ss << "0,";
                 break;
              case 'v':
                 ss << "1,";
                 break;
              case 'l':
                 ss << "2,";
                 break;
              case 'r':
                 ss << "3,";
                 break;
              }
           break;
        case 'C': // DVB-C modulation: 0 = Auto, 1 = 16QAM, 2 = 32QAM, 3 = 64QAM, 4 = 128QAM, 5 = 256QAM
           switch(dtp.Modulation()) {
              default:
                 /* fall through */
              case 999:
                 ss << "0,";
                 break;
              case 16:
                 ss << "1,";
                 break;
              case 32:
                 ss << "2,";
                 break;
              case 64:
                 ss << "3,";
                 break;
              case 128:
                 ss << "4,";
                 break;
              case 256:
                 ss << "5,";
                 break;
              }
           break;
        case 'T': // DVB-T bandwidth: 0 = 6 MHz, 1 = 7 MHz, 2 = 8 MHz
           switch(dtp.Bandwidth()) {
              default:
                 /* fall through */
              case 8:
                 ss << "2,";
                 break;
              case 7:
                 ss << "1,";
                 break;
              case 6:
                 ss << "0,";
                 break;
              }
           break;
        default: // IPTV: Byte3 of SourceIP
           ss << "0,";
        }

     // DiSEqC: Byte;
     //   0 = None
     //   1 = Pos A (mostly translated to PosA/OptA)
     //   2 = Pos B (mostly translated to PosB/OptA)
     //   3 = PosA/OptA
     //   4 = PosB/OptA
     //   5 = PosA/OptB
     //   6 = PosB/OptB
     //   7 = Preset Position (DiSEqC 1.2, see DiSEqCExt)
     //   8 = Angular Position (DiSEqC 1.2, see DiSEqCExt)
     //   9 = DiSEqC Command Sequence (see DiSEqCExt)
     ss << "0,";

     // FEC: Byte;
     if (channel->IsSat()) {
        switch (dtp.CoderateH()) {
           default:
              /* fall through */
           case 999:  // 0 = Auto
              ss << "0,";
              break;
           case 12:   // 1 = 1/2
              ss << "1,";
              break;
           case 23:   // 2 = 2/3
              ss << "2,";
              break;
           case 34:   // 3 = 3/4
              ss << "3,";
              break;
           case 56:   // 4 = 5/6
              ss << "4,";
              break;
           case 78:   // 5 = 7/8
              ss << "5,";
              break;
           case 89:   // 6 = 8/9
              ss << "6,";
              break;
           case 35:   // 7 = 3/5
              ss << "7,";
              break;
           case 45:   // 8 = 4/5
              ss << "8,";
              break;
           case 910:  // 9 = 9/10
              ss << "9,";
              break;
           }
        }
     else //  IPTV: Byte2 of SourceIP, DVB-C/-T, ATSC: 0
        ss << "0,";

     // Audio_PID: Word;
     int Audio_PID = channel->Apid(0);
     if (IS_AUDIO_TRACK(track))
        Audio_PID = channel->Apid(int(track - ttAudioFirst));
     else if (IS_DOLBY_TRACK(track))
        Audio_PID = channel->Dpid(int(track - ttDolbyFirst));
     ss << Audio_PID << ',';

     // Video_PID: Word;
     ss << channel->Vpid() << ',';

     // PMT_PID: Word;
     ss << channel->Ppid() << ',';

     // Service_ID: Word;
     ss << channel->Sid() << ',';

     // SatModulation: Byte;
     int SatModulation = 0;
     if (channel->IsSat() && dtp.System()) {
        int m; // Bit 0..1: satellite modulation.
        switch(dtp.Modulation()) {
           default:
              /* fall through */
           case 999: //  0 = Auto
              m = 0;
              break;
           case 2:   //  1 = QPSK
              m = 1;
              break;
           case 5:   //  2 = 8PSK
              m = 2;
              break;
           case 6:   //  3 = 16QAM or APSK for DVB-S2 
              m = 3;
              break;
           }
        SatModulation |= (m & 0x3);
        }
     // Bit 2: modulation system. 0 = DVB-S/T/C, 1 = DVB-S2/T2/C2
     SatModulation |= (dtp.System() & 0x1) << 2;

     if (channel->IsSat() && dtp.System()) {
        int r; // Bit 3..4: DVB-S2: roll-off.
        switch(dtp.RollOff()) {
           default:
              /* fall through ; 3 = reserved */
           case 35: // 0 = 0.35
              r = 0;
              break;
           case 25: // 1 = 0.25
              r = 1;
              break;
           case 20: // 2 = 0.20
              r = 2;
              break;
           }
        SatModulation |= (r & 0x3) << 3;
        }

     { // Bit 5..6: spectral inversion.
     int i;
     switch(dtp.Inversion()) {
        default:  // 0 = undefined
           /* fall through */
        case 999: // 1 = auto
           i = 1;
           break;
        case 0:   // 2 = normal
           i = 2;
           break;
        case 1:   // 3 = inverted
           i = 3;
           break;
        }
     SatModulation |= (i & 0x3) << 5;
     }

     //   Bit 7: DVB-S2: pilot symbols, 0 = off, 1 = on
     //          DVB-T2: DVB-T2 Lite, 0 = off, 1 = on
     if (channel->IsSat() && dtp.System()) {
        int p;
        switch(dtp.Pilot()) {
           default:
              /* fall through */
           case 0: // pilot symbols, 0 = off
              p = 0;
              break;
           case 1: // pilot symbols, 1 = on
              p = 1;
              break;
           }
        SatModulation |= (p & 0x1) << 7;
        }

     ss << SatModulation << ',';

     // DiSEqCExt: Word;
     //   DiSEqC Extension, meaning depends on DiSEqC
     //   DiSEqC = 0..6: 0
     //   DiSEqC = 7: Preset Position (DiSEqC 1.2)
     //   DiSEqC = 8: Orbital Position (DiSEqC 1.2, USALS, for calculating motor angle)
     //               Same format as OrbitalPos above
     //   DiSEQC = 9: Orbital Position referencing DiSEqC sequence defined in DiSEqC.xml/ini
     //               Same format as OrbitalPos above
     ss << "0,";

     // Flags: Byte;
     //   Bit 0: 1 = encrypted channel
     //   Bit 1: reserved, set to 0
     //   Bit 2: 1 = channel broadcasts RDS data
     //   Bit 3: 1 = channel is a video service (even if the Video PID is temporarily = 0)
     //   Bit 4: 1 = channel is an audio service (even if the Audio PID is temporarily = 0)
     //   Bit 5: 1 = audio has a different samplerate than 48 KHz
     //   Bit 6: 1 = bandstacking, internally polarisation is always set to H
     //   Bit 7: 1 = channel entry is an additional audio track of the preceding
     //              channel with bit 7 = 0
     if (channel->Ca() > 0xFF)
        ss << "1,";
     else
        ss << "0,";

     // ChannelGroup: Byte;
     //   0 = Group A, 1 = Group B, 2 = Group C etc.
     ss << "0,";

     // TransportStream_ID: Word;
     ss << channel->Tid() << ',';

     // OriginalNetwork_ID: Word;
     ss << channel->Nid() << ',';

     // Substream: Word;
     //   DVB-S/C/T, ATSC, IPTV: 0
     //   DVB-T2: 0 = PLP_ID not set, 1..256: PLP_ID + 1, 257... reserved
     if (channel->IsTerr() && dtp.System())
        ss << (dtp.StreamId() - 1) << ',';
     else
        ss << "0,";

     // OrbitalPos: Word;
     //   DVB-S: orbital position x 10, 0 = undefined, 1..1800 east, 1801..3599 west (1°W = 3599)
     //   DVB-C: 4000..4999
     //   DVB-T: 5000..5999
     //   ATSC:  6000..6999
     //   IPTV:  7000..7999
     //   Stream: 8000..8999
     if (channel->IsSat()) {
        int pos = cSource::Position(channel->Source());
        if (pos != 3600)
           pos += 1800;
        ss << pos << ',';
        }
     else
        ss << "0,";

     return ss.str();
     }
  return "";
}

int SrcIdToSource(int pos) {
  for(cSource* s = Sources.First(); s; s = Sources.Next(s)) {
     if ((s->Code() >> 24) != 'S')
        continue;

     const char* d = s->Description();

     if (!d or *d < '1' and *d > '4')
        continue;

     return s->Code();
     }
  return -1;
}
