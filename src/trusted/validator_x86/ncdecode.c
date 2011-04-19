/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
 * names for type NaClInsttype.
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

static void NullDecoderAction(const struct NCDecoderInst* dinst) {
  UNREFERENCED_PARAMETER(dinst);
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
static void ErrorSegmentation(NCDecoderInst* dinst) {
  NCDecoderState* dstate = dinst->dstate;
  fprintf(stdout, "ErrorSegmentation\n");
  /* When the decoder is used by the NaCl validator    */
  /* the validator provides an error handler that does */
  /* the necessary bookeeping to track these errors.   */
  dstate->segmentation_error_fn(dstate->vstate);
}

static void ErrorInternal(NCDecoderInst* dinst) {
  NCDecoderState* dstate = dinst->dstate;
  fprintf(stdout, "ErrorInternal\n");
  /* When the decoder is used by the NaCl validator    */
  /* the validator provides an error handler that does */
  /* the necessary bookeeping to track these errors.   */
  dstate->internal_error_fn(dstate->vstate);
}

/* Defines how to handle errors found while parsing the memory segment. */
static void NCRemainingMemoryInternalError(NCRemainingMemoryError error,
                                           struct NCRemainingMemory* memory) {
  /* Don't do anything for memory overflow! Let NCDecodeSegment generate
   * the corresponding segmentation error. This allows us to back out overflow
   * if a predefined nop is matched.
   */
  if (NCRemainingMemoryOverflow != error) {
    NCDecoderState* dstate = (NCDecoderState*) memory->error_fn_state;
    NCRemainingMemoryReportError(error, memory);
    ErrorInternal(&dstate->inst_buffer[dstate->cur_inst_index]);
  }
}

void InitDecoder(struct NCDecoderInst* dinst) {
  NCInstBytesInit(&dinst->inst.bytes);
  dinst->inst.prefixbytes = 0;
  dinst->inst.prefixmask = 0;
  dinst->inst.num_opbytes = 1;  /* unless proven otherwise. */
  dinst->inst.hassibbyte = 0;
  dinst->inst.mrm = 0;
  dinst->inst.immtype = IMM_UNKNOWN;
  dinst->inst.immbytes = 0;
  dinst->inst.dispbytes = 0;
  dinst->inst.rexprefix = 0;
  dinst->opinfo = NULL;
}

/* Returns the number of bytes defined for the operand of the instruction. */
static int ExtractOperandSize(NCDecoderInst* dinst) {
  if (NACL_TARGET_SUBARCH == 64 &&
      dinst->inst.rexprefix && dinst->inst.rexprefix & 0x8) {
    return 8;
  }
  if (dinst->inst.prefixmask & kPrefixDATA16) {
    return 2;
  }
  return 4;
}

/* at most four prefix bytes are allowed */
void ConsumePrefixBytes(struct NCDecoderInst* dinst) {
  uint8_t nb;
  int ii;
  uint32_t prefix_form;

  for (ii = 0; ii < kMaxPrefixBytes; ++ii) {
    nb = NCRemainingMemoryGetNext(&dinst->dstate->memory);
    prefix_form = kPrefixTable[nb];
    if (prefix_form == 0) return;
    DEBUG( printf("Consume prefix[%d]: %02x => %x\n", ii, nb, prefix_form) );
    dinst->inst.prefixmask |= prefix_form;
    dinst->inst.prefixmask |= kPrefixTable[nb];
    dinst->inst.prefixbytes += 1;
    NCInstBytesRead(&dinst->inst.bytes);
    DEBUG( printf("  prefix mask: %08x\n", dinst->inst.prefixmask) );
    if (NACL_TARGET_SUBARCH == 64 && prefix_form == kPrefixREX) {
      dinst->inst.rexprefix = nb;
      /* REX prefix must be last prefix. */
      return;
    }
  }
}

static const struct OpInfo* GetExtendedOpInfo(NCDecoderInst* dinst,
                                              uint8_t opbyte2) {
  uint32_t pm;
  pm = dinst->inst.prefixmask;
  if ((pm & (kPrefixDATA16 | kPrefixREPNE | kPrefixREP)) == 0) {
    return &kDecode0FXXOp[opbyte2];
  } else if (pm & kPrefixDATA16) {
    return &kDecode660FXXOp[opbyte2];
  } else if (pm & kPrefixREPNE) {
    return &kDecodeF20FXXOp[opbyte2];
  } else if (pm & kPrefixREP) {
    return &kDecodeF30FXXOp[opbyte2];
  }
  ErrorInternal(dinst);
  return dinst->opinfo;
}

static void GetX87OpInfo(NCDecoderInst* dinst) {
  /* WAIT is an x87 instruction but not in the coproc opcode space. */
  uint8_t op1 = NCInstBytesByte(&dinst->inst_bytes, dinst->inst.prefixbytes);
  if (op1 < kFirstX87Opcode || op1 > kLastX87Opcode) {
    if (op1 != kWAITOp) ErrorInternal(dinst);
    return;
  }
  dinst->opinfo = &kDecodeX87Op[op1 - kFirstX87Opcode][dinst->inst.mrm];
  DEBUG( printf("NACL_X87 op1 = %02x, ", op1);
         PrintOpInfo(dinst->opinfo) );
}

void ConsumeOpcodeBytes(NCDecoderInst* dinst) {
  uint8_t opcode = NCInstBytesRead(&dinst->inst.bytes);
  dinst->opinfo = &kDecode1ByteOp[opcode];
  DEBUG( printf("NACLi_1BYTE: opcode = %02x, ", opcode);
         PrintOpInfo(dinst->opinfo) );
  if (opcode == kTwoByteOpcodeByte1) {
    uint8_t opcode2 = NCInstBytesRead(&dinst->inst.bytes);
    dinst->opinfo = GetExtendedOpInfo(dinst, opcode2);
    DEBUG( printf("NACLi_2BYTE: opcode2 = %02x, ", opcode2);
           PrintOpInfo(dinst->opinfo) );
    dinst->inst.num_opbytes = 2;
    if (dinst->opinfo->insttype == NACLi_3BYTE) {
      uint8_t opcode3 = NCInstBytesRead(&dinst->inst.bytes);
      uint32_t pm;
      pm = dinst->inst.prefixmask;
      dinst->inst.num_opbytes = 3;

      DEBUG( printf("NACLi_3BYTE: opcode3 = %02x, ", opcode3) );
      switch (opcode2) {
      case 0x38:        /* SSSE3, SSE4 */
        if (pm & kPrefixDATA16) {
          dinst->opinfo = &kDecode660F38Op[opcode3];
        } else if (pm & kPrefixREPNE) {
          dinst->opinfo = &kDecodeF20F38Op[opcode3];
        } else if (pm == 0) {
          dinst->opinfo = &kDecode0F38Op[opcode3];
        } else {
          /* Other prefixes like F3 cause an undefined instruction error. */
          /* Note from decoder table that NACLi_3BYTE is only used with   */
          /* data16 and repne prefixes.                                   */
          ErrorInternal(dinst);
        }
        break;
      case 0x3A:        /* SSSE3, SSE4 */
        if (pm & kPrefixDATA16) {
          dinst->opinfo = &kDecode660F3AOp[opcode3];
        } else if (pm == 0) {
          dinst->opinfo = &kDecode0F3AOp[opcode3];
        } else {
          /* Other prefixes like F3 cause an undefined instruction error. */
          /* Note from decoder table that NACLi_3BYTE is only used with   */
          /* data16 and repne prefixes.                                   */
          ErrorInternal(dinst);
        }
        break;
      default:
        /* if this happens there is a decoding table bug */
        ErrorInternal(dinst);
        break;
      }
      DEBUG( PrintOpInfo(dinst->opinfo) );
    }
  }
  dinst->inst.immtype = dinst->opinfo->immtype;
}

void ConsumeModRM(NCDecoderInst* dinst) {
  if (dinst->opinfo->hasmrmbyte != 0) {
    const uint8_t mrm = NCInstBytesRead(&dinst->inst.bytes);
    DEBUG( printf("Mod/RM byte: %02x\n", mrm) );
    dinst->inst.mrm = mrm;
    if (dinst->opinfo->insttype == NACLi_X87) {
      GetX87OpInfo(dinst);
    }
    if (dinst->opinfo->opinmrm) {
      const struct OpInfo* mopinfo =
        &kDecodeModRMOp[dinst->opinfo->opinmrm][modrm_opcode(mrm)];
      dinst->opinfo = mopinfo;
      DEBUG( printf("NACLi_opinmrm: modrm.opcode = %x, ", modrm_opcode(mrm));
             PrintOpInfo(dinst->opinfo) );
      if (dinst->inst.immtype == IMM_UNKNOWN) {
        assert(0);
        dinst->inst.immtype = mopinfo->immtype;
      }
      /* handle weird case for 0xff TEST Ib/Iv */
      if (modrm_opcode(mrm) == 0) {
        if (dinst->inst.immtype == IMM_GROUP3_F6) {
          dinst->inst.immtype = IMM_FIXED1;
        }
        if (dinst->inst.immtype == IMM_GROUP3_F7) {
          dinst->inst.immtype = IMM_DATAV;
        }
      }
      DEBUG( printf("  immtype = %s\n",
                    NCDecodeImmediateTypeName(dinst->inst.immtype)) );
    }
    if (dinst->inst.prefixmask & kPrefixADDR16) {
      switch (modrm_mod(mrm)) {
        case 0:
          if (modrm_rm(mrm) == 0x06) dinst->inst.dispbytes = 2;  /* disp16 */
          else dinst->inst.dispbytes = 0;
          break;
        case 1:
          dinst->inst.dispbytes = 1;           /* disp8 */
          break;
        case 2:
          dinst->inst.dispbytes = 2;           /* disp16 */
          break;
        case 3:
          dinst->inst.dispbytes = 0;           /* no disp */
          break;
        default:
          ErrorInternal(dinst);
      }
      dinst->inst.hassibbyte = 0;
    } else {
      switch (modrm_mod(mrm)) {
        case 0:
          if (modrm_rm(mrm) == 0x05) dinst->inst.dispbytes = 4;  /* disp32 */
          else dinst->inst.dispbytes = 0;
          break;
        case 1:
          dinst->inst.dispbytes = 1;           /* disp8 */
          break;
        case 2:
          dinst->inst.dispbytes = 4;           /* disp32 */
          break;
        case 3:
          dinst->inst.dispbytes = 0;           /* no disp */
          break;
        default:
          ErrorInternal(dinst);
      }
      dinst->inst.hassibbyte = ((modrm_rm(mrm) == 0x04) &&
                                 (modrm_mod(mrm) != 3));
    }
  }
  DEBUG( printf("  dispbytes = %d, hasibbyte = %d\n",
                dinst->inst.dispbytes, dinst->inst.hassibbyte) );
}

void ConsumeSIB(NCDecoderInst* dinst) {
  if (dinst->inst.hassibbyte != 0) {
    const uint8_t sib = NCInstBytesRead(&dinst->inst.bytes);
    if (sib_base(sib) == 0x05) {
      switch (modrm_mod(dinst->inst.mrm)) {
      case 0: dinst->inst.dispbytes = 4; break;
      case 1: dinst->inst.dispbytes = 1; break;
      case 2: dinst->inst.dispbytes = 4; break;
      case 3:
      default:
        ErrorInternal(dinst);
      }
    }
    DEBUG( printf("sib byte: %02x, dispbytes = %d\n",
                  sib, dinst->inst.dispbytes) );
  }
}

void ConsumeID(NCDecoderInst* dinst) {
  if (dinst->inst.immtype == IMM_UNKNOWN) {
    ErrorInternal(dinst);
  }
  /* NOTE: NaCl allows at most one prefix byte (for 32-bit mode) */
  if (dinst->inst.immtype == IMM_MOV_DATAV) {
    dinst->inst.immbytes = ExtractOperandSize(dinst);
  } else if (dinst->inst.prefixmask & kPrefixDATA16) {
    dinst->inst.immbytes = kImmTypeToSize66[dinst->inst.immtype];
  } else if (dinst->inst.prefixmask & kPrefixADDR16) {
    dinst->inst.immbytes = kImmTypeToSize67[dinst->inst.immtype];
  } else {
    dinst->inst.immbytes = kImmTypeToSize[dinst->inst.immtype];
  }
  NCInstBytesReadBytes((ssize_t) dinst->inst.immbytes,
                       &dinst->inst.bytes);
  NCInstBytesReadBytes((ssize_t) dinst->inst.dispbytes,
                       &dinst->inst.bytes);
  DEBUG(printf("ID: %d disp bytes, %d imm bytes\n",
               dinst->inst.dispbytes, dinst->inst.immbytes));
}

/* Actually this routine is special for 3DNow instructions */
void MaybeGet3ByteOpInfo(NCDecoderInst* dinst) {
  if (dinst->opinfo->insttype == NACLi_3DNOW) {
    uint8_t opbyte1 = NCInstBytesByte(&dinst->inst_bytes,
                                      dinst->inst.prefixbytes);
    uint8_t opbyte2 = NCInstBytesByte(&dinst->inst_bytes,
                                      dinst->inst.prefixbytes + 1);
    if (opbyte1 == kTwoByteOpcodeByte1 &&
        opbyte2 == k3DNowOpcodeByte2) {
      uint8_t immbyte =
          NCInstBytesByte(&dinst->inst_bytes, dinst->inst.bytes.length - 1);
      dinst->opinfo = &kDecode0F0FOp[immbyte];
      DEBUG( printf(
                 "NACLi_3DNOW: byte1 = %02x, byte2 = %02x, immbyte = %02x,\n  ",
                 opbyte1, opbyte2, immbyte);
             PrintOpInfo(dinst->opinfo) );
    }
  }
}

void NCDecodeRegisterCallbacks(NCDecoderAction decoderaction,
                               NCDecoderStats newsegment,
                               NCDecoderStats segfault,
                               NCDecoderStats internalerror) {
  /* Clear old definitions before continuing. */
  g_DecoderAction = NullDecoderAction;
  g_NewSegment = NullDecoderStats;
  g_InternalError = DefaultInternalError;
  g_SegFault = NullDecoderStats;
  if (decoderaction != NULL) g_DecoderAction = decoderaction;
  if (newsegment != NULL) g_NewSegment = newsegment;
  if (segfault != NULL) g_SegFault = segfault;
  if (internalerror != NULL) g_InternalError = internalerror;
}

struct NCDecoderInst* PreviousInst(const NCDecoderInst* dinst,
                                   int nindex) {
  /* Note: This code also handles increments, so that we can
   * use the same code for both.
   */
  int index = (dinst->inst_index + nindex + kDecodeInstBufferSize)
      & (kDecodeInstBufferSize - 1);
  return &dinst->dstate->inst_buffer[index];
}

/* Initialize the ring buffer used to store decoded instructions. Returns
 * The address of the initial instruction to be decoded.
 *
 * mbase: The actual address in memory of the instructions being iterated.
 * vbase: The virtual address instructions will be executed from.
 * decodebuffer: Ring buffer containing kDecodeBufferSize elements (output)
 * mstate: Current instruction pointer into the ring buffer (output)
 */
static NCDecoderInst* InitDecoderState(uint8_t* mbase, NaClPcAddress vbase,
                                       NaClMemorySize size,
                                       struct NCValidatorState* vstate,
                                       NCDecoderState* dstate,
                                       NCDecoderStats segmentationerror,
                                       NCDecoderStats internalerror) {
  int dbindex;
  NCRemainingMemory* memory = &dstate->memory;
  NCDecoderInst* inst_buffer = dstate->inst_buffer;
  NCDecoderInst* cur_inst = &inst_buffer[0];
  dstate->segmentation_error_fn = segmentationerror;
  dstate->internal_error_fn = internalerror;
  dstate->cur_inst_index = 0;
  NCRemainingMemoryInit(mbase, size, memory);
  memory->error_fn = NCRemainingMemoryInternalError;
  memory->error_fn_state = (void*) dstate;
  dstate->vstate = vstate;
  for (dbindex = 0; dbindex < kDecodeInstBufferSize; ++dbindex) {
    inst_buffer[dbindex].dstate = dstate;
    inst_buffer[dbindex].inst_index   = dbindex;
    inst_buffer[dbindex].vpc          = 0;
    NCInstBytesInitMemory(&inst_buffer[dbindex].inst.bytes, memory);
    NCInstBytesPtrInit((NCInstBytesPtr*) &inst_buffer[dbindex].inst_bytes,
                       &inst_buffer[dbindex].inst.bytes);

  }
  cur_inst->vpc = vbase;
  return cur_inst;
}

/* Modify the current instruction pointer to point to the next instruction
 * in the ring buffer.  Reset the state of that next instruction.
 */
static NCDecoderInst* IncrementInst(NCDecoderInst* inst) {
  /* giving PreviousInst a positive number will get NextInst
   * better to keep the buffer switching logic in one place
   */
  NCDecoderInst* next_inst = PreviousInst(inst, 1);
  next_inst->vpc = inst->vpc + inst->inst.bytes.length;
  next_inst->dstate->cur_inst_index = next_inst->inst_index;
  return next_inst;
}

/* Get the i-th byte of the current instruction being parsed. */
static uint8_t GetInstByte(NCDecoderInst* dinst, ssize_t i) {
  if (i < dinst->inst.bytes.length) {
    return dinst->inst.bytes.byte[i];
  } else {
    return NCRemainingMemoryLookahead(&dinst->dstate->memory,
                                      i - dinst->inst.bytes.length);
  }
}

/* Consume a predefined nop byte sequence, if a match can be found.
 * Further, if found, replace the currently matched instruction with
 * the consumed predefined nop.
 */
static void ConsumePredefinedNop(NCDecoderInst* dinst) {
  /* Do maximal match of possible nops */
  uint8_t pos = 0;
  struct OpInfo* matching_opinfo = NULL;
  ssize_t matching_length = 0;
  NCNopTrieNode* next = (NCNopTrieNode*) (kNcNopTrieNode + 0);
  uint8_t byte = GetInstByte(dinst, pos);
  while (NULL != next) {
    if (byte == next->matching_byte) {
      DEBUG(printf("NOP match byte: 0x%02x\n", (int) byte));
      byte = GetInstByte(dinst, ++pos);
      if (NULL != next->matching_opinfo) {
        DEBUG(printf("NOP matched rule! %d\n", pos));
        matching_opinfo = next->matching_opinfo;
        matching_length = pos;
      }
      next = next->success;
    } else {
      next = next->fail;
    }
  }
  if (NULL == matching_opinfo) {
    DEBUG(printf("NOP match failed!\n"));
  } else {
    DEBUG(printf("NOP match succeeds! Using last matched rule.\n"));
    NCRemainingMemoryReset(&dinst->dstate->memory);
    InitDecoder(dinst);
    NCInstBytesReadBytes(matching_length, &dinst->inst.bytes);
    dinst->opinfo = matching_opinfo;
  }
}

/* If we didn't find a good instruction, try to consume one of the
 * predefined NOP's.
 */
static void MaybeConsumePredefinedNop(NCDecoderInst* dinst) {
  switch (dinst->opinfo->insttype) {
    case NACLi_UNDEFINED:
    case NACLi_INVALID:
    case NACLi_ILLEGAL:
      ConsumePredefinedNop(dinst);
      break;
    default:
      break;
  }
}

/* All of the actions needed to read one additional instruction into mstate.
 */
static void ConsumeNextInstruction(struct NCDecoderInst* inst) {
  DEBUG( printf("Decoding instruction at %"NACL_PRIxNaClPcAddress":\n",
                inst->vpc) );
  InitDecoder(inst);
  ConsumePrefixBytes(inst);
  ConsumeOpcodeBytes(inst);
  ConsumeModRM(inst);
  ConsumeSIB(inst);
  ConsumeID(inst);
  MaybeGet3ByteOpInfo(inst);
  MaybeConsumePredefinedNop(inst);
}

/* The actual decoder */
void NCDecodeSegmentUsing(uint8_t* mbase, NaClPcAddress vbase,
                          NaClMemorySize size,
                          struct NCValidatorState* vstate,
                          NCDecoderState* dstate,
                          NCDecoderAction decoderaction,
                          NCDecoderStats newsegment,
                          NCDecoderStats segmentationerror,
                          NCDecoderStats internalerror) {
  const NaClPcAddress vlimit = vbase + size;
  NCDecoderInst* dinst;
  dinst = InitDecoderState(mbase, vbase, size, vstate, dstate,
                           segmentationerror, internalerror);

  DEBUG( printf("DecodeSegment(%p[%"NACL_PRIxNaClPcAddress
                "-%"NACL_PRIxNaClPcAddress"])\n",
                (void*) mbase, vbase, vlimit) );
  newsegment(vstate);
  while (dinst->vpc < vlimit) {
    ConsumeNextInstruction(dinst);
    if (dstate->memory.overflow_count) {
      NaClPcAddress newpc = dinst->vpc + dinst->inst.bytes.length;
      fprintf(stdout, "%"NACL_PRIxNaClPcAddress" > %"NACL_PRIxNaClPcAddress
              " (read overflow of %d bytes)\n",
              newpc, vlimit, dstate->memory.overflow_count);
      ErrorSegmentation(dinst);
      break;
    }
    decoderaction(dinst);
    /* get ready for next round */
    dinst = IncrementInst(dinst);
  }
}

/* The actual decoder */
void NCDecodeSegment(uint8_t* mbase, NaClPcAddress vbase,
                     NaClMemorySize size,
                     struct NCValidatorState* vstate) {
  NCDecoderState dstate;
  NCDecodeSegmentUsing(mbase, vbase, size, vstate, &dstate,
                       g_DecoderAction,
                       g_NewSegment,
                       g_SegFault,
                       g_InternalError);
}

/* The actual decoder -- decodes two instruction segments in parallel */
void NCDecodeSegmentPairUsing(
    uint8_t* mbase_old, uint8_t* mbase_new,
    NaClPcAddress vbase, NaClMemorySize size,
    struct NCValidatorState* vstate,
    NCDecoderState* dstate_old,
    NCDecoderState* dstate_new,
    NCDecoderPairAction action,
    NCDecoderStats newsegment,
    NCDecoderStats segmentationerror,
    NCDecoderStats internalerror) {

  const NaClPcAddress vlimit = vbase + size;
  NCDecoderInst* dinst_old;
  NCDecoderInst* dinst_new;
  NaClPcAddress newpc_old;
  NaClPcAddress newpc_new;

  dinst_old = InitDecoderState(mbase_old, vbase, size, vstate, dstate_old,
                               segmentationerror, internalerror);
  dinst_new = InitDecoderState(mbase_new, vbase, size, vstate, dstate_new,
                               segmentationerror, internalerror);

  DEBUG( printf("DecodeSegmentPair(%"NACL_PRIxNaClPcAddress
                "-%"NACL_PRIxNaClPcAddress")\n",
                vbase, vlimit) );
  newsegment(vstate);
  while (dinst_old->vpc < vlimit && dinst_new->vpc < vlimit) {
    ConsumeNextInstruction(dinst_old);
    ConsumeNextInstruction(dinst_new);
    newpc_old = dinst_old->vpc + dinst_old->inst.bytes.length;
    newpc_new = dinst_new->vpc + dinst_old->inst.bytes.length;

    if (newpc_old != newpc_new) {
      fprintf(stdout, "misaligned replacement code "
              "%"NACL_PRIxNaClPcAddress" != %"NACL_PRIxNaClPcAddress"\n",
              newpc_old, newpc_new);
      ErrorSegmentation(dinst_new);
      break;
    }

    if (newpc_new > vlimit) {
      fprintf(stdout, "%"NACL_PRIxNaClPcAddress" > %"NACL_PRIxNaClPcAddress"\n",
              newpc_new, vlimit);
      ErrorSegmentation(dinst_new);
      break;
    }

    action(dinst_old, dinst_new);

    /* get ready for next round */
    dinst_old = IncrementInst(dinst_old);
    dinst_new = IncrementInst(dinst_new);
  }
}

/* The actual decoder -- decodes two instruction segments in parallel */
void NCDecodeSegmentPair(uint8_t* mbase_old, uint8_t* mbase_new,
                         NaClPcAddress vbase, NaClMemorySize size,
                         struct NCValidatorState* vstate,
                         NCDecoderPairAction action) {
  NCDecoderState dstate_old;
  NCDecoderState dstate_new;
  NCDecodeSegmentPairUsing(mbase_old, mbase_new, vbase, size, vstate,
                           &dstate_old, &dstate_new,
                           action, g_NewSegment, g_SegFault, g_InternalError);
}
