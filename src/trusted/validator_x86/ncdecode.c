/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * ncdecode.c - table driven decoder for Native Client
 *
 * Most x86 decoders I've looked at are big case statements. While
 * this organization is fairly transparent and obvious, it tends to
 * lead to messy control flow (gotos, etc.) that make the decoder
 * more complicated, hence harder to maintain and harder to validate.
 *
 * This decoder is table driven, which will hopefully result in
 * substantially less code. Although the code+tables may be more
 * lines of code than a decoder built around a switch statement,
 * the smaller amount of actual procedural code and the regular
 * structure of the tables should make it easier to understand,
 * debug, and easier to become confident the decoder is correct.
 *
 * As it is specialized to Native Client, this decoder can also
 * benefit from any exclusions or simplifications we decide to
 * make in the dialect of x86 machine code accepted by Native
 * Client. Any such simplifications should ultimately be easily
 * recognized by inspection of the decoder configuration tables.
 * ALSO, the decoder mostly needs to worry about accurate
 * instruction lengths and finding opcodes. It does not need
 * to completely resolve the operands of all instructions.
 */

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Turn this flag on, so that we can print out human readable
 * names for type NaclInsttype.
 */
#define NEEDSNACLINSTTYPESTRING

#include <stdio.h>
#include <assert.h>
#include "native_client/src/trusted/validator_x86/ncdecode.h"
#include "gen/native_client/src/trusted/validator_x86/ncdecodetab.h"

/* Generates a print name for the given NCDecodeImmediateType. */
static const char* NCDecodeImmediateTypeName(NCDecodeImmediateType type) {
  switch(type) {
    case IMM_UNKNOWN: return "IMM_UNKNOWN";
    case IMM_NONE: return "IMM_NONE";
    case IMM_FIXED1: return "IMM_FIXED1";
    case IMM_FIXED2: return "IMM_FIXED2";
    case IMM_FIXED3: return "IMM_FIXED3";
    case IMM_FIXED4: return "IMM_FIXED4";
    case IMM_DATAV: return "IMM_DATAV";
    case IMM_ADDRV: return "IMM_ADDRV";
    case IMM_GROUP3_F6: return "IMM_GROUP3_F6";
    case IMM_GROUP3_F7: return "IMM_GROUP3_F7";
    case IMM_FARPTR: return "IMM_FARPTR";
    case IMM_MOV_DATAV: return "IMM_MOV_DATAV";
    default: assert(0);
  }

  /* NOTREACHED */
  return NULL;
}

/* Prints out the contents of the given OpInfo. */
static void PrintOpInfo(const struct OpInfo* info) {
  printf("opinfo(%s, hasmrm=%u, immtype=%s, opinmrm=%d)\n",
         NaClInstTypeString(info->insttype),
         info->hasmrmbyte,
         NCDecodeImmediateTypeName(info->immtype),
         info->opinmrm);
}

/* later this will make decoding x87 instructions a bit more concise. */
static const struct OpInfo* kDecodeX87Op[8] = { kDecode87D8,
                                                kDecode87D9,
                                                kDecode87DA,
                                                kDecode87DB,
                                                kDecode87DC,
                                                kDecode87DD,
                                                kDecode87DE,
                                                kDecode87DF };

static void NullDecoderAction(const struct NCDecoderState* mstate) {
  UNREFERENCED_PARAMETER(mstate);
}
static void NullDecoderStats(struct NCValidatorState* vstate) {
  UNREFERENCED_PARAMETER(vstate);
}
static void DefaultInternalError(struct NCValidatorState* vstate) {
  UNREFERENCED_PARAMETER(vstate);
}

/* TODO(bradchen): Thread safety? */
/* TODO(bradchen): More comments */
NCDecoderAction g_DecoderAction = NullDecoderAction;
NCDecoderStats g_NewSegment = NullDecoderStats;
NCDecoderStats g_InternalError = DefaultInternalError;
NCDecoderStats g_SegFault = NullDecoderStats;

