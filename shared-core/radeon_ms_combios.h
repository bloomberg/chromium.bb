/*
 * Copyright 2006-2007 Advanced Micro Devices, Inc.
 * Copyright 2007 Jérôme Glisse
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * Authors:
 *    Jérôme Glisse <glisse@freedesktop.org>
 */
#ifndef __RADEON_MS_COMBIOS_H__
#define __RADEON_MS_COMBIOS_H__

#pragma pack(1)

#define ROM_HEADER                      0x48

struct combios_header
{
    uint8_t  ucTypeDefinition;
    uint8_t  ucExtFunctionCode;
    uint8_t  ucOemID1;
    uint8_t  ucOemID2;
    uint8_t  ucBiosMajorRev;
    uint8_t  ucBiosMinorRev;
    uint16_t usStructureSize;
    uint16_t usPointerToSmi;
    uint16_t usPointerToPmid;
    uint16_t usPointerToInitTable;
    uint16_t usPointerToCrcChecksumBlock;
    uint16_t usPointerToConfigFilename;
    uint16_t usPointerToLogonMessage;
    uint16_t usPointerToMiscInfo;
    uint16_t usPciBusDevInitCode;
    uint16_t usBiosRuntimeSegmentAddress;
    uint16_t usIoBaseAddress;
    uint16_t usSubsystemVendorID;
    uint16_t usSubsystemID;
    uint16_t usPostVendorID;
    uint16_t usInt10Offset;
    uint16_t usInt10Segment;
    uint16_t usMonitorInfo;
    uint16_t usPointerToConfigBlock;
    uint16_t usPointerToDacDelayInfo;
    uint16_t usPointerToCapDataStruct;
    uint16_t usPointerToInternalCrtTables;
    uint16_t usPointerToPllInfoBlock;
    uint16_t usPointerToTVInfoTable;
    uint16_t usPointerToDFPInfoTable;
    uint16_t usPointerToHWConfigTable;
    uint16_t usPointerToMMConfigTable;
    uint32_t ulTVStdPatchTableSignature;
    uint16_t usPointerToTVStdPatchTable;
    uint16_t usPointerToPanelInfoTable;
    uint16_t usPointerToAsicInfoTable;
    uint16_t usPointerToAuroraInfoTable;
    uint16_t usPointerToPllInitTable;
    uint16_t usPointerToMemoryConfigTable;
    uint16_t usPointerToSaveMaskTable;
    uint16_t usPointerHardCodedEdid;
    uint16_t usPointerToExtendedInitTable1;
    uint16_t usPointerToExtendedInitTable2;
    uint16_t usPointerToDynamicClkTable;
    uint16_t usPointerToReservedMemoryTable;
    uint16_t usPointerToBridgetInitTable;
    uint16_t usPointerToExtTMDSInitTable;
    uint16_t usPointerToMemClkInfoTable;
    uint16_t usPointerToExtDACTable;
    uint16_t usPointerToMiscInfoTable;
};

struct combios_pll_block
{
    /* Usually 6 */
    uint8_t  ucPLLBiosVersion;
    /* Size in bytes */
    uint8_t  ucStructureSize;
    /* Dot clock entry used for accelerated modes */
    uint8_t  ucDotClockEntry;
    /* Dot clock entry used for extended VGA modes */
    uint8_t  ucDotClockEntryVga;
    /* Offset into internal clock table used for by VGA parameter table */
    uint16_t usPointerToInternalClock;
    /* Offset into actual programmed frequency table at POST */
    uint16_t usPointerToFreqTable;
    /* XCLK setting, (memory clock in 10 KHz units) */
    uint16_t usXclkSetting;
    /* MCLK setting, (engine clock in 10 KHz units) */
    uint16_t usMclkSetting;
    /* Number of PLL information block to follow, currently value is 3 */
    uint8_t  ucPllInfoBlockNumber;
    /* Size of each PLL information block */
    uint8_t  ucPllInfoBlockSize;
    /* Reference frequency of the dot clock */
    uint16_t usDotClockRefFreq;
    /* Reference Divider of the dot clock */
    uint16_t usDotClockRefDiv;
    /* Min Frequency supported before post divider for the dot clock */
    uint32_t ulDotClockMinFreq;
    /* Max Frequency can be supported for the dot clock */
    uint32_t ulDotClockMaxFreq;
    /* Reference frequency of the MCLK, engine clock */
    uint16_t usMclkRefFreq;
    /* Reference Divider of the MCLK, engine clock */
    uint16_t usMclkRefDiv;
    /* Min Frequency supported before post divider for MCLK, engine clock */
    uint32_t ulMclkMinFreq;
    /* Max Frequency can be supported for the MCLK, engine clock */
    uint32_t ulMclkMaxFreq;
    /* Reference frequency of the XCLK, memory clock */
    uint16_t usXclkRefFreq;
    /* Reference Divider of the XCLK, memory clock */
    uint16_t usXclkRefDiv;
    /* Min Frequency supported before post divider for XCLK, memory clock */
    uint32_t ulXclkMinFreq;
    /* Max Frequency can be supported for the XCLK, memory clock */
    uint32_t ulXclkMaxFreq;

