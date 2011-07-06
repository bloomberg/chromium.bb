/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncvalidate.c
 * Validate x86 instructions for Native Client
 *
 */

#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/validator/x86/halt_trim.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate_internaltypes.h"

#if NACL_TARGET_SUBARCH == 64
# include "native_client/src/trusted/validator_x86/gen/ncbadprefixmask_64.h"
#else
# include "native_client/src/trusted/validator_x86/gen/ncbadprefixmask_32.h"
#endif

/* debugging stuff */
#define DEBUGGING 0
#if DEBUGGING
#define dprint(args)        do { printf args; } while (0)
#else
#define dprint(args)        do { if (0) { printf args; } } while (0)
/* allows DCE but compiler can still do format string checks */
#endif  /* DEBUGGING */

/* TODO(bradchen) verbosity needs to be controllable via commandline flags */
#define VERBOSE 1
#if VERBOSE
#define vprint(vstate, args) \
  do { \
    NaClErrorReporter* reporter = vstate->dstate.error_reporter; \
    reporter->printf args; \
  } while (0)
#else
#define vprint(vstate, args) \
  do { \
    if (0) { \
      NaClErrorReporter* reporter = vstate->dstate.error_reporter; \
      reporter->printf args; \
    } \
  } while (0)
/* allows DCE but compiler can still do format string checks */
#endif  /* VERBOSE */

/* The following macro is used to clarify the derived class relationship
 * of NCValidateState and NCDecoderState. That is, &this->dstate is also
 * an instance of a validator state. Hence one can downcast this pointer.
 */
#define VALIDATOR_STATE_DOWNCAST(this_dstate) \
  ((NCValidatorState*) (this_dstate))

static const uint8_t kNaClFullStop = 0xf4;   /* x86 HALT opcode */

/* Define how many diagnostic error messages are printed by the validator.
 * A value of zero generates no diagnostics.
 * A value >0 allows up to that many diagnostic error messages.
 * A negative value prints all diagnostic error messages.
 */
static int kMaxDiagnostics = 0;

int NCValidatorGetMaxDiagnostics() {
  return kMaxDiagnostics;
}

void NCValidatorSetMaxDiagnostics(int new_value) {
  kMaxDiagnostics = new_value;
}

static void ValidatePrintError(const NaClPcAddress addr, const char *msg,
                               struct NCValidatorState *vstate) {
  if (vstate->num_diagnostics != 0) {
    NaClErrorReporter* reporter = vstate->dstate.error_reporter;
    reporter->printf(
        reporter,
        "VALIDATOR: %"NACL_PRIxNaClPcAddress": %s\n", addr, msg);
    --(vstate->num_diagnostics);
    if (vstate->num_diagnostics == 0) {
      reporter->printf(
          reporter,
          "VALIDATOR: Error limit reached, turning off diagnostics!\n");
    }
  }
}

static void BadInstructionError(const NCDecoderInst *dinst,
                                const char *msg) {
  NCValidatorState* vstate = VALIDATOR_STATE_DOWNCAST(dinst->dstate);
  ValidatePrintError(dinst->vpc, msg, vstate);
  if (vstate->do_stub_out) {
    memset(dinst->dstate->memory.mpc, kNaClFullStop,
           dinst->dstate->memory.read_length);
  }
}

/* The set of cpu features to use, if non-NULL.
 * NOTE: This global is used to allow the injection of
 * a command-line override of CPU features, from that of the local
 * CPU id, for the tool ncval. As such, when this global is non-null,
 * it uses the injected value this pointer points to as the corresponding
 * CPU id results to use.
 */
static CPUFeatures *nc_validator_features = NULL;

void NCValidateSetCPUFeatures(CPUFeatures *new_features) {
  nc_validator_features = new_features;
}

/* opcode histogram */
#if VERBOSE == 1
void OpcodeHisto(const uint8_t byte1, struct NCValidatorState *vstate) {
  vstate->opcodehisto[byte1] += 1;
}

static void InitOpcodeHisto(struct NCValidatorState *vstate) {
  int i;
  for (i = 0; i < 256; i += 1) vstate->opcodehisto[i] = 0;
}

static void PrintOpcodeHisto(struct NCValidatorState *vstate) {
  int i;
  int printed_in_this_row = 0;
  NaClErrorReporter* reporter = vstate->dstate.error_reporter;
  if (!VERBOSE) return;
  reporter->printf(reporter, "\nOpcode Histogram;\n");
  for (i = 0; i < 256; ++i) {
    if (0 != vstate->opcodehisto[i]) {
      reporter->printf(reporter, "%d\t0x%02x\t", vstate->opcodehisto[i], i);
      ++printed_in_this_row;
      if (printed_in_this_row > 3) {
        printed_in_this_row = 0;
        reporter->printf(reporter, "\n");
      }
    }
  }
  if (0 != printed_in_this_row) {
    reporter->printf(reporter, "\n");
  }
}
#else
#define OpcodeHisto(b, v)
#define InitOpcodeHisto(v)
#define PrintOpcodeHisto(f, v)
#endif /* VERBOSE == 1 */

/* statistics code */
static void Stats_Inst(struct NCValidatorState *vstate) {
  vstate->stats.instructions += 1;
}