/* Error Condition Handling */
static void ErrorSegmentation(struct NCValidatorState* vstate) {
  fprintf(stdout, "ErrorSegmentation\n");
  /* When the decoder is used by the NaCl validator    */
  /* the validator provides an error handler that does */
  /* the necessary bookeeping to track these errors.   */
  g_SegFault(vstate);
}

static void ErrorInternal(struct NCValidatorState* vstate) {
  fprintf(stdout, "ErrorInternal\n");
  /* When the decoder is used by the NaCl validator    */
  /* the validator provides an error handler that does */
  /* the necessary bookeeping to track these errors.   */
  g_InternalError(vstate);
}

void InitDecoder(struct NCDecoderState* mstate) {
  mstate->inst.vaddr = mstate->vpc;
  mstate->inst.maddr = mstate->mpc;
  mstate->inst.prefixbytes = 0;
  mstate->inst.prefixmask = 0;
  mstate->inst.hasopbyte2 = 0;
  mstate->inst.hasopbyte3 = 0;
  mstate->inst.hassibbyte = 0;
  mstate->inst.mrm = 0;
  mstate->inst.immtype = IMM_UNKNOWN;
  mstate->inst.dispbytes = 0;
  mstate->inst.length = 0;
  mstate->inst.rexprefix = 0;
  mstate->opinfo = NULL;
}

/* Returns the number of bytes defined for the operand of the instruction. */
static int ExtractOperandSize(NCDecoderState* mstate) {
  if (NACL_TARGET_SUBARCH == 64 &&
      mstate->inst.rexprefix && mstate->inst.rexprefix & 0x8) {
    return 8;
  }
  if (mstate->inst.prefixmask & kPrefixDATA16) {
    return 2;
  }
  return 4;
}

/* at most four prefix bytes are allowed */
void ConsumePrefixBytes(struct NCDecoderState* mstate) {
  uint8_t nb;
  int ii;
  uint32_t prefix_form;

  for (ii = 0; ii < kMaxPrefixBytes; ++ii) {
    nb = *mstate->nextbyte;
    prefix_form = kPrefixTable[nb];
    if (prefix_form == 0) return;
    DEBUG( printf("Consume prefix[%d]: %02x => %x\n", ii, nb, prefix_form) );
    mstate->inst.prefixmask |= prefix_form;
    mstate->inst.prefixmask |= kPrefixTable[nb];
    mstate->inst.prefixbytes += 1;
    mstate->nextbyte += 1;
    DEBUG( printf("  prefix mask: %08x\n", mstate->inst.prefixmask) );
    if (NACL_TARGET_SUBARCH == 64 && prefix_form == kPrefixREX) {
      mstate->inst.rexprefix = nb;
      /* REX prefix must be last prefix. */
      return;
    }
  }
}

static const struct OpInfo *GetExtendedOpInfo(struct NCDecoderState* mstate,
                                              uint8_t opbyte2) {
  uint32_t pm;
  pm = mstate->inst.prefixmask;
  if ((pm & (kPrefixDATA16 | kPrefixREPNE | kPrefixREP)) == 0) {
    return &kDecode0FXXOp[opbyte2];
  } else if (pm & kPrefixDATA16) {
    return &kDecode660FXXOp[opbyte2];
  } else if (pm & kPrefixREPNE) {
    return &kDecodeF20FXXOp[opbyte2];
  } else if (pm & kPrefixREP) {
    return &kDecodeF30FXXOp[opbyte2];
  }
  ErrorInternal(mstate->vstate);
  return mstate->opinfo;
}

static void GetX87OpInfo(struct NCDecoderState* mstate) {
  /* WAIT is an x87 instruction but not in the coproc opcode space. */
  const uint8_t kWAITOp = 0x9b;
  uint8_t kFirstX87Opcode = 0xd8;
  uint8_t kLastX87Opcode = 0xdf;
  uint8_t op1 = mstate->inst.maddr[mstate->inst.prefixbytes];
  if (op1 < kFirstX87Opcode || op1 > kLastX87Opcode) {
    if (op1 != kWAITOp) ErrorInternal(mstate->vstate);
    return;
  }
  mstate->opinfo = &kDecodeX87Op[op1 - kFirstX87Opcode][mstate->inst.mrm];
  DEBUG( printf("NACL_X87 op1 = %02x, ", op1);
         PrintOpInfo(mstate->opinfo) );
}

