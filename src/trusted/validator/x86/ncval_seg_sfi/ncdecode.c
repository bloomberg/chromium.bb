/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
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

#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_aux.h"

#include <stdio.h>
#include <assert.h>

#if NACL_TARGET_SUBARCH == 64
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/gen/ncdecodetab_64.h"
#else
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/gen/ncdecodetab_32.h"
#endif

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

#include "native_client/src/trusted/validator/x86/ncinstbuffer_inl.c"
#include "native_client/src/trusted/validator/x86/x86_insts_inl.c"

/* Generates a print name for the given NCDecodeImmediateType. */
static const char* NCDecodeImmediateTypeName(NCDecodeImmediateType type) {
  DEBUG_OR_ERASE(
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
      });
  /* NOTREACHED */
  return NULL;
}

/* Prints out the contents of the given OpInfo. Should only be called
 * inside a DEBUG macro (i.e. for debugging only).
 */
static void PrintOpInfo(const struct OpInfo* info) {
  DEBUG_OR_ERASE(printf("opinfo(%s, hasmrm=%u, immtype=%s, opinmrm=%d)\n",
                        NaClInstTypeString(info->insttype),
                        info->hasmrmbyte,
                        NCDecodeImmediateTypeName(info->immtype),
                        info->opinmrm));
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

static Bool NullDecoderAction(const struct NCDecoderInst* dinst) {
  UNREFERENCED_PARAMETER(dinst);
  return TRUE;
}
static void NullDecoderMethod(struct NCDecoderState* dstate) {
  UNREFERENCED_PARAMETER(dstate);
}

/* API to virtual methods of a decoder state. */
void NCDecoderStateNewSegment(NCDecoderState* tthis) {
  (tthis->new_segment_fn)(tthis);
}

static Bool NCDecoderStateApplyAction(NCDecoderState* tthis,
                                      NCDecoderInst* dinst) {
  return (tthis->action_fn)(dinst);
}

static void NCDecoderStateSegmentationError(NCDecoderState* tthis) {
  (tthis->segmentation_error_fn)(tthis);
}

static void NCDecoderStateInternalError(NCDecoderState* tthis) {
  (tthis->internal_error_fn)(tthis);
}

/* Error Condition Handling */
static void ErrorSegmentation(NCDecoderInst* dinst) {
  NCDecoderState* dstate = dinst->dstate;
  NaClErrorReporter* reporter = dstate->error_reporter;
  (*reporter->printf)(dstate->error_reporter, "ErrorSegmentation\n");
  /* When the decoder is used by the NaCl validator    */
  /* the validator provides an error handler that does */
  /* the necessary bookeeping to track these errors.   */
  NCDecoderStateSegmentationError(dstate);
}

static void ErrorInternal(NCDecoderInst* dinst) {
  NCDecoderState* dstate = dinst->dstate;
  NaClErrorReporter* reporter = dstate->error_reporter;
  (*reporter->printf)(reporter, "ErrorInternal\n");
  /* When the decoder is used by the NaCl validator    */
  /* the validator provides an error handler that does */
  /* the necessary bookeeping to track these errors.   */
  NCDecoderStateInternalError(dstate);
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

static INLINE void InitDecoder(struct NCDecoderInst* dinst) {
  NCInstBytesInitInline(&dinst->inst.bytes);
  dinst->inst.prefixbytes = 0;
  dinst->inst.prefixmask = 0;
  dinst->inst.opcode_prefixmask = 0;
  dinst->inst.num_opbytes = 1;  /* unless proven otherwise. */
  dinst->inst.hassibbyte = 0;
  dinst->inst.mrm = 0;
  dinst->inst.immtype = IMM_UNKNOWN;
  dinst->inst.immbytes = 0;
  dinst->inst.dispbytes = 0;
  dinst->inst.rexprefix = 0;
  dinst->inst.lock_prefix_index = kNoLockPrefixIndex;
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
static void ConsumePrefixBytes(struct NCDecoderInst* dinst) {
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
    NCInstBytesReadInline(&dinst->inst.bytes);
    DEBUG( printf("  prefix mask: %08x\n", dinst->inst.prefixmask) );
    if (NACL_TARGET_SUBARCH == 64 && prefix_form == kPrefixREX) {
      dinst->inst.rexprefix = nb;
      /* REX prefix must be last prefix. */
      return;
    }
    if (prefix_form == kPrefixLOCK) {
      /* Note: we don't have to worry about duplicates, since
       * ValidatePrefixes in ncvalidate.c will not allow such
       * a possibility.
       */
      dinst->inst.lock_prefix_index = (uint8_t) ii;
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
    dinst->inst.prefixmask &= ~kPrefixDATA16;
    dinst->inst.opcode_prefixmask = kPrefixDATA16;
    return &kDecode660FXXOp[opbyte2];
  } else if (pm & kPrefixREPNE) {
    dinst->inst.prefixmask &= ~kPrefixREPNE;
    dinst->inst.opcode_prefixmask = kPrefixREPNE;
    return &kDecodeF20FXXOp[opbyte2];
  } else if (pm & kPrefixREP) {
    dinst->inst.prefixmask &= ~kPrefixREP;
    dinst->inst.opcode_prefixmask = kPrefixREP;
    return &kDecodeF30FXXOp[opbyte2];
  }
  ErrorInternal(dinst);
  return dinst->opinfo;
}

static void GetX87OpInfo(NCDecoderInst* dinst) {
  /* WAIT is an x87 instruction but not in the coproc opcode space. */
  uint8_t op1 = NCInstBytesByteInline(&dinst->inst_bytes,
                                      dinst->inst.prefixbytes);
  if (op1 < kFirstX87Opcode || op1 > kLastX87Opcode) {
    if (op1 != kWAITOp) ErrorInternal(dinst);
    return;
  }
  dinst->opinfo = &kDecodeX87Op[op1 - kFirstX87Opcode][dinst->inst.mrm];
  DEBUG( printf("NACL_X87 op1 = %02x, ", op1);
         PrintOpInfo(dinst->opinfo) );
}

static void ConsumeOpcodeBytes(NCDecoderInst* dinst) {
  uint8_t opcode = NCInstBytesReadInline(&dinst->inst.bytes);
  dinst->opinfo = &kDecode1ByteOp[opcode];
  DEBUG( printf("NACLi_1BYTE: opcode = %02x, ", opcode);
         PrintOpInfo(dinst->opinfo) );
  if (opcode == kTwoByteOpcodeByte1) {
    uint8_t opcode2 = NCInstBytesReadInline(&dinst->inst.bytes);
    dinst->opinfo = GetExtendedOpInfo(dinst, opcode2);
    DEBUG( printf("NACLi_2BYTE: opcode2 = %02x, ", opcode2);
           PrintOpInfo(dinst->opinfo) );
    dinst->inst.num_opbytes = 2;
    if (dinst->opinfo->insttype == NACLi_3BYTE) {
      uint8_t opcode3 = NCInstBytesReadInline(&dinst->inst.bytes);
      uint32_t pm;
      pm = dinst->inst.opcode_prefixmask;
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

static void ConsumeModRM(NCDecoderInst* dinst) {
  if (dinst->opinfo->hasmrmbyte != 0) {
    const uint8_t mrm = NCInstBytesReadInline(&dinst->inst.bytes);
    DEBUG( printf("Mod/RM byte: %02x\n", mrm) );
    dinst->inst.mrm = mrm;
    if (dinst->opinfo->insttype == NACLi_X87 ||
        dinst->opinfo->insttype == NACLi_X87_FSINCOS) {
      GetX87OpInfo(dinst);
    }
    if (dinst->opinfo->opinmrm) {
      const struct OpInfo* mopinfo =
        &kDecodeModRMOp[dinst->opinfo->opinmrm][modrm_opcodeInline(mrm)];
      dinst->opinfo = mopinfo;
      DEBUG( printf("NACLi_opinmrm: modrm.opcode = %x, ",
                    modrm_opcodeInline(mrm));
             PrintOpInfo(dinst->opinfo) );
      if (dinst->inst.immtype == IMM_UNKNOWN) {
        assert(0);
        dinst->inst.immtype = mopinfo->immtype;
      }
      /* handle weird case for 0xff TEST Ib/Iv */
      if (modrm_opcodeInline(mrm) == 0) {
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
      switch (modrm_modInline(mrm)) {
        case 0:
          if (modrm_rmInline(mrm) == 0x06) {
            dinst->inst.dispbytes = 2;        /* disp16 */
          } else {
            dinst->inst.dispbytes = 0;
          }
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
      switch (modrm_modInline(mrm)) {
        case 0:
          if (modrm_rmInline(mrm) == 0x05) {
            dinst->inst.dispbytes = 4;         /* disp32 */
          } else {
            dinst->inst.dispbytes = 0;
          }
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
      dinst->inst.hassibbyte = ((modrm_rmInline(mrm) == 0x04) &&
                                 (modrm_modInline(mrm) != 3));
    }
  }
  DEBUG( printf("  dispbytes = %d, hasibbyte = %d\n",
                dinst->inst.dispbytes, dinst->inst.hassibbyte) );
}

static INLINE void ConsumeSIB(NCDecoderInst* dinst) {
  if (dinst->inst.hassibbyte != 0) {
    const uint8_t sib = NCInstBytesReadInline(&dinst->inst.bytes);
    if (sib_base(sib) == 0x05) {
      switch (modrm_modInline(dinst->inst.mrm)) {
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

static INLINE void ConsumeID(NCDecoderInst* dinst) {
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
  NCInstBytesReadBytesInline((ssize_t) dinst->inst.immbytes,
                             &dinst->inst.bytes);
  NCInstBytesReadBytesInline((ssize_t) dinst->inst.dispbytes,
                             &dinst->inst.bytes);
  DEBUG(printf("ID: %d disp bytes, %d imm bytes\n",
               dinst->inst.dispbytes, dinst->inst.immbytes));
}

/* Actually this routine is special for 3DNow instructions */
static INLINE void MaybeGet3ByteOpInfo(NCDecoderInst* dinst) {
  if (dinst->opinfo->insttype == NACLi_3DNOW) {
    uint8_t opbyte1 = NCInstBytesByteInline(&dinst->inst_bytes,
                                      dinst->inst.prefixbytes);
    uint8_t opbyte2 = NCInstBytesByteInline(&dinst->inst_bytes,
                                      dinst->inst.prefixbytes + 1);
    if (opbyte1 == kTwoByteOpcodeByte1 &&
        opbyte2 == k3DNowOpcodeByte2) {
      uint8_t immbyte =
          NCInstBytesByteInline(&dinst->inst_bytes,
                                dinst->inst.bytes.length - 1);
      dinst->opinfo = &kDecode0F0FOp[immbyte];
      DEBUG( printf(
                 "NACLi_3DNOW: byte1 = %02x, byte2 = %02x, immbyte = %02x,\n  ",
                 opbyte1, opbyte2, immbyte);
             PrintOpInfo(dinst->opinfo) );
    }
  }
}

/* Gets an instruction nindex away from the given instruction.
 * WARNING: Does not do bounds checking, other than rolling the
 * index as needed to stay within the (circular) instruction buffer.
 */
static NCDecoderInst* NCGetInstDiff(const NCDecoderInst* dinst,
                                      int nindex) {
  /* Note: This code also handles increments, so that we can
   * use the same code for both.
   */
  size_t index = (dinst->inst_index + nindex) % dinst->dstate->inst_buffer_size;
  return &dinst->dstate->inst_buffer[index];
}

struct NCDecoderInst* PreviousInst(const NCDecoderInst* dinst,
                                   int nindex) {
  if ((nindex > 0) && (((size_t) nindex) < dinst->inst_count)) {
    return NCGetInstDiff(dinst, -nindex);
  } else {
    return NULL;
  }
}

/* Initialize the decoder state fields, assuming constructor parameter
 * fields mbase, vbase, size, inst_buffer, and inst_buffer_size have
 * already been set.
 */
static void NCDecoderStateInitFields(NCDecoderState* this) {
  size_t dbindex;
  this->error_reporter = &kNCNullErrorReporter;
  NCRemainingMemoryInit(this->mbase, this->size, &this->memory);
  this->memory.error_fn = NCRemainingMemoryInternalError;
  this->memory.error_fn_state = (void*) this;
  for (dbindex = 0; dbindex < this->inst_buffer_size; ++dbindex) {
    this->inst_buffer[dbindex].dstate = this;
    this->inst_buffer[dbindex].inst_index = dbindex;
    this->inst_buffer[dbindex].inst_count = 1;
    this->inst_buffer[dbindex].inst_addr = 0;
    this->inst_buffer[dbindex].unchanged = FALSE;
    NCInstBytesInitMemory(&this->inst_buffer[dbindex].inst.bytes,
                          &this->memory);
    NCInstBytesPtrInit((NCInstBytesPtr*) &this->inst_buffer[dbindex].inst_bytes,
                       &this->inst_buffer[dbindex].inst.bytes);
  }
  this->cur_inst_index = 0;
}

void NCDecoderStateConstruct(NCDecoderState* this,
                             uint8_t* mbase, NaClPcAddress vbase,
                             NaClMemorySize size,
                             NCDecoderInst* inst_buffer,
                             size_t inst_buffer_size) {

  /* Start by setting up virtual functions. */
  this->action_fn = NullDecoderAction;
  this->new_segment_fn = NullDecoderMethod;
  this->segmentation_error_fn = NullDecoderMethod;
  this->internal_error_fn = NullDecoderMethod;

  /* Initialize the user-provided fields. */
  this->mbase = mbase;
  this->vbase = vbase;
  this->size = size;
  this->inst_buffer = inst_buffer;
  this->inst_buffer_size = inst_buffer_size;

  NCDecoderStateInitFields(this);
}

void NCDecoderStateDestruct(NCDecoderState* this) {
  /* Currently, there is nothing to do. */
}

/* "Printable" means the value returned by this function can be used for
 * printing user-readable output, but it should not be used to influence if the
 * validation algorithm passes or fails.  The validation algorithm should not
 * depend on vbase - in other words, it should not depend on where the code is
 * being mapped in memory.
 */
static INLINE NaClPcAddress NCPrintableVLimit(NCDecoderState *dstate) {
  return dstate->vbase + dstate->size;
}

/* Modify the current instruction pointer to point to the next instruction
 * in the ring buffer.  Reset the state of that next instruction.
 */
static NCDecoderInst* IncrementInst(NCDecoderInst* inst) {
  /* giving PreviousInst a positive number will get NextInst
   * better to keep the buffer switching logic in one place
   */
  NCDecoderInst* next_inst = NCGetInstDiff(inst, 1);
  next_inst->inst_addr = inst->inst_addr + inst->inst.bytes.length;
  next_inst->dstate->cur_inst_index = next_inst->inst_index;
  next_inst->inst_count = inst->inst_count + 1;
  next_inst->unchanged = FALSE;
  return next_inst;
}

/* Get the i-th byte of the current instruction being parsed. */
static uint8_t GetInstByte(NCDecoderInst* dinst, ssize_t i) {
  if (i < dinst->inst.bytes.length) {
    return dinst->inst.bytes.byte[i];
  } else {
    return NCRemainingMemoryLookaheadInline(&dinst->dstate->memory,
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
    NCRemainingMemoryResetInline(&dinst->dstate->memory);
    InitDecoder(dinst);
    NCInstBytesReadBytesInline(matching_length, &dinst->inst.bytes);
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
void NCConsumeNextInstruction(struct NCDecoderInst* inst) {
  DEBUG( printf("Decoding instruction at %"NACL_PRIxNaClPcAddress":\n",
                inst->inst_addr) );
  InitDecoder(inst);
  ConsumePrefixBytes(inst);
  ConsumeOpcodeBytes(inst);
  ConsumeModRM(inst);
  ConsumeSIB(inst);
  ConsumeID(inst);
  MaybeGet3ByteOpInfo(inst);
  MaybeConsumePredefinedNop(inst);
}

void NCDecoderStateSetErrorReporter(NCDecoderState* this,
                                    NaClErrorReporter* reporter) {
  switch (reporter->supported_reporter) {
    case NaClNullErrorReporter:
    case NCDecoderInstErrorReporter:
      this->error_reporter = reporter;
      return;
    default:
      break;
  }
  (*reporter->printf)(
      reporter,
      "*** FATAL: using unsupported error reporter! ***\n"
      "*** NCDecoderInstErrorReporter expected but found %s***\n",
      NaClErrorReporterSupportedName(reporter->supported_reporter));
  exit(1);
}

static void NCNullErrorPrintInst(NaClErrorReporter* self,
                                 struct NCDecoderInst* inst) {}

NaClErrorReporter kNCNullErrorReporter = {
  NaClNullErrorReporter,
  NaClNullErrorPrintf,
  NaClNullErrorPrintfV,
  (NaClPrintInst) NCNullErrorPrintInst,
};

Bool NCDecoderStateDecode(NCDecoderState* this) {
  NCDecoderInst* dinst = &this->inst_buffer[this->cur_inst_index];
  DEBUG( printf("DecodeSegment(%p[%"NACL_PRIxNaClPcAddress"])\n",
                (void*) this->memory.mpc, (NaClPcAddress) this->size) );
  NCDecoderStateNewSegment(this);
  while (dinst->inst_addr < this->size) {
    NCConsumeNextInstruction(dinst);
    if (this->memory.overflow_count) {
      NaClPcAddress newpc = (NCPrintableInstructionAddress(dinst)
                             + dinst->inst.bytes.length);
      (*this->error_reporter->printf)(
          this->error_reporter,
          "%"NACL_PRIxNaClPcAddress" > %"NACL_PRIxNaClPcAddress
          " (read overflow of %d bytes)\n",
          newpc, NCPrintableVLimit(this), this->memory.overflow_count);
      ErrorSegmentation(dinst);
      return FALSE;
    }
    if (!NCDecoderStateApplyAction(this, dinst)) return FALSE;
    /* get ready for next round */
    dinst = IncrementInst(dinst);
  }
  return TRUE;
}

/* Default action for a decoder state pair. */
static Bool NullNCDecoderStatePairAction(struct NCDecoderStatePair* tthis,
                                         NCDecoderInst* old_inst,
                                         NCDecoderInst* new_inst) {
  return TRUE;
}

void NCDecoderStatePairConstruct(NCDecoderStatePair* tthis,
                                 NCDecoderState* old_dstate,
                                 NCDecoderState* new_dstate,
                                 NaClCopyInstructionFunc copy_func) {
  tthis->old_dstate = old_dstate;
  tthis->new_dstate = new_dstate;
  tthis->action_fn = NullNCDecoderStatePairAction;
  tthis->copy_func = copy_func;
}

void NCDecoderStatePairDestruct(NCDecoderStatePair* tthis) {
}

Bool NCDecoderStatePairDecode(NCDecoderStatePair* tthis) {
  NCDecoderInst* old_dinst =
      &tthis->old_dstate->inst_buffer[tthis->old_dstate->cur_inst_index];
  NCDecoderInst* new_dinst =
      &tthis->new_dstate->inst_buffer[tthis->new_dstate->cur_inst_index];

  /* Verify that the size of the code segments is the same, and has not
   * been changed.
   */
  if (tthis->old_dstate->size != tthis->new_dstate->size) {
    /* If sizes differ, then they can't be the same, except for some
     * (constant-sized) changes. Hence fail to decode.
     */
    ErrorSegmentation(new_dinst);
    return FALSE;
  }

  /* Since the sizes of the segments are the same, only one limit
   * needs to be checked. Hence, we will track the limit of the new
   * decoder state.
   */
  DEBUG( printf("NCDecoderStatePairDecode(%"NACL_PRIxNaClPcAddress")\n",
                (NaClPcAddress) tthis->new_dstate->size));

  /* Initialize decoder statements for decoding segment, by calling
   * the corresponding virtual in the decoder.
   */
  NCDecoderStateNewSegment(tthis->old_dstate);
  NCDecoderStateNewSegment(tthis->new_dstate);

  /* Walk through both instruction segments, checking that
   * they decode similarly.
   */
  while (new_dinst->inst_addr < tthis->new_dstate->size) {

    NCConsumeNextInstruction(old_dinst);
    NCConsumeNextInstruction(new_dinst);


    /* Verify that the instruction lengths match. */
    if (old_dinst->inst.bytes.length !=
        new_dinst->inst.bytes.length) {
      ErrorInternal(new_dinst);
      return FALSE;
    }

    /* Verify that we haven't walked past the end of the segment
     * in either decoder state.
     *
     * Note: Since instruction lengths are the same, and the
     * segment lengths are the same, if overflow occurs on one
     * segment, it must occur on the other.
     */
    if (new_dinst->inst_addr > tthis->new_dstate->size) {
      NaClErrorReporter* reporter = new_dinst->dstate->error_reporter;
      (*reporter->printf)(
          reporter,
          "%"NACL_PRIxNaClPcAddress" > %"NACL_PRIxNaClPcAddress"\n",
          NCPrintableInstructionAddress(new_dinst),
          NCPrintableVLimit(tthis->new_dstate));
      ErrorSegmentation(new_dinst);
      return FALSE;
    }

    /* Apply the action to the instructions, and continue
     * only if the action succeeds.
     */
    if (! (tthis->action_fn)(tthis, old_dinst, new_dinst)) {
      return FALSE;
    }

    /* Move to next instruction. */
    old_dinst = IncrementInst(old_dinst);
    new_dinst = IncrementInst(new_dinst);
  }
  return TRUE;
}