static void Stats_CheckTarget(struct NCValidatorState *vstate) {
  vstate->stats.checktarget += 1;
}

static void Stats_TargetIndirect(struct NCValidatorState *vstate) {
  vstate->stats.targetindirect += 1;
}

static void Stats_SawFailure(struct NCValidatorState *vstate) {
  vstate->stats.sawfailure = 1;
}

static void Stats_InternalError(struct NCValidatorState *vstate) {
  vstate->stats.internalerrors += 1;
  Stats_SawFailure(vstate);
}

static void Stats_BadAlignment(struct NCValidatorState *vstate) {
  vstate->stats.badalignment += 1;
  Stats_SawFailure(vstate);
}

static void Stats_SegFault(struct NCValidatorState *vstate) {
  vstate->stats.segfaults += 1;
  Stats_SawFailure(vstate);
}

static void Stats_NewSegment(struct NCValidatorState *vstate) {
  vstate->stats.segments += 1;
  if (vstate->stats.segments > 1) {
    vprint(vstate, (reporter, "error: multiple segments\n"));
    Stats_SawFailure(vstate);
  }
}

static void Stats_BadTarget(struct NCValidatorState *vstate) {
  vstate->stats.badtarget += 1;
  Stats_SawFailure(vstate);
}

static void Stats_UnsafeIndirect(struct NCValidatorState *vstate) {
  vstate->stats.unsafeindirect += 1;
  Stats_SawFailure(vstate);
}

static void Stats_Return(struct NCValidatorState *vstate) {
  vstate->stats.returns += 1;
  Stats_UnsafeIndirect(vstate);
  Stats_SawFailure(vstate);
}

static void Stats_IllegalInst(struct NCValidatorState *vstate) {
  vstate->stats.illegalinst += 1;
  Stats_SawFailure(vstate);
}

static void Stats_BadPrefix(struct NCValidatorState *vstate) {
  vstate->stats.badprefix += 1;
  vstate->stats.illegalinst += 1; /* a bad prefix is also an invalid inst */
  Stats_SawFailure(vstate);
}

static void Stats_BadInstLength(struct NCValidatorState *vstate) {
  vstate->stats.badinstlength += 1;
  Stats_SawFailure(vstate);
}

static void Stats_Init(struct NCValidatorState *vstate) {
  vstate->stats.instructions = 0;
  vstate->stats.segments = 0;
  vstate->stats.checktarget = 0;
  vstate->stats.targetindirect = 0;
  vstate->stats.badtarget = 0;
  vstate->stats.unsafeindirect = 0;
  vstate->stats.returns = 0;
  vstate->stats.illegalinst = 0;
  vstate->stats.badalignment = 0;
  vstate->stats.internalerrors = 0;
  vstate->stats.badinstlength = 0;
  vstate->stats.badprefix = 0;
  vstate->stats.sawfailure = 0;
  InitOpcodeHisto(vstate);
}

void Stats_Print(struct NCValidatorState *vstate) {
  NaClErrorReporter* reporter;
  if (!VERBOSE || (vstate == NULL)) return;
  reporter = vstate->dstate.error_reporter;
  PrintOpcodeHisto(vstate);
  reporter->printf(reporter, "Analysis Summary:\n");
  reporter->printf(reporter, "%d Checked instructions\n",
                   vstate->stats.instructions);
  reporter->printf(reporter, "%d checked jump targets\n",
                   vstate->stats.checktarget);
  reporter->printf(
      reporter, "%d calls/jumps need dynamic checking (%0.2f%%)\n",
      vstate->stats.targetindirect,
      vstate->stats.instructions ?
      100.0 * vstate->stats.targetindirect/vstate->stats.instructions : 0);
  reporter->printf(reporter, "\nProblems:\n");
  reporter->printf(reporter, "%d illegal instructions\n",
                   vstate->stats.illegalinst);
  reporter->printf(reporter, "%d bad jump targets\n", vstate->stats.badtarget);
  reporter->printf(
      reporter, "%d illegal unprotected indirect jumps (including ret)\n",
      vstate->stats.unsafeindirect);
  reporter->printf(reporter, "%d instruction alignment defects\n",
                   vstate->stats.badalignment);
  reporter->printf(reporter, "%d segmentation errors\n",
                   vstate->stats.segfaults);
  reporter->printf(reporter, "%d bad prefix\n",
                   vstate->stats.badprefix);
  reporter->printf(reporter, "%d bad instruction length\n",
                   vstate->stats.badinstlength);
  reporter->printf(reporter, "%d internal errors\n",
                   vstate->stats.internalerrors);
}

/***********************************************************************/
/* jump target table                                                   */
static const uint8_t iadrmasks[8] = {0x01, 0x02, 0x04, 0x08,
                                     0x10, 0x20, 0x40, 0x80};
#define IATOffset(__IA) ((__IA) >> 3)
#define IATMask(__IA) (iadrmasks[(__IA) & 0x7])
#define SetAdrTable(__IOFF, __TABLE) \
  (__TABLE)[IATOffset(__IOFF)] |= IATMask(__IOFF)
#define ClearAdrTable(__IOFF, __TABLE) \
  (__TABLE)[IATOffset(__IOFF)] &= ~(IATMask(__IOFF))
#define GetAdrTable(__IOFF, __TABLE) \
  ((__TABLE)[IATOffset(__IOFF)] & IATMask(__IOFF))