void ConsumeOpcodeBytes(struct NCDecoderState* mstate) {
  uint8_t opcode = *mstate->nextbyte;
  mstate->opinfo = &kDecode1ByteOp[opcode];
  DEBUG( printf("NACLi_1BYTE: opcode = %02x, ", opcode);
         PrintOpInfo(mstate->opinfo) );
  mstate->nextbyte += 1;
  if (opcode == kTwoByteOpcodeByte1) {
    uint8_t opcode2 = *mstate->nextbyte;
    mstate->opinfo = GetExtendedOpInfo(mstate, opcode2);
    DEBUG( printf("NACLi_2BYTE: opcode2 = %02x, ", opcode2);
           PrintOpInfo(mstate->opinfo) );
    mstate->inst.hasopbyte2 = 1;
    mstate->nextbyte += 1;
    if (mstate->opinfo->insttype == NACLi_3BYTE) {
      uint8_t opcode3 = *mstate->nextbyte;
      uint32_t pm;
      pm = mstate->inst.prefixmask;
      mstate->nextbyte += 1;
      mstate->inst.hasopbyte3 = 1;

      DEBUG( printf("NACLi_3BYTE: opcode3 = %02x, ", opcode3) );
      switch (opcode2) {
      case 0x38:        /* SSSE3, SSE4 */
        if (pm & kPrefixDATA16) {
          mstate->opinfo = &kDecode660F38Op[opcode3];
        } else if (pm & kPrefixREPNE) {
          mstate->opinfo = &kDecodeF20F38Op[opcode3];
        } else if (pm == 0) {
          mstate->opinfo = &kDecode0F38Op[opcode3];
        } else {
          /* Other prefixes like F3 cause an undefined instruction error. */
          /* Note from decoder table that NACLi_3BYTE is only used with   */
          /* data16 and repne prefixes.                                   */
          ErrorInternal(mstate->vstate);
        }
        break;
      case 0x3A:        /* SSSE3, SSE4 */
        if (pm & kPrefixDATA16) {
          mstate->opinfo = &kDecode660F3AOp[opcode3];
        } else if (pm == 0) {
          mstate->opinfo = &kDecode0F3AOp[opcode3];
        } else {
          /* Other prefixes like F3 cause an undefined instruction error. */
          /* Note from decoder table that NACLi_3BYTE is only used with   */
          /* data16 and repne prefixes.                                   */
          ErrorInternal(mstate->vstate);
        }
        break;
      default:
        /* if this happens there is a decoding table bug */
        ErrorInternal(mstate->vstate);
        break;
      }
      DEBUG( PrintOpInfo(mstate->opinfo) );
    }
  }
  mstate->inst.immtype = mstate->opinfo->immtype;
}

