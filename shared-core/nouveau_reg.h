

#define NV03_BOOT_0                                        0x00100000
#    define NV03_BOOT_0_RAM_AMOUNT                         0x00000003
#    define NV03_BOOT_0_RAM_AMOUNT_8MB                     0x00000000
#    define NV03_BOOT_0_RAM_AMOUNT_2MB                     0x00000001
#    define NV03_BOOT_0_RAM_AMOUNT_4MB                     0x00000002
#    define NV03_BOOT_0_RAM_AMOUNT_8MB_SDRAM               0x00000003
#    define NV04_BOOT_0_RAM_AMOUNT_32MB                    0x00000000
#    define NV04_BOOT_0_RAM_AMOUNT_4MB                     0x00000001
#    define NV04_BOOT_0_RAM_AMOUNT_8MB                     0x00000002
#    define NV04_BOOT_0_RAM_AMOUNT_16MB                    0x00000003

#define NV04_FIFO_DATA                                     0x0010020c
#    define NV10_FIFO_DATA_RAM_AMOUNT_MB_MASK              0xfff00000
#    define NV10_FIFO_DATA_RAM_AMOUNT_MB_SHIFT             20

#define NV03_PGRAPH_STATUS                                 0x004006b0
#define NV04_PGRAPH_STATUS                                 0x00400700

#define NV_RAMIN                                           0x00700000

#define NV_RAMHT_HANDLE_OFFSET                             0
#define NV_RAMHT_CONTEXT_OFFSET                            4
#    define NV_RAMHT_CONTEXT_VALID                         (1<<31)
#    define NV_RAMHT_CONTEXT_CHANNEL_SHIFT                 24
#    define NV_RAMHT_CONTEXT_ENGINE_SHIFT                  16
#        define NV_RAMHT_CONTEXT_ENGINE_SOFTWARE           0
#        define NV_RAMHT_CONTEXT_ENGINE_GRAPHICS           1
#    define NV_RAMHT_CONTEXT_INSTANCE_SHIFT                0
#    define NV40_RAMHT_CONTEXT_CHANNEL_SHIFT               23
#    define NV40_RAMHT_CONTEXT_ENGINE_SHIFT                20
#    define NV40_RAMHT_CONTEXT_INSTANCE_SHIFT              0

#define NV_DMA_ACCESS_RW 0
#define NV_DMA_ACCESS_RO 1
#define NV_DMA_ACCESS_WO 2
#define NV_DMA_TARGET_VIDMEM 0
#define NV_DMA_TARGET_AGP    3

#define NV03_FIFO_SIZE                                     0x8000UL
#define NV_MAX_FIFO_NUMBER                                 32
#define NV03_FIFO_REGS_SIZE                                0x10000
#define NV03_FIFO_REGS(i)                                  (0x00800000+i*NV03_FIFO_REGS_SIZE)
#    define NV03_FIFO_REGS_DMAPUT(i)                       (NV03_FIFO_REGS(i)+0x40)
#    define NV03_FIFO_REGS_DMAGET(i)                       (NV03_FIFO_REGS(i)+0x44)

#define NV_PMC_BOOT_0                                      0x00000000
#define NV_PMC_INTSTAT                                     0x00000100
#    define NV_PMC_INTSTAT_PFIFO_PENDING                      (1<< 8)
#    define NV_PMC_INTSTAT_PGRAPH_PENDING                     (1<<12)
#    define NV_PMC_INTSTAT_CRTC0_PENDING                      (1<<24)
#    define NV_PMC_INTSTAT_CRTC1_PENDING                      (1<<25)
#    define NV_PMC_INTSTAT_CRTCn_PENDING                      (3<<24)
#define NV_PMC_INTEN                                       0x00000140
#    define NV_PMC_INTEN_MASTER_ENABLE                        (1<< 0)