/* forward declarations, needed for registration */
static Bool ValidateInst(const NCDecoderInst *dinst);
static Bool ValidateInstReplacement(NCDecoderStatePair* tthis,
                                    NCDecoderInst *dinst_old,
                                    NCDecoderInst *dinst_new);

/*
 * NCValidateInit: Initialize NaCl validator internal state
 * Parameters:
 *    vbase: base virtual address for code segment
 *    vlimit: size in bytes of code segment
 *    alignment: 16 or 32, specifying alignment
 * Returns:
 *    an initialized struct NCValidatorState * if everything is okay,
 *    else NULL
 */
struct NCValidatorState *NCValidateInit(const NaClPcAddress vbase,
                                        const NaClPcAddress vlimit,
                                        const uint8_t alignment) {
  struct NCValidatorState *vstate = NULL;

  dprint(("NCValidateInit(%"NACL_PRIxNaClPcAddressAll
          ", %"NACL_PRIxNaClPcAddressAll", %08x)\n", vbase, vlimit, alignment));
  do {
    if (vlimit <= vbase) break;
    if (alignment != 16 && alignment != 32) break;
    if ((vbase & (alignment - 1)) != 0) break;
    dprint(("ncv_init(%"NACL_PRIxNaClPcAddress", %"NACL_PRIxNaClPcAddress
            ")\n", vbase, vlimit));
    vstate = (struct NCValidatorState *)calloc(1, sizeof(*vstate));
    if (vstate == NULL) break;
    /* Record default error reporter here, since we don't construct
     * the decoder state until the call to NCValidateSegment. This allows
     * us to update the error reporter in the decoder state properly.
     */
    vstate->dstate.error_reporter = &kNCNullErrorReporter;
    vstate->num_diagnostics = kMaxDiagnostics;
    vstate->iadrbase = vbase;
    vstate->iadrlimit = vlimit;
    vstate->alignment = alignment;
    vstate->alignmask = alignment-1;
    vstate->vttable = (uint8_t *)calloc(IATOffset(vlimit - vbase) + 1, 1);
    vstate->kttable = (uint8_t *)calloc(IATOffset(vlimit - vbase) + 1, 1);
    vstate->do_stub_out = 0;
    if (vstate->vttable == NULL || vstate->kttable == NULL) break;
    dprint(("  allocated tables\n"));
    Stats_Init(vstate);
    if (NULL == nc_validator_features) {
      NaClCPUData cpu_data;
      NaClCPUDataGet(&cpu_data);
      GetCPUFeatures(&cpu_data, &(vstate->cpufeatures));
    } else {
      vstate->cpufeatures = *nc_validator_features;
    }
    return vstate;
  } while (0);
  /* Failure. Clean up memory before returning. */
  if (NULL != vstate) {
    if (NULL != vstate->kttable) free(vstate->kttable);
    if (NULL != vstate->vttable) free(vstate->vttable);
    free(vstate);
  }
  return NULL;
}

void NCValidateSetStubOutMode(struct NCValidatorState *vstate,
                              int do_stub_out) {
  vstate->do_stub_out = do_stub_out;
  /* We also turn off error diagnostics, under the assumption
   * you don't want them. (Note: if the user wants them,
   * you can run ncval to get them)/
   */
  if (do_stub_out) {
    NCValidateSetNumDiagnostics(vstate, 0);
  }
}

void NCValidatorStateSetCPUFeatures(struct NCValidatorState* vstate,
                                    const CPUFeatures* features) {
  NaClCopyCPUFeatures(&vstate->cpufeatures, features);
}

void NCValidateSetNumDiagnostics(struct NCValidatorState* vstate,
                                 int num_diagnostics) {
  vstate->num_diagnostics = num_diagnostics;
}

void NCValidateSetErrorReporter(struct NCValidatorState* state,
                                NaClErrorReporter* error_reporter) {
  NCDecoderStateSetErrorReporter(&state->dstate, error_reporter);
}

static void RememberIP(const NaClPcAddress ip,
                       struct NCValidatorState *vstate) {
  const NaClMemorySize ioffset =  ip - vstate->iadrbase;
  if (ip < vstate->iadrbase || ip >= vstate->iadrlimit) {
    ValidatePrintError(ip, "JUMP TARGET out of range in RememberIP", vstate);
    Stats_BadTarget(vstate);
    return;
  }
  if (GetAdrTable(ioffset, vstate->vttable)) {
    vprint(vstate, (reporter,
                    "RememberIP: Saw inst at %"NACL_PRIxNaClPcAddressAll
                    " twice\n", ip));
    Stats_InternalError(vstate);
    return;
  }
  Stats_Inst(vstate);
  SetAdrTable(ioffset, vstate->vttable);
}

static void RememberTP(const NaClPcAddress src, NaClPcAddress target,
                       struct NCValidatorState *vstate) {
  const NaClMemorySize ioffset =  target - vstate->iadrbase;

  if (vstate->iadrbase <= target && target < vstate->iadrlimit) {
    SetAdrTable(ioffset, vstate->kttable);
  }
  else if ((target & vstate->alignmask) == 0) {
    /* Allow bundle-aligned jumps. */
  }
  else {
    ValidatePrintError(src, "JUMP TARGET out of range", vstate);
    Stats_BadTarget(vstate);
  }
}