    /*this is the PLL Information Table Extended structure version 10 */
    uint8_t  ucNumberOfExtendedPllBlocks;
    uint8_t  ucSizePLLDefinition;
    uint16_t ulCrystalFrequencyPixelClock_pll;
    uint32_t ulMinInputPixelClockPLLFrequency;
    uint32_t ulMaxInputPixelClockPLLFrequency;
    uint32_t ulMinOutputPixelClockPLLFrequency;
    uint32_t ulMaxOutputPixelClockPLLFrequency;

    /*version 11 */
    uint16_t ulCrystalFrequencyEngineClock_pll;
    uint32_t ulMinInputFrequencyEngineClock_pll;
    uint32_t ulMaxInputFrequencyEngineClock_pll;
    uint32_t ulMinOutputFrequencyEngineClock_pll;
    uint32_t ulMaxOutputFrequencyEngineClock_pll;
    uint16_t ulCrystalFrequencyMemoryClock_pll;
    uint32_t ulMinInputFrequencyMemoryClock_pll;
    uint32_t ulMaxInputFrequencyMemoryClock_pll;
    uint32_t ulMinOutputFrequencyMemoryClock_pll;
    uint32_t ulMaxOutputFrequencyMemoryClock_pll;
    uint32_t ulMaximumDACOutputFrequency;
};

#define MAX_NO_OF_LCD_RES_TIMING                25

struct panel_information_table
{
    uint8_t  ucPanelIdentification;
    uint8_t  ucPanelIDString[24];
    uint16_t usHorizontalSize;
    uint16_t usVerticalSize;
    uint16_t usFlatPanelType;
    uint8_t  ucRedBitsPerPrimary;
    uint8_t  ucGreenBitsPerPrimary;
    uint8_t  ucBlueBitsPerPrimary;
    uint8_t  ucReservedBitsPerPrimary;
    uint8_t  ucPanelCaps;
    uint8_t  ucPowerSequenceDelayStepsInMS;
    uint8_t  ucSupportedRefreshRateExtended;
    uint16_t usExtendedPanelInfoTable;
    uint16_t usPtrToHalfFrameBufferInformationTable;
    uint16_t usVccOntoBlOn;
    uint16_t usOffDelay;
    uint16_t usRefDiv;
    uint8_t  ucPostDiv;
    uint16_t usFeedBackDiv;
    uint8_t  ucSpreadSpectrumType;
    uint16_t usSpreadSpectrumPercentage;
    uint8_t  ucBackLightLevel;
    uint8_t  ucBiasLevel;
    uint8_t  ucPowerSequenceDelay;
    uint32_t ulPanelData;
    uint8_t  ucPanelRefreshRateData;
    uint16_t usSupportedRefreshRate;
    uint16_t usModeTableOffset[MAX_NO_OF_LCD_RES_TIMING];
};

struct extended_panel_info_table
{
    uint8_t  ucExtendedPanelInfoTableVer;
    uint8_t  ucSSDelay;
    uint8_t  ucSSStepSizeIndex;
};