#define NV_PGRAPH_DEBUG_4                                  0x00400090
#define NV_PGRAPH_INTSTAT                                  0x00400100
#define NV04_PGRAPH_INTEN                                  0x00400140
#define NV40_PGRAPH_INTEN                                  0x0040013C
#    define NV_PGRAPH_INTR_NOTIFY                             (1<< 0)
#    define NV_PGRAPH_INTR_MISSING_HW                         (1<< 4)
#    define NV_PGRAPH_INTR_CONTEXT_SWITCH                     (1<<12)
#    define NV_PGRAPH_INTR_BUFFER_NOTIFY                      (1<<16)
#    define NV_PGRAPH_INTR_ERROR                              (1<<20)
#define NV_PGRAPH_CTX_CONTROL                              0x00400144
#define NV_PGRAPH_NV40_UNK220                              0x00400220
#    define NV_PGRAPH_NV40_UNK220_FB_INSTANCE
#define NV_PGRAPH_CTX_USER                                 0x00400148
#define NV_PGRAPH_CTX_SWITCH1                              0x0040014C
#define NV_PGRAPH_CTX_SWITCH2                              0x00400150
#define NV_PGRAPH_CTX_SWITCH3                              0x00400154
#define NV_PGRAPH_CTX_SWITCH4                              0x00400158
#define NV_PGRAPH_CTX_SWITCH5                              0x0040015C
#define NV_PGRAPH_X_MISC                                   0x00400500
#define NV_PGRAPH_Y_MISC                                   0x00400504
#define NV_PGRAPH_VALID1                                   0x00400508
#define NV_PGRAPH_SOURCE_COLOR                             0x0040050C
#define NV_PGRAPH_MISC24_0                                 0x00400510
#define NV_PGRAPH_XY_LOGIC_MISC0                           0x00400514
#define NV_PGRAPH_XY_LOGIC_MISC1                           0x00400518
#define NV_PGRAPH_XY_LOGIC_MISC2                           0x0040051C
#define NV_PGRAPH_XY_LOGIC_MISC3                           0x00400520
#define NV_PGRAPH_CLIPX_0                                  0x00400524
#define NV_PGRAPH_CLIPX_1                                  0x00400528
#define NV_PGRAPH_CLIPY_0                                  0x0040052C
#define NV_PGRAPH_CLIPY_1                                  0x00400530
#define NV_PGRAPH_ABS_ICLIP_XMAX                           0x00400534
#define NV_PGRAPH_ABS_ICLIP_YMAX                           0x00400538
#define NV_PGRAPH_ABS_UCLIP_XMIN                           0x0040053C
#define NV_PGRAPH_ABS_UCLIP_YMIN                           0x00400540
#define NV_PGRAPH_ABS_UCLIP_XMAX                           0x00400544
#define NV_PGRAPH_ABS_UCLIP_YMAX                           0x00400548
#define NV_PGRAPH_ABS_UCLIPA_XMIN                          0x00400560
#define NV_PGRAPH_ABS_UCLIPA_YMIN                          0x00400564
#define NV_PGRAPH_ABS_UCLIPA_XMAX                          0x00400568
#define NV_PGRAPH_ABS_UCLIPA_YMAX                          0x0040056C
#define NV_PGRAPH_MISC24_1                                 0x00400570
#define NV_PGRAPH_MISC24_2                                 0x00400574
#define NV_PGRAPH_VALID2                                   0x00400578
#define NV_PGRAPH_PASSTHRU_0                               0x0040057C
#define NV_PGRAPH_PASSTHRU_1                               0x00400580
#define NV_PGRAPH_PASSTHRU_2                               0x00400584
#define NV_PGRAPH_DIMX_TEXTURE                             0x00400588
#define NV_PGRAPH_WDIMX_TEXTURE                            0x0040058C
#define NV_PGRAPH_MONO_COLOR0                              0x00400600
#define NV_PGRAPH_ROP3                                     0x00400604
#define NV_PGRAPH_BETA_AND                                 0x00400608
#define NV_PGRAPH_BETA_PREMULT                             0x0040060C
#define NV_PGRAPH_BOFFSET0                                 0x00400640
#define NV_PGRAPH_BOFFSET1                                 0x00400644
#define NV_PGRAPH_BOFFSET2                                 0x00400648
#define NV_PGRAPH_BOFFSET3                                 0x0040064C
#define NV_PGRAPH_BOFFSET4                                 0x00400650
#define NV_PGRAPH_BOFFSET5                                 0x00400654
#define NV_PGRAPH_BBASE0                                   0x00400658
#define NV_PGRAPH_BBASE1                                   0x0040065C
#define NV_PGRAPH_BBASE2                                   0x00400660
#define NV_PGRAPH_BBASE3                                   0x00400664
#define NV_PGRAPH_BBASE4                                   0x00400668
#define NV_PGRAPH_BBASE5                                   0x0040066C
#define NV_PGRAPH_BPITCH0                                  0x00400670
#define NV_PGRAPH_BPITCH1                                  0x00400674
#define NV_PGRAPH_BPITCH2                                  0x00400678
#define NV_PGRAPH_BPITCH3                                  0x0040067C
#define NV_PGRAPH_BPITCH4                                  0x00400680
#define NV_PGRAPH_BLIMIT0                                  0x00400684
#define NV_PGRAPH_BLIMIT1                                  0x00400688
#define NV_PGRAPH_BLIMIT2                                  0x0040068C
#define NV_PGRAPH_BLIMIT3                                  0x00400690
#define NV_PGRAPH_BLIMIT4                                  0x00400694
#define NV_PGRAPH_BLIMIT5                                  0x00400698
#define NV_PGRAPH_BSWIZZLE2                                0x0040069C
#define NV_PGRAPH_BSWIZZLE5                                0x004006A0
#define NV_PGRAPH_SURFACE                                  0x00400710
#define NV_PGRAPH_STATE                                    0x00400714
#define NV_PGRAPH_NOTIFY                                   0x00400718