static void ForgetIP(const NaClPcAddress ip,
                     struct NCValidatorState *vstate) {
  NaClMemorySize ioffset =  ip - vstate->iadrbase;
  if (ip < vstate->iadrbase || ip >= vstate->iadrlimit) {
    ValidatePrintError(ip, "JUMP TARGET out of range in ForgetIP", vstate);
    Stats_BadTarget(vstate);
    return;
  }
  ClearAdrTable(ioffset, vstate->vttable);
}

int NCValidateFinish(struct NCValidatorState *vstate) {
  uint32_t offset;
  if (vstate == NULL) {
    vprint(vstate,
           (reporter,
            "validator not initialized. Did you call ncvalidate_init()?\n"));
    /* non-zero indicates failure */
    return 1;
  }
  /* If we are stubbing out code, the following checks don't provide any
   * usefull information, so quit early.
   */
  if (vstate->do_stub_out) return vstate->stats.sawfailure;
  dprint(("CheckTargets: %"NACL_PRIxNaClPcAddress"-%"NACL_PRIxNaClPcAddress"\n",
          vstate->iadrbase, vstate->iadrlimit));
  for (offset = 0;
       offset < vstate->iadrlimit - vstate->iadrbase;
       offset += 1) {
    if (GetAdrTable(offset, vstate->kttable)) {
      /* printf("CheckTarget %x\n", offset + iadrbase); */
      Stats_CheckTarget(vstate);
      if (!GetAdrTable(offset, vstate->vttable)) {
        ValidatePrintError(vstate->iadrbase + offset,
                           "Bad jump target", vstate);
        Stats_BadTarget(vstate);
      }
    }
  }
  /* check basic block boundaries */
  if (vstate->iadrbase & vstate->alignmask) {
    /* This should never happen because the alignment of iadrbase is */
    /* checked in NCValidateInit(). */
    ValidatePrintError(vstate->iadrbase, "Bad base address alignment", vstate);
    Stats_BadAlignment(vstate);
  }
  for (offset = 0; offset < vstate->iadrlimit - vstate->iadrbase;
       offset += vstate->alignment) {
    if (!GetAdrTable(offset, vstate->vttable)) {
      ValidatePrintError(vstate->iadrbase + offset,
                         "Bad basic block alignment", vstate);
      Stats_BadAlignment(vstate);
    }
  }
  fflush(stdout);

  /* Now that all the work is done, generate return code. */
  /* Return zero if there are no problems.                */
  return (vstate->stats.sawfailure);
}

void NCValidateFreeState(struct NCValidatorState **vstate) {
  if (*vstate == NULL) return;
  free((*vstate)->vttable);
  free((*vstate)->kttable);
  free(*vstate);
  *vstate = NULL;
}

/* ValidateSFenceClFlush is called for the sfence/clflush opcode 0f ae /7 */
/* It returns 0 if the current instruction is implemented, and 1 if not.  */
static int ValidateSFenceClFlush(const NCDecoderInst *dinst) {
  NCValidatorState* vstate = VALIDATOR_STATE_DOWNCAST(dinst->dstate);
  uint8_t mrm = NCInstBytesByte(&dinst->inst_bytes, 2);

  if (modrm_mod(mrm) == 3) {
    /* this is an sfence */
    if (vstate->cpufeatures.f_FXSR) return 0;
    return 1;
  } else {
    /* this is an clflush */
    if (vstate->cpufeatures.f_CLFLUSH) return 0;
    return 1;
  }
}

static void ValidateCallAlignment(const NCDecoderInst *dinst) {
  NaClPcAddress fallthru = dinst->vpc + dinst->inst.bytes.length;
  struct NCValidatorState* vstate = VALIDATOR_STATE_DOWNCAST(dinst->dstate);
  if (fallthru & vstate->alignmask) {
    ValidatePrintError(dinst->vpc, "Bad call alignment", vstate);
    /* This makes bad call alignment a fatal error. */
    Stats_BadAlignment(vstate);
  }
}

static void ValidateJmp8(const NCDecoderInst *dinst) {
  uint8_t opcode = NCInstBytesByte(&dinst->inst_bytes,
                                   dinst->inst.prefixbytes);
  int8_t offset = NCInstBytesByte(&dinst->inst_bytes,
                                  dinst->inst.prefixbytes+1);
  struct NCValidatorState* vstate = VALIDATOR_STATE_DOWNCAST(dinst->dstate);
  NaClPcAddress target =
      dinst->vpc + dinst->inst.bytes.length + offset;
  Stats_CheckTarget(vstate);
  if ((opcode & 0xf0) == 0x70 || opcode == 0xeb ||
      opcode == 0xe0 || opcode == 0xe1 || opcode == 0xe2 || opcode == 0xe3) {
    RememberTP(dinst->vpc, target, vstate);
  } else {
    /* If this ever happens, it's probably a decoder bug. */
    vprint(vstate, (reporter, "ERROR: JMP8 %"NACL_PRIxNaClPcAddress": %x\n",
                    dinst->vpc, opcode));
    Stats_InternalError(vstate);
  }
}