struct lcd_mode_table_center
{
    uint16_t usHorizontalRes;
    uint16_t usVerticalRes;
    uint8_t  ucModeType;
    uint16_t usOffset2ExpParamTable;
    uint16_t usOffset2TvParamTable;
    uint16_t usPixelClock;
    uint16_t usPixelClockAdjustment;
    uint16_t usFpPos;
    uint8_t  ucReserved;
    uint8_t  ucMiscBits;
    uint16_t usCrtcHTotal;
    uint16_t usCrtcHDisp;
    uint16_t usCrtcHSyncStrt;
    uint8_t  ucCrtcHSyncWid;
    uint16_t usCrtcVTotal;
    uint16_t usCrtcVDisp;
    uint16_t usCrtcVSyncStrt;
    uint8_t  ucOvrWidTop;
};

struct lcd_mode_table_exp
{
    uint16_t usPixelClock;
    uint16_t usPixelClockAdjustment;
    uint16_t usFpPos;
    uint8_t  ucReserved;
    uint8_t  ucMiscBits;
    uint16_t usCrtcHTotal;
    uint16_t usCrtcHDisp;
    uint16_t usCrtcHSyncStrt;
    uint8_t  ucCrtcHSyncWid;
    uint16_t usCrtcVTotal;
    uint16_t usCrtcVDisp;
    uint16_t usCrtcVSyncStrt;
    uint8_t  ucOvrWidTop;
    uint16_t usHorizontalBlendRatio;
    uint32_t ulVgaVertStretching;
    uint16_t usCopVertStretching;
    uint16_t usVgaExtVertStretching;
};

struct tmds_pll_cntl_block
{
    uint16_t usClockUpperRange;
    uint32_t ulPllSetting;
};

#define MAX_PLL_CNTL_ENTRIES                    8

struct combios_dfp_info_table
{
    uint8_t                     ucDFPInfoTableRev;
    uint8_t                     ucDFPInfoTableSize;
    uint16_t                    usOffsetDetailedTimingTable;
    uint8_t                     ucReserved;
    uint8_t                     ucNumberOfClockRanges;
    uint16_t                    usMaxPixelClock;
    uint32_t                    ulInitValueTmdsPllCntl;
    uint32_t                    ulFinalValueTmdsPllCntl;
    struct tmds_pll_cntl_block  sTmdsPllCntlBlock[MAX_PLL_CNTL_ENTRIES];
};

struct combios_exttmds_table_header
{
    uint8_t  ucTableRev;
    uint16_t usTableSize;
    uint8_t  ucNoBlocks;
};

struct combios_exttmds_block_header
{
    uint16_t usMaxFreq;
    uint8_t  ucI2CSlaveAddr;
    uint8_t  ucI2CLine;
    uint8_t  ucConnectorId;
    uint8_t  ucFlags;
};

/* Connector table - applicable from Piglet and later ASICs
    byte 0     (embedded revision)
        [7:4]    = number of chips (valid number 1 - 15)
        [3:0]    = revision number of table (valid number 1 - 15)

    byte 1 (Chip info)
        [7:4]    = chip number, max. 15 (valid number 1 - 15)
        [3:0]    = number of connectors for that chip, (valid number 1 - 15)
                   (number of connectors = number of 'Connector info' entries
                    for that chip)

    byte 2,3 (Connector info)
        [15:12]    - connector type
            = 0     - no connector
            = 1     - proprietary
            = 2     - CRT
            = 3     - DVI-I
            = 4      - DVI-D
            = 5-15    - reserved for future expansion
        [11:8]    - DDC line pair used for that connector
            = 0     - no DDC
            = 1     - MONID 0/1
            = 2     - DVI_DDC
            = 3     - VGA_DDC
            = 4     - CRT2_DDC
            = 5-15    - reserved for future expansion
        [5] - bit indicating presence of multiplexer for TV,CRT2
        [7:6]    - reserved for future expansion
        [4]    - TMDS type
            = 0     - internal TMDS
            = 1      - external TMDS
        [3:1]    - reserved for future expansion
        [0]    - DAC associated with that connector
            = 0    - CRT DAC
            = 1    - non-CRT DAC (e.g. TV DAC, external DAC ..)

    byte 4,5,6...     - byte 4,5 can be another "Connector info" word
                      describing another connector
                     - or byte 5 is a "Chip info" byte for anther chip,
                      then start with byte 5,6 to describe connectors
                      for that chip
                    - or byte 5 = 0 if all connectors for all chips on
                      board have been described, no more connector left
                      to describe.
*/
#define BIOS_CONNECTOR_INFO__TYPE__MASK                   0xF000
#define BIOS_CONNECTOR_INFO__TYPE__SHIFT                  0x0000000C
#define BIOS_CONNECTOR_TYPE__NONE                         0x00000000
#define BIOS_CONNECTOR_TYPE__PROPRIETARY                  0x00000001
#define BIOS_CONNECTOR_TYPE__CRT                          0x00000002
#define BIOS_CONNECTOR_TYPE__DVI_I                        0x00000003
#define BIOS_CONNECTOR_TYPE__DVI_D                        0x00000004

