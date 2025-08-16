/*
 * common.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <ctype.h>
#include <vdr/tools.h>
#include "common.h"

uint16_t ts_pid(const uint8_t *bufP)
{
  return (uint16_t)(((bufP[1] & 0x1f) << 8) + bufP[2]);
}

uint8_t payload(const uint8_t *bufP)
{
  if (!(bufP[3] & 0x10)) // no payload?
     return 0;

  if (bufP[3] & 0x20) {  // adaptation field?
     if (bufP[4] > 183)  // corrupted data?
        return 0;
     else
        return (uint8_t)((184 - 1) - bufP[4]);
     }

  return 184;
}

const char *id_pid(const u_short pidP)
{
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i) {
      if (pidP == section_filter_table[i].pid)
         return section_filter_table[i].tag;
      }
  return "---";
}

char *StripTags(char *strP)
{
  if (strP) {
     char *c = strP, *r = strP, t = 0;
     while (*strP) {
           if (*strP == '<')
              ++t;
           else if (*strP == '>')
              --t;
           else if (t < 1)
              *(c++) = *strP;
           ++strP;
           }
     *c = 0;
     return r;
     }
  return NULL;
}

char *SkipZeroes(const char *strP)
{
  if ((uchar)*strP != '0')
     return (char *)strP;
  while (*strP && (uchar)*strP == '0')
        strP++;
  return (char *)strP;
}

cString ChangeCase(const cString &strP, bool upperP)
{
  cString res(strP);
  char *p = (char *)*res;
  while (p && *p) {
        *p = upperP ? toupper(*p) : tolower(*p);
        ++p;
        }
  return res;
}

const section_filter_table_type section_filter_table[SECTION_FILTER_TABLE_SIZE] =
{
  // description                        tag    pid   tid   mask
  {trNOOP("PAT (0x00)"),                "PAT", 0x00, 0x00, 0xFF},
  {trNOOP("NIT (0x40)"),                "NIT", 0x10, 0x40, 0xFF},
  {trNOOP("SDT (0x42)"),                "SDT", 0x11, 0x42, 0xFF},
  {trNOOP("EIT (0x4E/0x4F/0x5X/0x6X)"), "EIT", 0x12, 0x40, 0xC0},
  {trNOOP("TDT (0x70)"),                "TDT", 0x14, 0x70, 0xFF},
};

// https://app.dvbservices.com/identifiers/ca_system_id
const ca_systems_table_type ca_systems_table[CA_SYSTEMS_TABLE_SIZE] =
{
  // start end     description
  {0x0000, 0x0000, "Reserved"                                 }, // 0
//{0x0001, 0x0001, "IPDC SPP"                                 }, //
//{0x0002, 0x0002, "18Crypt"                                  }, //
//{0x0004, 0x0004, "OMA DRM"                                  }, //
//{0x0005, 0x0006, "OMA BCAST 1.0"                            }, //
//{0x0007, 0x0007, "(Reserved for Open IPTV Forum)"           }, //
//{0x0008, 0x0008, "Open Mobile Alliance"                     }, //
//{0x0009, 0x00FF, "reserved for future use"                  }, //
  {0x0100, 0x01FF, "0100-01FF: Canal Plus/Mediaguard"         }, // 1
  {0x0200, 0x02FF, "0200-02FF: CCETT"                         }, // 2
  {0x0300, 0x03FF, "0300-03FF: Kabel Deutschland"             }, // 3
  {0x0400, 0x04FF, "0400-04FF: Eurodec"                       }, // 4
  {0x0500, 0x05FF, "0500-05FF: France Telecom/Viaccess"       }, // 5
  {0x0600, 0x06FF, "0600-06FF: Irdeto"                        }, // 6
  {0x0700, 0x07FF, "0700-07FF: Jerrold/GI/Motorola DigiCipher"}, // 7
  {0x0800, 0x08FF, "0800-08FF: Matra Communication"           }, // 8
  {0x0900, 0x09FF, "0900-09FF: News Datacom/NDS Videoguard"   }, // 9
  {0x0A00, 0x0AFF, "0A00-0AFF: Nokia"                         }, // 10
  {0x0B00, 0x0BFF, "0B00-0BFF: Conax AS"                      }, // 11
  {0x0C00, 0x0CFF, "0C00-0CFF: NTL"                           }, // 12
  {0x0D00, 0x0DFF, "0D00-0DFF: Irdeto/CryptoWorks"            }, // 13
  {0x0E00, 0x0EFF, "0E00-0EFF: Scientific Atlanta/PowerVu"    }, // 14
  {0x0F00, 0x0FFF, "0F00-0FFF: Sony"                          }, // 15
  {0x1000, 0x10FF, "1000-10FF: Tandberg/RAS"                  }, // 16
  {0x1100, 0x11FF, "1100-11FF: Thomson"                       }, // 17
  {0x1200, 0x12FF, "1200-12FF: TV/Com NagraVision"            }, // 18
  {0x1300, 0x14FF, "1300-14FF: HPT/HRT"                       }, // 19
  {0x1500, 0x15FF, "1500-15FF: IBM"                           }, // 20
  {0x1600, 0x16FF, "1600-16FF: Nera"                          }, // 21
  {0x1700, 0x17FF, "1700-17FF: Verimatrix"                    }, // 22
  {0x1800, 0x18FF, "1800-18FF: Kudelski SA/NagraVision"       }, // 23
  {0x1900, 0x19FF, "1900-19FF: Titan Info Systems"            }, // 24
  {0x1E00, 0x1E07, "1E00-1E07: Alticast"                      }, // 25
  {0x1EA0, 0x1EA0, "1EA0: Protac"                             }, // 26
  {0x1EB0, 0x1EB0, "1EB0: Telecast Technology"                }, // 27
  {0x1EC0, 0x1EC2, "1EC0-1EC2: Cryptoguard AB"                }, // 28
  {0x1ED0, 0x1ED1, "1ED0-1ED1: MM Comunicaciones S.A."        }, // 29
  {0x2000, 0x20FF, "2000-20FF: Telefonica Servicios AV"       }, // 30
  {0x2100, 0x21FF, "2100-21FF: France Telecom/CNES/DGA"       }, // 31
  {0x2200, 0x22FF, "2200-22FF: Harmonic/Codicrypt"            }, // 32
  {0x2300, 0x23FF, "2300-23FF: BARCO AS"                      }, // 33
  {0x2400, 0x24FF, "2400-24FF: StarGuide Digital"             }, // 34
  {0x2500, 0x25FF, "2500-25FF: Mentor Data System"            }, // 35
  {0x2600, 0x26FF, "2600-26FF: EBU/BISS"                      }, // 36
  {0x2700, 0x270F, "2700-270F: PolyCipher"                    }, // 37
  {0x2710, 0x2711, "2710-2711: Extended Secure"               }, // 38
  {0x2712, 0x2712, "2712: Derincrypt"                         }, // 39
  {0x2713, 0x2714, "2713-2714: Wuhan Tianyu"                  }, // 40
  {0x2715, 0x2715, "2715: Network Broadcast"                  }, // 41
  {0x2716, 0x2716, "2716: Bromteck"                           }, // 42
  {0x2717, 0x2718, "2717-2718: Logiways"                      }, // 43
  {0x2719, 0x2719, "2719: S-Curious RT/VanyaCas"              }, // 44
  {0x27A0, 0x27A4, "27A0-27A4: ByDesign India"                }, // 45
  {0x2800, 0x2809, "2800-2809: LCS LLC"                       }, // 46
  {0x2810, 0x2810, "2810: Multikom Deltasat"                  }, // 47
  {0x4347, 0x4347, "4347: Crypton"                            }, // 48
  {0x4348, 0x4348, "4348: Secure TV, LLC"                     }, // 49
  {0x4700, 0x47FF, "4700-47FF: Motorola"                      }, // 50
  {0x4825, 0x4825, "4825: ChinaEPG"                           }, // 51
  {0x4855, 0x4856, "4855-4856: Intertrust"                    }, // 52
  {0x4800, 0x48FF, "4800-48FF: Telemann"                      }, // 53
  {0x4900, 0x49FF, "4900-49FF: CrytoWorks Irdeto"             }, // 54
  {0x4A10, 0x4A1F, "4A10-4A1F: Easycas"                       }, // 55
  {0x4A20, 0x4A2F, "4A20-4A2F: AlphaCrypt"                    }, // 56
  {0x4A30, 0x4A3F, "4A30-4A3F: DVN Holdings"                  }, // 57
  {0x4A40, 0x4A4F, "4A40-4A4F: Shanghai Advanced Digital"     }, // 58
  {0x4A50, 0x4A5F, "4A50-4A5F: Shenzhen Kingsky Company"      }, // 59
  {0x4A60, 0x4A6F, "4A60-4A6F: Neotion/SkyCrypt"              }, // 60
  {0x4A70, 0x4A7F, "4A70-4A7F: Dreamcrypt"                    }, // 61
  {0x4A80, 0x4A8F, "4A80-4A8F: ThalesCrypt"                   }, // 62
  {0x4A90, 0x4A9F, "4A90-4A9F: Runcom Technologies"           }, // 63
  {0x4AA0, 0x4AAF, "4AA0-4AAF: SIDSA"                         }, // 64
  {0x4AB0, 0x4ABF, "4AB0-4ABF: Beijing Compunicate"           }, // 65
  {0x4AC0, 0x4ACF, "4AC0-4ACF: Latens Systems"                }, // 66
  {0x4AD0, 0x4AD1, "4AD0-4AD1: XCrypt"                        }, // 67
  {0x4AD2, 0x4AD3, "4AD2-4AD3: Beijing Digital Video"         }, // 68
  {0x4AD4, 0x4AD5, "4AD4-4AD5: Widevine/Omnicrypt"            }, // 69
  {0x4AD6, 0x4AD7, "4AD6-4AD7: SK Telecom Co., Ltd."          }, // 70
  {0x4AD8, 0x4AD9, "4AD8-4AD9: Enigma Systems"                }, // 71
  {0x4ADA, 0x4ADA, "4ADA: Wyplay SAS"                         }, // 72
  {0x4ADB, 0x4ADB, "4ADB: Jinan Taixin Electronics"           }, // 73
  {0x4ADC, 0x4ADC, "4ADC: LogiWays"                           }, // 74
  {0x4ADD, 0x4ADD, "4ADD: ATSC System Msg"                    }, // 75
  {0x4ADE, 0x4ADE, "4ADE: CerberCrypt"                        }, // 76
  {0x4ADF, 0x4ADF, "4ADF: Caston Co."                         }, // 77
  {0x4AE0, 0x4AE1, "4AE0-4AE1: Cifra LLC/DRE-Crypt"           }, // 78
  {0x4AE2, 0x4AE3, "4AE2-4AE3: Microsoft"                     }, // 79
  {0x4AE4, 0x4AE4, "4AE4-4AE4: Coretrust/CoreCrypt"           }, // 80
  {0x4AE5, 0x4AE5, "4AE5-4AE5: IK SATPROF/PRO-Crypt"          }, // 81
  {0x4AE6, 0x4AE6, "4AE6: SypherMedia International"          }, // 82
  {0x4AE7, 0x4AE7, "4AE7: Guangzhou Ewider Technology"        }, // 83
  {0x4AE8, 0x4AE8, "4AE8: FG DIGITAL Ltd."                    }, // 84
  {0x4AE9, 0x4AE9, "4AE9: Dreamer-i Co., Ltd."                }, // 85
  {0x4AEA, 0x4AEA, "4AEA: Cryptoguard"                        }, // 86
  {0x4AEB, 0x4AEB, "4AEB: Abel DRM Systems AS"                }, // 87
  {0x4AEC, 0x4AEC, "4AEC: FTS DVL SRL"                        }, // 88
  {0x4AED, 0x4AED, "4AED: Unitend Technologies, Inc."         }, // 89
  {0x4AEE, 0x4AEE, "4AEE: Deltacom Electronics OOD"           }, // 90
  {0x4AEF, 0x4AEF, "4AEF: NetUP Inc."                         }, // 91
  {0x4AF0, 0x4AF0, "4AF0: ABV International"                  }, // 92
  {0x4AF1, 0x4AF2, "4AF1-4AF2: China DTV Media"               }, // 93
  {0x4AF3, 0x4AF3, "4AF3: Baustem"                            }, // 94
  {0x4AF4, 0x4AF4, "4AF4: Marlin Developer Community"         }, // 95
  {0x4AF5, 0x4AF5, "4AF5: SecureMedia"                        }, // 96
  {0x4AF6, 0x4AF6, "4AF6: Tongfang CAS"                       }, // 97
  {0x4AF7, 0x4AF7, "4AF7: MSA"                                }, // 98
  {0x4AF8, 0x4AF8, "4AF8: Griffin CAS"                        }, // 99
  {0x4AF9, 0x4AFA, "4AF9-4AFA: Beijing Topreal"               }, // 100
  {0x4AFB, 0x4AFB, "4AFB: NST"                                }, // 101
  {0x4AFC, 0x4AFC, "4AFC: Panaccess Systems"                  }, // 102
  {0x4AFD, 0x4AFD, "4AFD: Comteza SIA"                        }, // 103
  {0x4B00, 0x4B02, "4B00-4B02: Tongfang CAS"                  }, // 104
  {0x4B03, 0x4B03, "4B03: DuoCrypt"                           }, // 105
  {0x4B04, 0x4B04, "4B04: Great Wall CAS"                     }, // 106
  {0x4B05, 0x4B06, "4B05-4B06: Digicap"                       }, // 107
  {0x4B07, 0x4B07, "4B07: Wuhan Reikost"                      }, // 108
  {0x4B08, 0x4B08, "4B08: Philips"                            }, // 109
  {0x4B09, 0x4B09, "4B09: Ambernetas"                         }, // 110
  {0x4B0A, 0x4B0B, "4B0A-4B0B: Beijing Sumavision"            }, // 111
  {0x4B0C, 0x4B0F, "4B0C-4B0F: Sichuan changhong electric"    }, // 112
  {0x4B10, 0x4B10, "4B10: Exterity Limited"                   }, // 113
  {0x4B11, 0x4B12, "4B11-4B12: Advanced Digital Platform"     }, // 114
  {0x4B13, 0x4B14, "4B13-4B14: Microsoft"                     }, // 115
  {0x4B19, 0x4B19, "4B19: Ridsys"                             }, // 116
  {0x4B20, 0x4B22, "4B20-4B22: Multikom Deltasat"             }, // 117
  {0x4B23, 0x4B23, "4B23: SkyNLand Video Networks"            }, // 118
  {0x4B24, 0x4B24, "4B24: Prowill AB"                         }, // 119
  {0x4B25, 0x4B25, "4B25: Suresoft Systems"                   }, // 120
  {0x4B26, 0x4B26, "4B26: Unitend Technologies"               }, // 121
  {0x4B30, 0x4B31, "4B30-4B31: Vietnam Multimedia"            }, // 122
  {0x4B3A, 0x4B3A, "4B3A: ipanel"                             }, // 123
  {0x4B3B, 0x4B3B, "4B3B: Jinggangshan Electric"              }, // 124
  {0x4B40, 0x4B41, "4B40-4B41: Excaf Telecom"                 }, // 125
  {0x4B42, 0x4B43, "4B42-4B43: CI Plus LLP"                   }, // 126
  {0x4B4A, 0x4B4A, "4B4A: Topwell"                            }, // 127
  {0x4B4B, 0x4B4D, "4B4B-4B4D: ABV"                           }, // 128
  {0x4B50, 0x4B53, "4B50-4B53: Safeview India"                }, // 129
  {0x4B54, 0x4B54, "4B54: Telelynx"                           }, // 130
  {0x4B60, 0x4B60, "4B60: Kiwisat"                            }, // 131
  {0x4B61, 0x4B61, "4B61: O2 Czech"                           }, // 132
  {0x4B62, 0x4B62, "4B62: GMA New Media"                      }, // 133
  {0x4B63, 0x4B63, "4B63: redCrypter"                         }, // 134
  {0x4B64, 0X4B64, "4B64: TVKey"                              }, // 135
  {0x4DCA, 0x4DCA, "4DCA: Shandong Taixin"                    }, // 136
  {0x5347, 0x5347, "5347: GkWare"                             }, // 137
  {0x5448, 0x5449, "5448-5449: Gospell"                       }, // 138
  {0x5601, 0x5604, "5601-5604: Verimatrix"                    }, // 139
  {0x5605, 0x5606, "5605-5606: Sichuan Juizhou"               }, // 140
  {0x5607, 0x5608, "5607-5608: Viewscenes"                    }, // 141
  {0x5609, 0x5609, "5609: Power On"                           }, // 142
  {0x56A0, 0x56A0, "56A0: Laxmi remote india"                 }, // 143
  {0x56A1, 0x56A1, "56A1: C-Dot"                              }, // 144
  {0x56B0, 0x56B0, "56B0: Laxmi remote india"                 }, // 145
  {0x56C0, 0x56C9, "56C0-56C9: Google"                        }, // 146
  {0x56D0, 0x56D0, "56D0: Onnet Systems India"                }, // 147
  {0x56D1, 0x56D1, "56D1: redCrypter"                         }, // 148
  {0x56D2, 0x56D2, "56D2: Imaqliq Service"                    }, // 149
  {0x56D3, 0x56D3, "56D3: Kodeniti"                           }, // 150
  {0x56D4, 0x56D4, "56D4: Eleсtra"                            }, // 151
  {0x6448, 0x6449, "6448-6449: Gospell"                       }, // 152
  {0x7700, 0x7704, "7700-7704: LCC Cifra"                     }, // 153
  {0x7BE0, 0x7BE1, "7BE0-7BE1: DRE-Crypt"                     }, // 154
  {0xAA00, 0xAA01, "AA00-AA01: BestCAS"                       }  // 155
};

bool checkCASystem(unsigned int cicamP, int caidP)
{
  // always skip the first row
  if ((cicamP > 0) && (cicamP < ELEMENTS(ca_systems_table)))
     return ((caidP >= ca_systems_table[cicamP].start) && (caidP <= ca_systems_table[cicamP].end));
  return false;
}