static void ValidateJmpz(const NCDecoderInst *dinst) {
  NCInstBytesPtr opcode;
  uint8_t opcode0;
  int32_t offset;
  NaClPcAddress target;
  NCValidatorState* vstate = VALIDATOR_STATE_DOWNCAST(dinst->dstate);
  NCInstBytesPtrInitInc(&opcode, &dinst->inst_bytes,
                        dinst->inst.prefixbytes);
  opcode0 = NCInstBytesByte(&opcode, 0);
  Stats_CheckTarget(vstate);
  if (opcode0 == 0xe8 || opcode0 == 0xe9) {
    NCInstBytesPtr opcode_1;
    NCInstBytesPtrInitInc(&opcode_1, &opcode, 1);
    offset = NCInstBytesInt32(&opcode_1);
    target = dinst->vpc + dinst->inst.bytes.length + offset;
    RememberTP(dinst->vpc, target, vstate);
    /* as a courtesy, check call alignment correctness */
    if (opcode0 == 0xe8) ValidateCallAlignment(dinst);
  } else if (opcode0 == 0x0f) {
    uint8_t opcode1 = NCInstBytesByte(&opcode, 1);
    if ((opcode1 & 0xf0) == 0x80) {
      NCInstBytesPtr opcode_2;
      NCInstBytesPtrInitInc(&opcode_2, &opcode, 2);
      offset = NCInstBytesInt32(&opcode_2);
      target = dinst->vpc + dinst->inst.bytes.length + offset;
      RememberTP(dinst->vpc, target, vstate);
    }
  } else {
    /* If this ever happens, it's probably a decoder bug. */
    uint8_t opcode1 = NCInstBytesByte(&opcode, 1);
    vprint(vstate,
           (reporter, "ERROR: JMPZ %"NACL_PRIxNaClPcAddress": %x %x\n",
            dinst->vpc, opcode0, opcode1));
    Stats_InternalError(vstate);
  }
}

/*
 * The NaCl five-byte safe indirect calling sequence looks like this:
 *   83 e0 f0                 and  $0xfffffff0,%eax
 *   ff d0                    call *%eax
 * The call may be replaced with a ff e0 jmp. Any register may
 * be used, not just eax. The validator requires exactly this
 * sequence.
 * TODO(brad): validate or write the masks.
 */
static void ValidateIndirect5(const NCDecoderInst *dinst) {
  NCInstBytesPtr jmpopcode;
  NCInstBytesPtr andopcode;
  uint8_t               mrm;
  uint8_t               targetreg;
  const uint8_t         kReg_ESP = 4;
  NCValidatorState* vstate = VALIDATOR_STATE_DOWNCAST(dinst->dstate);

  struct NCDecoderInst *andinst = PreviousInst(dinst, 1);
  if ((andinst == NULL) || (andinst->inst.bytes.length != 3)) {
    BadInstructionError(dinst, "Unsafe indirect jump");
    Stats_UnsafeIndirect(vstate);
    return;
  }
  /* note: no prefixbytes allowed */
  NCInstBytesPtrInitInc(&jmpopcode, &dinst->inst_bytes, 0);
  /* note: no prefixbytes allowed */
  NCInstBytesPtrInitInc(&andopcode, &andinst->inst_bytes, 0);
  mrm = NCInstBytesByte(&jmpopcode, 1);
  targetreg = modrm_rm(mrm);  /* Note that the modrm_rm field holds the   */
                              /* target addr the modrm_reg is the opcode. */

  Stats_CheckTarget(vstate);
  Stats_TargetIndirect(vstate);
  do {
    /* no prefix bytes allowed */
    if (dinst->inst.prefixbytes != 0) break;
    if (dinst->inst.prefixbytes != 0) break;
    /* Check all the opcodes. */
    /* In GROUP5, 2 => call, 4 => jmp */
    if (NCInstBytesByte(&jmpopcode, 0) != 0xff) break;
    if ((modrm_reg(mrm) != 2) && (modrm_reg(mrm) != 4)) break;
    /* Issue 32: disallow unsafe call/jump indirection */
    /* example:    ff 12     call (*edx)               */
    /* Reported by defend.the.world on 11 Dec 2008     */
    if (modrm_mod(mrm) != 3) break;
    if (targetreg == kReg_ESP) break;
    if (NCInstBytesByte(&andopcode, 0) != 0x83) break;
    /* check modrm bytes of or and and instructions */
    if (NCInstBytesByte(&andopcode, 1) != (0xe0 | targetreg)) break;
    /* check mask */
    if (NCInstBytesByte(&andopcode, 2) !=
        (0x0ff & ~vstate->alignmask)) break;
    /* All checks look good. Make the sequence 'atomic.' */
    ForgetIP(dinst->vpc, vstate);
    /* as a courtesy, check call alignment correctness */
    if (modrm_reg(mrm) == 2) ValidateCallAlignment(dinst);
    return;
  } while (0);
  BadInstructionError(dinst, "Unsafe indirect jump");
  Stats_UnsafeIndirect(vstate);
}