#define NV_PGRAPH_FIFO                                     0x00400720

#define NV_PGRAPH_BPIXEL                                   0x00400724
#define NV_PGRAPH_RDI_INDEX                                0x00400750
#define NV_PGRAPH_RDI_DATA                                 0x00400754
#define NV_PGRAPH_FFINTFC_ST2                              0x00400764
#define NV_PGRAPH_DMA_PITCH                                0x00400770
#define NV_PGRAPH_DVD_COLORFMT                             0x00400774
#define NV_PGRAPH_SCALED_FORMAT                            0x00400778
#define NV_PGRAPH_CHANNEL_CTX_TABLE                        0x00400780
#define NV_PGRAPH_CHANNEL_CTX_SIZE                         0x00400784
#define NV_PGRAPH_CHANNEL_CTX_POINTER                      0x00400788
#define NV_PGRAPH_PATT_COLOR0                              0x00400800
#define NV_PGRAPH_PATT_COLOR1                              0x00400804
#define NV_PGRAPH_PATTERN_SHAPE                            0x00400810
#define NV_PGRAPH_CHROMA                                   0x00400814
#define NV_PGRAPH_STORED_FMT                               0x00400830
#define NV_PGRAPH_XFMODE0                                  0x00400F40
#define NV_PGRAPH_XFMODE1                                  0x00400F44
#define NV_PGRAPH_GLOBALSTATE0                             0x00400F48
#define NV_PGRAPH_GLOBALSTATE1                             0x00400F4C
#define NV_PGRAPH_PIPE_ADDRESS                             0x00400F50
#define NV_PGRAPH_PIPE_DATA                                0x00400F54
#define NV_PGRAPH_DMA_START_0                              0x00401000
#define NV_PGRAPH_DMA_START_1                              0x00401004
#define NV_PGRAPH_DMA_LENGTH                               0x00401008
#define NV_PGRAPH_DMA_MISC                                 0x0040100C

