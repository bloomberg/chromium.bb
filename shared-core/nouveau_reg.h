

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

#define NV03_FIFO_SIZE                                     0x8000
#define NV_MAX_FIFO_NUMBER                                 32
#define NV03_FIFO_REGS_SIZE                                0x10000
#define NV03_FIFO_REGS(i)                                  (0x00800000+i*NV03_FIFO_REGS_SIZE)
#    define NV03_FIFO_REGS_DMAPUT(i)                       (NV03_FIFO_REGS(i)+0x40)
#    define NV03_FIFO_REGS_DMAGET(i)                       (NV03_FIFO_REGS(i)+0x44)

#define NV_PMC_INTSTAT                                     0x00000100
#    define NV_PMC_INTSTAT_PFIFO_PENDING                      (1<< 8)
#    define NV_PMC_INTSTAT_PGRAPH_PENDING                     (1<<12)
#    define NV_PMC_INTSTAT_CRTC0_PENDING                      (1<<24)
#    define NV_PMC_INTSTAT_CRTC1_PENDING                      (1<<25)
#    define NV_PMC_INTSTAT_CRTCn_PENDING                      (3<<24)
#define NV_PMC_INTEN                                       0x00000140
#    define NV_PMC_INTEN_MASTER_ENABLE                        (1<< 0)

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
#define NV_PGRAPH_FIFO                                     0x00400720
#define NV_PGRAPH_FFINTFC_ST2                              0x00400764

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
#define NV_PFIFO_CACH1_PUL0                                0x00003250
#define NV_PFIFO_CACH1_PUL1                                0x00003254
#define NV_PFIFO_CACH1_HASH                                0x00003258
#define NV_PFIFO_CACH1_ENG                                 0x00003280

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
#define NV40_RAMFC_GRCTX_INSTANCE_32E0 /* guess */               0x38
#define NV40_RAMFC_DMA_TIMESLICE                                 0x3C
#define NV40_RAMFC_UNK_40                                        0x40
#define NV40_RAMFC_UNK_44                                        0x44
#define NV40_RAMFC_UNK_48                                        0x48
#define NV40_RAMFC_2088                                          0x4C
#define NV40_RAMFC_3300                                          0x50