/* Checks if the set of prefixes are allowed for the instruction.
 * By default, we only allow prefixes if they have been allowed
 * by the bad prefix mask generated inside ncdecode_table.c.
 * These masks are defined by the NaClInstType of the instruction.
 * See ncdecode_table.c for more details on how these masks are set.
 *
 * Currently:
 * Only 386, 386L, 386R, 386RE instruction allow  Data 16
 * (unless used as part of instruction selection in a multibyte instruction).
 * Only 386, JMP8, and JMPZ allow segment registers prefixes.
 * Only 386L and CMPXCHG8B allow the LOCK prefix.
 * Only 386R and 386RE instructions allow the REP prefix.
 * Only 386RE instructions allow the REPNE prefix.
 *
 * Note: The prefixmask does not include the prefix value (if any) used to
 * select multiple byte instructions. Such prefixes have been moved to
 * opcode_prefixmask, so that the selection (based on that prefix) has
 * been recorded.
 *
 * In general, we do not allow multiple prefixes. Exceptions are as
 * follows:
 *   1 - Data 16 is allowed on lock instructions, so that 2 byte values
 *       can be locked.
 *   2 - Multibyte instructions that are selected
 *       using prefix values Data 16, REP and REPNE, can only have
 *       one of these prefixes (Combinations of these three prefixes
 *       are not allowed for such multibyte instructions).
 *   3 - Locks are only allowed on instructions with type 386L. The
 *       exception is inst cmpxch8b, which also can have a lock.
 *   4 - The only two prefix byte combination allowed is Data 16 and Lock.
 *   5 - Long nops that are hard coded can contain more than one prefix.
 *       See ncdecode.c for details (they don't use ValidatePrefixes).
 */
static Bool ValidatePrefixes(const NCDecoderInst *dinst) {
  if (dinst->inst.prefixbytes == 0) return TRUE;

  if ((dinst->inst.prefixmask &
       BadPrefixMask[dinst->opinfo->insttype]) != 0) {
    return FALSE;
  }

  /* If a multibyte instruction is using a selection prefix, be
   * sure that there is no conflict with other selection prefixes.
   */
  if ((dinst->inst.opcode_prefixmask != 0) &&
      ((dinst->inst.prefixmask &
        (kPrefixDATA16 | kPrefixREPNE | kPrefixREP)) != 0)) {
    return FALSE;
  }

  /* Only allow a lock if it is a 386L instruction, or the special
   * cmpxchg8b instruction.
   */
  if (dinst->inst.prefixmask & kPrefixLOCK) {
    if ((dinst->opinfo->insttype != NACLi_386L) &&
        (dinst->opinfo->insttype != NACLi_CMPXCHG8B)) {
      return FALSE;
    }
  }

  /* Only allow more than one prefix if two prefixes, and they are
   * data 16 and lock.
   */
  if ((dinst->inst.prefixbytes > 1) &&
      !((dinst->inst.prefixbytes == 2) &&
        (dinst->inst.prefixmask == (kPrefixLOCK | kPrefixDATA16)))) {
    return FALSE;
  }

  return TRUE;
}

static const size_t kMaxValidInstLength = 11;

/* The modrm mod field is a two-bit value. Values 00, 01, and, 10
 * define memory references. Value 11 defines register accesses instead
 * of memory.
 */
static const int kModRmModFieldDefinesRegisterRef = 0x3;