/* It's a guess that this works on NV03. Confirmed on NV04, though */
#define NV_PFIFO_DELAY_0                                   0x00002040
#define NV_PFIFO_DMA_TIMESLICE                             0x00002044
#define NV_PFIFO_INTSTAT                                   0x00002100
#define NV_PFIFO_INTEN                                     0x00002140
#    define NV_PFIFO_INTR_CACHE_ERROR                         (1<< 0)
#    define NV_PFIFO_INTR_RUNOUT                              (1<< 4)
#    define NV_PFIFO_INTR_RUNOUT_OVERFLOW                     (1<< 8)
#    define NV_PFIFO_INTR_DMA_PUSHER                          (1<<12)
#    define NV_PFIFO_INTR_DMA_PT                              (1<<16)
#    define NV_PFIFO_INTR_SEMAPHORE                           (1<<20)
#    define NV_PFIFO_INTR_ACQUIRE_TIMEOUT                     (1<<24)
#define NV_PFIFO_RAMHT                                     0x00002210
#define NV_PFIFO_RAMFC                                     0x00002214
#define NV_PFIFO_RAMRO                                     0x00002218
#define NV40_PFIFO_RAMFC                                   0x00002220
#define NV_PFIFO_CACHES                                    0x00002500
#define NV_PFIFO_MODE                                      0x00002504
#define NV_PFIFO_DMA                                       0x00002508
#define NV_PFIFO_SIZE                                      0x0000250c
#define NV_PFIFO_CACH0_PSH0                                0x00003000
#define NV_PFIFO_CACH0_PUL0                                0x00003050
#define NV_PFIFO_CACH0_PUL1                                0x00003054
#define NV_PFIFO_CACH1_PSH0                                0x00003200
#define NV_PFIFO_CACH1_PSH1                                0x00003204
#define NV_PFIFO_CACH1_DMAPSH                              0x00003220
#define NV_PFIFO_CACH1_DMAF                                0x00003224
#    define NV_PFIFO_CACH1_DMAF_TRIG_8_BYTES               0x00000000
#    define NV_PFIFO_CACH1_DMAF_TRIG_16_BYTES              0x00000008
#    define NV_PFIFO_CACH1_DMAF_TRIG_24_BYTES              0x00000010
#    define NV_PFIFO_CACH1_DMAF_TRIG_32_BYTES              0x00000018
#    define NV_PFIFO_CACH1_DMAF_TRIG_40_BYTES              0x00000020
#    define NV_PFIFO_CACH1_DMAF_TRIG_48_BYTES              0x00000028
#    define NV_PFIFO_CACH1_DMAF_TRIG_56_BYTES              0x00000030
#    define NV_PFIFO_CACH1_DMAF_TRIG_64_BYTES              0x00000038
#    define NV_PFIFO_CACH1_DMAF_TRIG_72_BYTES              0x00000040
#    define NV_PFIFO_CACH1_DMAF_TRIG_80_BYTES              0x00000048
#    define NV_PFIFO_CACH1_DMAF_TRIG_88_BYTES              0x00000050
#    define NV_PFIFO_CACH1_DMAF_TRIG_96_BYTES              0x00000058
#    define NV_PFIFO_CACH1_DMAF_TRIG_104_BYTES             0x00000060
#    define NV_PFIFO_CACH1_DMAF_TRIG_112_BYTES             0x00000068
#    define NV_PFIFO_CACH1_DMAF_TRIG_120_BYTES             0x00000070
#    define NV_PFIFO_CACH1_DMAF_TRIG_128_BYTES             0x00000078
#    define NV_PFIFO_CACH1_DMAF_TRIG_136_BYTES             0x00000080
#    define NV_PFIFO_CACH1_DMAF_TRIG_144_BYTES             0x00000088
#    define NV_PFIFO_CACH1_DMAF_TRIG_152_BYTES             0x00000090
#    define NV_PFIFO_CACH1_DMAF_TRIG_160_BYTES             0x00000098
#    define NV_PFIFO_CACH1_DMAF_TRIG_168_BYTES             0x000000A0
#    define NV_PFIFO_CACH1_DMAF_TRIG_176_BYTES             0x000000A8
#    define NV_PFIFO_CACH1_DMAF_TRIG_184_BYTES             0x000000B0
#    define NV_PFIFO_CACH1_DMAF_TRIG_192_BYTES             0x000000B8
#    define NV_PFIFO_CACH1_DMAF_TRIG_200_BYTES             0x000000C0
#    define NV_PFIFO_CACH1_DMAF_TRIG_208_BYTES             0x000000C8
#    define NV_PFIFO_CACH1_DMAF_TRIG_216_BYTES             0x000000D0
#    define NV_PFIFO_CACH1_DMAF_TRIG_224_BYTES             0x000000D8
#    define NV_PFIFO_CACH1_DMAF_TRIG_232_BYTES             0x000000E0
#    define NV_PFIFO_CACH1_DMAF_TRIG_240_BYTES             0x000000E8
#    define NV_PFIFO_CACH1_DMAF_TRIG_248_BYTES             0x000000F0
#    define NV_PFIFO_CACH1_DMAF_TRIG_256_BYTES             0x000000F8
#    define NV_PFIFO_CACH1_DMAF_SIZE                       0x0000E000
#    define NV_PFIFO_CACH1_DMAF_SIZE_32_BYTES              0x00000000
#    define NV_PFIFO_CACH1_DMAF_SIZE_64_BYTES              0x00002000
#    define NV_PFIFO_CACH1_DMAF_SIZE_96_BYTES              0x00004000
#    define NV_PFIFO_CACH1_DMAF_SIZE_128_BYTES             0x00006000
#    define NV_PFIFO_CACH1_DMAF_SIZE_160_BYTES             0x00008000
#    define NV_PFIFO_CACH1_DMAF_SIZE_192_BYTES             0x0000A000
#    define NV_PFIFO_CACH1_DMAF_SIZE_224_BYTES             0x0000C000
#    define NV_PFIFO_CACH1_DMAF_SIZE_256_BYTES             0x0000E000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS                   0x001F0000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_0                 0x00000000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_1                 0x00010000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_2                 0x00020000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_3                 0x00030000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_4                 0x00040000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_5                 0x00050000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_6                 0x00060000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_7                 0x00070000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_8                 0x00080000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_9                 0x00090000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_10                0x000A0000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_11                0x000B0000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_12                0x000C0000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_13                0x000D0000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_14                0x000E0000
#    define NV_PFIFO_CACH1_DMAF_MAX_REQS_15                0x000F0000
#    define NV_PFIFO_CACH1_ENDIAN                          0x80000000
#    define NV_PFIFO_CACH1_LITTLE_ENDIAN                   0x7FFFFFFF
#    define NV_PFIFO_CACH1_BIG_ENDIAN                      0x80000000
#define NV_PFIFO_CACH1_DMAS                                0x00003228
#define NV_PFIFO_CACH1_DMAI                                0x0000322c
#define NV_PFIFO_CACH1_DMAC                                0x00003230
#define NV_PFIFO_CACH1_DMAP                                0x00003240
#define NV_PFIFO_CACH1_DMAG                                0x00003244
#define NV_PFIFO_CACH1_REF_CNT                             0x00003248
#define NV_PFIFO_CACH1_DMASR                               0x0000324C
#define NV_PFIFO_CACH1_PUL0                                0x00003250
#define NV_PFIFO_CACH1_PUL1                                0x00003254
#define NV_PFIFO_CACH1_HASH                                0x00003258
#define NV_PFIFO_CACH1_ACQUIRE_TIMEOUT                     0x00003260
#define NV_PFIFO_CACH1_ACQUIRE_TIMESTAMP                   0x00003264
#define NV_PFIFO_CACH1_ACQUIRE_VALUE                       0x00003268
#define NV_PFIFO_CACH1_SEMAPHORE                           0x0000326C
#define NV_PFIFO_CACH1_GET                                 0x00003270
#define NV_PFIFO_CACH1_ENG                                 0x00003280
#define NV_PFIFO_CACH1_DMA_DCOUNT                          0x000032A0
#define NV40_PFIFO_GRCTX_INSTANCE                          0x000032E0
#define NV40_PFIFO_UNK32E4                                 0x000032E4
#define NV_PFIFO_CACH1_METHOD(i)                   (0x00003800+(i*8))
#define NV_PFIFO_CACH1_DATA(i)                     (0x00003804+(i*8))
#define NV40_PFIFO_CACH1_METHOD(i)                 (0x00090000+(i*8))
#define NV40_PFIFO_CACH1_DATA(i)                   (0x00090004+(i*8))