void ConsumeModRM(struct NCDecoderState* mstate) {
  if (mstate->opinfo->hasmrmbyte != 0) {
    const uint8_t mrm = *mstate->nextbyte;
    DEBUG( printf("Mod/RM byte: %02x\n", mrm) );
    mstate->inst.mrm = mrm;
    mstate->nextbyte += 1;
    if (mstate->opinfo->insttype == NACLi_X87) {
      GetX87OpInfo(mstate);
    }
    if (mstate->opinfo->opinmrm) {
      const struct OpInfo *mopinfo =
        &kDecodeModRMOp[mstate->opinfo->opinmrm][modrm_opcode(mrm)];
      mstate->opinfo = mopinfo;
      DEBUG( printf("NACLi_opinmrm: modrm.opcode = %x, ", modrm_opcode(mrm));
             PrintOpInfo(mstate->opinfo) );
      if (mstate->inst.immtype == IMM_UNKNOWN) {
        assert(0);
        mstate->inst.immtype = mopinfo->immtype;
      }
      /* handle weird case for 0xff TEST Ib/Iv */
      if (modrm_opcode(mrm) == 0) {
        if (mstate->inst.immtype == IMM_GROUP3_F6) {
          mstate->inst.immtype = IMM_FIXED1;
        }
        if (mstate->inst.immtype == IMM_GROUP3_F7) {
          mstate->inst.immtype = IMM_DATAV;
        }
      }
      DEBUG( printf("  immtype = %s\n",
                    NCDecodeImmediateTypeName(mstate->inst.immtype)) );
    }
    if (mstate->inst.prefixmask & kPrefixADDR16) {
      switch (modrm_mod(mrm)) {
        case 0:
          if (modrm_rm(mrm) == 0x06) mstate->inst.dispbytes = 2;  /* disp16 */
          else mstate->inst.dispbytes = 0;
          break;
        case 1:
          mstate->inst.dispbytes = 1;           /* disp8 */
          break;
        case 2:
          mstate->inst.dispbytes = 2;           /* disp16 */
          break;
        case 3:
          mstate->inst.dispbytes = 0;           /* no disp */
          break;
        default:
          ErrorInternal(mstate->vstate);
      }
      mstate->inst.hassibbyte = 0;
    } else {
      switch (modrm_mod(mrm)) {
        case 0:
          if (modrm_rm(mrm) == 0x05) mstate->inst.dispbytes = 4;  /* disp32 */
          else mstate->inst.dispbytes = 0;
          break;
        case 1:
          mstate->inst.dispbytes = 1;           /* disp8 */
          break;
        case 2:
          mstate->inst.dispbytes = 4;           /* disp32 */
          break;
        case 3:
          mstate->inst.dispbytes = 0;           /* no disp */
          break;
        default:
          ErrorInternal(mstate->vstate);
      }
      mstate->inst.hassibbyte = ((modrm_rm(mrm) == 0x04) &&
                                 (modrm_mod(mrm) != 3));
    }
  }
  DEBUG( printf("  dispbytes = %d, hasibbyte = %d\n",
                mstate->inst.dispbytes, mstate->inst.hassibbyte) );
}

void ConsumeSIB(struct NCDecoderState* mstate) {
  if (mstate->inst.hassibbyte != 0) {
    const uint8_t sib = *mstate->nextbyte;
    mstate->nextbyte += 1;
    if (sib_base(sib) == 0x05) {
      switch (modrm_mod(mstate->inst.mrm)) {
      case 0: mstate->inst.dispbytes = 4; break;
      case 1: mstate->inst.dispbytes = 1; break;
      case 2: mstate->inst.dispbytes = 4; break;
      case 3:
      default:
        ErrorInternal(mstate->vstate);
      }
    }
    DEBUG( printf("sib byte: %02x, dispbytes = %d\n",
                  sib, mstate->inst.dispbytes) );
  }
}

void ConsumeID(struct NCDecoderState* mstate) {
  uint8_t* old_next_byte = mstate->nextbyte;
  ssize_t inst_length;
  if (mstate->inst.immtype == IMM_UNKNOWN) {
    ErrorInternal(mstate->vstate);
  }
  /* NOTE: NaCl allows at most one prefix byte (for 32-bit mode) */
  if (mstate->inst.immtype == IMM_MOV_DATAV) {
    mstate->nextbyte += ExtractOperandSize(mstate);
  } else if (mstate->inst.prefixmask & kPrefixDATA16) {
    mstate->nextbyte += kImmTypeToSize66[mstate->inst.immtype];
  } else if (mstate->inst.prefixmask & kPrefixADDR16) {
    mstate->nextbyte += kImmTypeToSize67[mstate->inst.immtype];
  } else {
    mstate->nextbyte += kImmTypeToSize[mstate->inst.immtype];
  }
  mstate->nextbyte += mstate->inst.dispbytes;
  inst_length = mstate->nextbyte - mstate->mpc;
  mstate->inst.length = (uint8_t)(inst_length & 0xff);
  if ((ssize_t)mstate->inst.length != inst_length) {
    printf("Unexpected instruction length\n");
    ErrorInternal(mstate->vstate);
  }
  DEBUG( printf("ID: consume %"NACL_PRIxPTR" bytes\n",
                (intptr_t)( mstate->nextbyte - old_next_byte)) );
}