static Bool ValidateInst(const NCDecoderInst *dinst) {
  CPUFeatures *cpufeatures;
  int squashme = 0;
  NCValidatorState* vstate;
  if (dinst == NULL) return TRUE;
  vstate = VALIDATOR_STATE_DOWNCAST(dinst->dstate);

 /*  dprint(("ValidateInst(%x, %x) at %x\n",
      (uint32_t)dinst, (uint32_t)vstate, dinst->vpc)); */

  OpcodeHisto(NCInstBytesByte(&dinst->inst_bytes, dinst->inst.prefixbytes),
              vstate);
  RememberIP(dinst->vpc, vstate);

  cpufeatures = &(vstate->cpufeatures);

  if (!ValidatePrefixes(dinst)) {
    BadInstructionError(dinst, "Bad prefix usage");
    Stats_BadPrefix(vstate);
  }

  if ((dinst->opinfo->insttype != NACLi_NOP) &&
      ((size_t) (dinst->inst.bytes.length - dinst->inst.prefixbytes)
       > kMaxValidInstLength)) {
    BadInstructionError(dinst, "Instruction too long");
    Stats_BadInstLength(vstate);
  }

  switch (dinst->opinfo->insttype) {
    case NACLi_NOP:
    case NACLi_386:
    case NACLi_386L:
    case NACLi_386R:
    case NACLi_386RE:
      break;
    case NACLi_JMP8:
      ValidateJmp8(dinst);
      break;
    case NACLi_JMPZ:
      ValidateJmpz(dinst);
      break;
    case NACLi_INDIRECT:
      ValidateIndirect5(dinst);
      break;
    case NACLi_X87:
      squashme = (!cpufeatures->f_x87);
      break;
    case NACLi_SFENCE_CLFLUSH:
      squashme = ValidateSFenceClFlush(dinst);
      break;
    case NACLi_CMPXCHG8B:
      /* Only allow if the modrm mod field accesses memory.
       * This stops us from accepting f00f on multiple bytes.
       * http://en.wikipedia.org/wiki/Pentium_F00F_bug
       */
      if (modrm_mod(dinst->inst.mrm) == kModRmModFieldDefinesRegisterRef) {
        BadInstructionError(dinst, "Illegal instruction");
        Stats_IllegalInst(vstate);
      }
      squashme = (!cpufeatures->f_CX8);
      break;
    case NACLi_CMOV:
      squashme = (!cpufeatures->f_CMOV);
      break;
    case NACLi_FCMOV:
      squashme = (!(cpufeatures->f_CMOV && cpufeatures->f_x87));
      break;
    case NACLi_RDTSC:
      squashme = (!cpufeatures->f_TSC);
      break;
    case NACLi_MMX:
      squashme = (!cpufeatures->f_MMX);
      break;
    case NACLi_MMXSSE2:
      /* Note: We accept these instructions if either MMX or SSE2 bits */
      /* are set, in case MMX instructions go away someday...          */
      squashme = (!(cpufeatures->f_MMX || cpufeatures->f_SSE2));
      break;
    case NACLi_SSE:
      squashme = (!cpufeatures->f_SSE);
      break;
    case NACLi_SSE2:
      squashme = (!cpufeatures->f_SSE2);
      break;
    case NACLi_SSE3:
      squashme = (!cpufeatures->f_SSE3);
      break;
    case NACLi_SSE4A:
      squashme = (!cpufeatures->f_SSE4A);
      break;
    case NACLi_SSE41:
      squashme = (!cpufeatures->f_SSE41);
      break;
    case NACLi_SSE42:
      squashme = (!cpufeatures->f_SSE42);
      break;
    case NACLi_MOVBE:
      squashme = (!cpufeatures->f_MOVBE);
      break;
    case NACLi_POPCNT:
      squashme = (!cpufeatures->f_POPCNT);
      break;
    case NACLi_LZCNT:
      squashme = (!cpufeatures->f_LZCNT);
      break;
    case NACLi_SSSE3:
      squashme = (!cpufeatures->f_SSSE3);
      break;
    case NACLi_3DNOW:
      squashme = (!cpufeatures->f_3DNOW);
      break;
    case NACLi_E3DNOW:
      squashme = (!cpufeatures->f_E3DNOW);
      break;
    case NACLi_SSE2x:
      /* This case requires CPUID checking code */
      /* Note: DATA16 prefix required. The generated table
       * for group 14 (which the only 2 SSE2x instructions are in),
       * allows instructions with and without a 66 prefix. However,
       * the SSE2x instructions psrldq and pslldq are only allowed
       * with the 66 prefix. Hence, this code has been added to
       * do this check.
       */
      if (!(dinst->inst.opcode_prefixmask & kPrefixDATA16)) {
        BadInstructionError(dinst, "Bad prefix usage");
        Stats_BadPrefix(vstate);
      }
      squashme = (!cpufeatures->f_SSE2);
      break;

    case NACLi_RETURN:
      BadInstructionError(dinst, "ret instruction (not allowed)");
      Stats_Return(vstate);
      /* ... and fall through to illegal instruction code */
    case NACLi_EMMX:
      /* EMMX needs to be supported someday but isn't ready yet. */
    case NACLi_INVALID:
    case NACLi_ILLEGAL:
    case NACLi_SYSTEM:
    case NACLi_RDMSR:
    case NACLi_RDTSCP:
    case NACLi_SYSCALL:
    case NACLi_SYSENTER:
    case NACLi_LONGMODE:
    case NACLi_SVM:
    case NACLi_OPINMRM:
    case NACLi_3BYTE:
    case NACLi_CMPXCHG16B: {
        BadInstructionError(dinst, "Illegal instruction");
        Stats_IllegalInst(vstate);
        break;
      }
    case NACLi_UNDEFINED: {
        BadInstructionError(dinst, "Undefined instruction");
        Stats_IllegalInst(vstate);
        Stats_InternalError(vstate);
        break;
      }
    default:
      BadInstructionError(dinst, "Undefined instruction type");
      Stats_InternalError(vstate);
      break;
  }
  if (squashme) memset(dinst->dstate->memory.mpc, kNaClFullStop,
                       dinst->dstate->memory.read_length);
  return TRUE;
}

/*
 * Validate that two nacljmps are byte-for-byte identical.  Note that
 * one of the individual jumps must be validated in isolation with
 * ValidateIndirect5() before this is called.
 */
static void ValidateIndirect5Replacement(NCDecoderInst *dinst_old,
                                         NCDecoderInst *dinst_new) {
  do {
    /* check that the and-guard is 3 bytes and bit-for-bit identical */
    NCDecoderInst *andinst_old = PreviousInst(dinst_old, 1);
    NCDecoderInst *andinst_new = PreviousInst(dinst_new, 1);
    if ((andinst_old == NULL) || (andinst_old->inst.bytes.length != 3)) break;
    if ((andinst_new == NULL) || (andinst_new->inst.bytes.length != 3)) break;
    if (memcmp(andinst_old->inst.bytes.byte,
               andinst_new->inst.bytes.byte, 3) != 0) break;

    /* check that the indirect-jmp is 2 bytes and bit-for-bit identical */
    if (dinst_old->inst.bytes.length != 2) break;
    if (dinst_new->inst.bytes.length != 2) break;
    if (memcmp(dinst_old->inst.bytes.byte,
               dinst_new->inst.bytes.byte, 2) != 0) break;

    return;
  } while (0);
  BadInstructionError(dinst_new,
                      "Replacement indirect jump must match original");
  Stats_UnsafeIndirect(VALIDATOR_STATE_DOWNCAST(dinst_new->dstate));
}