#define NV_CRTC0_INTSTAT                                   0x00600100
#define NV_CRTC0_INTEN                                     0x00600140
#define NV_CRTC1_INTSTAT                                   0x00602100
#define NV_CRTC1_INTEN                                     0x00602140
#    define NV_CRTC_INTR_VBLANK                                (1<<0)

/* Fifo commands. These are not regs, neither masks */
#define NV03_FIFO_CMD_JUMP                                 0x20000000
#define NV03_FIFO_CMD_JUMP_OFFSET_MASK                     0x1ffffffc
#define NV03_FIFO_CMD_REWIND                               (NV03_FIFO_CMD_JUMP | (0 & NV03_FIFO_CMD_JUMP_OFFSET_MASK))

/* RAMFC offsets */
#define NV04_RAMFC_DMA_PUT                                       0x00
#define NV04_RAMFC_DMA_GET                                       0x04
#define NV04_RAMFC_DMA_INSTANCE                                  0x08
#define NV04_RAMFC_DMA_FETCH                                     0x10

#define NV10_RAMFC_DMA_PUT                                       0x00
#define NV10_RAMFC_DMA_GET                                       0x04
#define NV10_RAMFC_REF_CNT                                       0x08
#define NV10_RAMFC_DMA_INSTANCE                                  0x0C
#define NV10_RAMFC_DMA_STATE                                     0x10
#define NV10_RAMFC_DMA_FETCH                                     0x14
#define NV10_RAMFC_ENGINE                                        0x18
#define NV10_RAMFC_PULL1_ENGINE                                  0x1C
#define NV10_RAMFC_ACQUIRE_VALUE                                 0x20
#define NV10_RAMFC_ACQUIRE_TIMESTAMP                             0x24
#define NV10_RAMFC_ACQUIRE_TIMEOUT                               0x28
#define NV10_RAMFC_SEMAPHORE                                     0x2C
#define NV10_RAMFC_DMA_SUBROUTINE                                0x30