#define BIOS_CONNECTOR_INFO__DDC_LINE__MASK               0x0F00
#define BIOS_CONNECTOR_INFO__DDC_LINE__SHIFT              0x00000008
#define BIOS_DDC_LINE__NONE                               0x00000000
#define BIOS_DDC_LINE__MONID01                            0x00000001
#define BIOS_DDC_LINE__DVI                                0x00000002
#define BIOS_DDC_LINE__VGA                                0x00000003
#define BIOS_DDC_LINE__CRT2                               0x00000004
#define BIOS_DDC_LINE__GPIOPAD                            0x00000005
#define BIOS_DDC_LINE__ZV_LCDPAD                          0x00000006

#define BIOS_CONNECTOR_INFO__TMDS_TYPE__MASK              0x0010
#define BIOS_CONNECTOR_INFO__TMDS_TYPE__SHIFT             0x00000004
#define BIOS_TMDS_TYPE__INTERNAL                          0x00000000
#define BIOS_TMDS_TYPE__EXTERNAL                          0x00000001

#define BIOS_CONNECTOR_INFO__DAC_TYPE__MASK               0x0001
#define BIOS_CONNECTOR_INFO__DAC_TYPE__SHIFT              0x00000000
#define BIOS_DAC_TYPE__CRT                                0x00000000
#define BIOS_DAC_TYPE__NON_CRT                            0x00000001

#define BIOS_CONNECTOR_INFO__MUX_MASK                     0x00000020
#define BIOS_CONNECTOR_INFO__MUX_SHIFT                    0x00000005

#define BIOS_CHIPINFO_HEADER__CHIP_NUMBER__MASK           0xF0
#define BIOS_CHIPINFO_HEADER__CHIP_NUMBER__SHIFT          0x00000004

#define BIOS_CHIPINFO_HEADER__NUMBER_OF_CONNECTORS__MASK  0x0F
#define BIOS_CHIPINFO_HEADER__NUMBER_OF_CONNECTORS__SHIFT 0x00000000

#define BIOS_CHIPINFO__MAX_NUMBER_OF_CONNECTORS           0x00000010

struct combios_connector_chip_info
{
    uint8_t  ucChipHeader;
    uint16_t sConnectorInfo[BIOS_CHIPINFO__MAX_NUMBER_OF_CONNECTORS];
};

#define BIOS_CONNECTOR_HEADER__NUMBER_OF_CHIPS__MASK      0xF0
#define BIOS_CONNECTOR_HEADER__NUMBER_OF_CHIPS__SHIFT     0x00000004

#define BIOS_CONNECTOR_HEADER__TABLE_REVISION__MASK       0x0F
#define BIOS_CONNECTOR_HEADER__TABLE_REVISION__SHIFT      0x00000000

struct combios_connector_table
{
    uint8_t                            ucConnectorHeader;
    struct combios_connector_chip_info sChipConnectorInfo[0x10];
};

#pragma pack()

int combios_parse(unsigned char *rom, struct combios_header *header);

#endif