/*
 * Check that mstate_new is a valid replacement instruction for mstate_old.
 * Note that mstate_old was validated when it was inserted originally.
 */
static Bool ValidateInstReplacement(NCDecoderStatePair* tthis,
                                    NCDecoderInst *dinst_old,
                                    NCDecoderInst *dinst_new) {
  /* Only validate individual instructions that have changed. */
  if (memcmp(dinst_old->inst.bytes.byte,
             dinst_new->inst.bytes.byte,
             dinst_new->inst.bytes.length)) {
    /* Call single instruction validator first, will call ValidateIndirect5() */
    ValidateInst(dinst_new);
  } else {
    /* Still need to record there is an intruction here for NCValidateFinish()
     * to verify basic block alignment.
     */
    RememberIP(dinst_new->vpc, VALIDATOR_STATE_DOWNCAST(dinst_new->dstate));
  }

  if (dinst_old->opinfo->insttype == NACLi_INDIRECT
    || dinst_new->opinfo->insttype == NACLi_INDIRECT) {
    /* Verify that nacljmps never change */
    ValidateIndirect5Replacement(dinst_old, dinst_new);
  }
  return TRUE;
}

/* Create the decoder state for the validator state, using the
 * given parameters.
 */
static void NCValidateDStateInit(NCValidatorState *vstate,
                                 uint8_t *mbase, NaClPcAddress vbase,
                                 NaClMemorySize sz) {
  /* TODO(karl): Refactor this so that NCValidatorState properly
   * inherits from NCDecoderState. This means refactoring the
   * API to have a constructor that does the following.
   */
  NCDecoderState* dstate = &vstate->dstate;
  NCDecoderStateConstruct(dstate, mbase, vbase, sz,
                          vstate->inst_buffer, kNCValidatorInstBufferSize);
  dstate->action_fn = ValidateInst;
  dstate->new_segment_fn = (NCDecoderStateMethod) Stats_NewSegment;
  dstate->segmentation_error_fn = (NCDecoderStateMethod) Stats_SegFault;
  dstate->internal_error_fn = (NCDecoderStateMethod) Stats_InternalError;
}

void NCValidateSegment(uint8_t *mbase, NaClPcAddress vbase, NaClMemorySize sz,
                       struct NCValidatorState *vstate) {
  NCHaltTrimSegment(mbase, vbase, vstate->alignment, &sz, &vstate->iadrlimit);
  if (sz == 0) {
    ValidatePrintError(0, "Bad text segment (zero size)", vstate);
    Stats_SegFault(vstate);
    return;
  } else {
    /* Note: Based on the current API, we must grab the error reporter
     * and reinstall it, after the call to NCValidateDStateInit, so that
     * we don't replace it with the default error reporter.
     */
    /* TODO(karl): Create a constructor for a decoder state so that
     * it doesn't require the mbase, vbase, and sz values, so that
     * the constructor can be called whe4n the validator state is
     * created.
     */
    NCDecoderState* dstate = &vstate->dstate;
    NaClErrorReporter* reporter = dstate->error_reporter;
    NCValidateDStateInit(vstate, mbase, vbase, sz);
    NCDecoderStateSetErrorReporter(dstate, reporter);
    NCDecoderStateDecode(dstate);
    NCDecoderStateDestruct(dstate);
  }
}

int NCValidateSegmentPair(uint8_t *mbase_old, uint8_t *mbase_new,
                          NaClPcAddress vbase, size_t sz,
                          uint8_t alignment) {
  /* TODO(karl): Refactor to use inheritance from NCDecoderStatePair? */
  NCDecoderStatePair pair;
  NCValidatorState* new_vstate;
  NCValidatorState* old_vstate;

  int result = 0;

  /* Verify that we actually have a segment to walk. */
  if (sz == 0) {
    printf("VALIDATOR: %"NACL_PRIxNaClPcAddress
           ": Bad text segment (zero size)\n", vbase);
    return 0;
  }

  old_vstate = NCValidateInit(vbase, vbase + sz, alignment);
  if (old_vstate != NULL) {
    NCValidateDStateInit(old_vstate, mbase_old, vbase, sz);
    new_vstate = NCValidateInit(vbase, vbase + sz, alignment);
    if (new_vstate != NULL) {
      NCValidateDStateInit(new_vstate, mbase_new, vbase, sz);

      NCDecoderStatePairConstruct(&pair,
                                  &old_vstate->dstate,
                                  &new_vstate->dstate);
      pair.action_fn = ValidateInstReplacement;
      if (NCDecoderStatePairDecode(&pair)) {
        result = 1;
      } else {
        ValidatePrintError(vbase, "Replacement not applied!\n", new_vstate);
      }
      if (NCValidateFinish(new_vstate)) {
        /* Errors occurred during validation. */
        result = 0;
      }
      NCDecoderStatePairDestruct(&pair);
      NCDecoderStateDestruct(&new_vstate->dstate);
      NCValidateFreeState(&new_vstate);
    }
    NCDecoderStateDestruct(&new_vstate->dstate);
    NCValidateFreeState(&new_vstate);
  }
  return result;
}