#define NV40_RAMFC_DMA_PUT                                       0x00
#define NV40_RAMFC_DMA_GET                                       0x04
#define NV40_RAMFC_REF_CNT                                       0x08
#define NV40_RAMFC_DMA_INSTANCE                                  0x0C
#define NV40_RAMFC_DMA_DCOUNT /* ? */                            0x10
#define NV40_RAMFC_DMA_STATE                                     0x14
#define NV40_RAMFC_DMA_FETCH                                     0x18
#define NV40_RAMFC_ENGINE                                        0x1C
#define NV40_RAMFC_PULL1_ENGINE                                  0x20
#define NV40_RAMFC_ACQUIRE_VALUE                                 0x24
#define NV40_RAMFC_ACQUIRE_TIMESTAMP                             0x28
#define NV40_RAMFC_ACQUIRE_TIMEOUT                               0x2C
#define NV40_RAMFC_SEMAPHORE                                     0x30
#define NV40_RAMFC_DMA_SUBROUTINE                                0x34
#define NV40_RAMFC_GRCTX_INSTANCE /* guess */                    0x38
#define NV40_RAMFC_DMA_TIMESLICE                                 0x3C
#define NV40_RAMFC_UNK_40                                        0x40
#define NV40_RAMFC_UNK_44                                        0x44
#define NV40_RAMFC_UNK_48                                        0x48
#define NV40_RAMFC_2088                                          0x4C
#define NV40_RAMFC_3300                                          0x50