/* Actually this routine is special for 3DNow instructions */
void MaybeGet3ByteOpInfo(struct NCDecoderState* mstate) {
  if (mstate->opinfo->insttype == NACLi_3DNOW) {
    uint8_t opbyte1 = mstate->mpc[mstate->inst.prefixbytes];
    uint8_t opbyte2 = mstate->mpc[mstate->inst.prefixbytes + 1];
    uint8_t immbyte = mstate->mpc[mstate->inst.length - 1];
    if (opbyte1 == kTwoByteOpcodeByte1 &&
        opbyte2 == k3DNowOpcodeByte2) {
      mstate->opinfo = &kDecode0F0FOp[immbyte];
      DEBUG( printf(
                 "NACLi_3DNOW: byte1 = %02x, byte2 = %02x, immbyte = %02x,\n  ",
                 opbyte1, opbyte2, immbyte);
             PrintOpInfo(mstate->opinfo) );
    }
  }
}

void NCDecodeRegisterCallbacks(NCDecoderAction decoderaction,
                               NCDecoderStats newsegment,
                               NCDecoderStats segfault,
                               NCDecoderStats internalerror) {
  if (decoderaction != NULL) g_DecoderAction = decoderaction;
  if (newsegment != NULL) g_NewSegment = newsegment;
  if (segfault != NULL) g_SegFault = segfault;
  if (internalerror != NULL) g_InternalError = internalerror;
}

struct NCDecoderState *PreviousInst(const struct NCDecoderState* mstate,
                                    int nindex) {
  int index = (mstate->dbindex + nindex + kDecodeBufferSize)
      & (kDecodeBufferSize - 1);
  return &mstate->decodebuffer[index];
}

/* The actual decoder */
void NCDecodeSegment(uint8_t *mbase, PcAddress vbase, MemorySize size,
                     struct NCValidatorState* vstate) {
  const PcAddress vlimit = vbase + size;
  struct NCDecoderState decodebuffer[kDecodeBufferSize];
  struct NCDecoderState *mstate;
  int dbindex;
  for (dbindex = 0; dbindex < kDecodeBufferSize; ++dbindex) {
    decodebuffer[dbindex].vstate = vstate;
    decodebuffer[dbindex].decodebuffer = decodebuffer;
    decodebuffer[dbindex].dbindex = dbindex;
    decodebuffer[dbindex].inst.length = 0;  /* indicates no instruction */
    decodebuffer[dbindex].vpc = 0;
    decodebuffer[dbindex].mpc = 0;
  }
  mstate = &decodebuffer[0];
  mstate->mpc = (uint8_t *)mbase;
  mstate->nextbyte = mbase;
  mstate->vpc = vbase;

  DEBUG( printf("DecodeSegment(%"NACL_PRIxPcAddress"-%"NACL_PRIxPcAddress")\n",
                vbase, vlimit) );
  g_NewSegment(mstate->vstate);
  while (mstate->vpc < vlimit) {
    PcAddress newpc;
    DEBUG( printf("Decoding instruction at %"NACL_PRIxPcAddress":\n",
                  mstate->vpc) );
    InitDecoder(mstate);
    ConsumePrefixBytes(mstate);
    ConsumeOpcodeBytes(mstate);
    ConsumeModRM(mstate);
    ConsumeSIB(mstate);
    ConsumeID(mstate);
    MaybeGet3ByteOpInfo(mstate);
    /* now scrutinize this instruction */
    newpc = mstate->vpc + mstate->inst.length;
    DEBUG( printf("new pc = %"NACL_PRIxPcAddress"\n", newpc) );
    if (newpc > vlimit) {
      fprintf(stdout, "%"NACL_PRIxPcAddress" > %"NACL_PRIxPcAddress"\n",
              newpc, vlimit);
      ErrorSegmentation(vstate);
      break;
    }
    g_DecoderAction(mstate);
    /* get read for next round */
    dbindex = (dbindex + 1) & (kDecodeBufferSize - 1);
    decodebuffer[dbindex].vpc = newpc;
    decodebuffer[dbindex].mpc = mstate->mpc + mstate->inst.length;
    decodebuffer[dbindex].nextbyte = mstate->nextbyte;
    mstate = &decodebuffer[dbindex];
  }
}
