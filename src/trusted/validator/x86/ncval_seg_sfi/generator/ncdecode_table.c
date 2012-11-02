/* Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncdecode_intable.h - table driven decoder for Native Client.
 *
 * This program generates C source for the ncdecode.c decoder tables.
 * It writes two C .h file to standard out, one for decoding and one
 * for disassembly, that contain six decoder tables.
 *
 * Note: Most of the organization of this document is based on the
 * Opcode Map appendix of one of the following documents:

 * (1) "Intel 64 and IA-32 Architectures
 * Software Developer's Manual (volume 2b, document 253665.pdf)".
 *
 * (2) "Intel 80386 Reference Programmer's Manual" (document
 * http://pdos.csail.mit.edu/6.828/2004/readings/i386/toc.htm).
 */


#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode.h"

typedef uint8_t bool;   /* zero or non-zero */
#define TRUE 1
#define FALSE 0

#define TODO(who, what)

/* Possible run modes for instructions. */
typedef enum {
  X86_32,       /* Model x86-32 bit instructions. */
  X86_64,       /* Model x86-64-bit instructions. */
  RunModeSize   /* Special end of list marker, denoting the number
                 * of run modes;
                 */
} RunMode;

/* Returns the print name of the given run mode. */
static const char* RunModeName(RunMode mode) {
  switch (mode) {
    case X86_32: return "x86-32 bit mode";
    case X86_64: return "x86-64 bit mode";
    default: assert(0);
  }

  /* NOTREACHED */
  return NULL;
}

/* Defines the run mode files that should be generated. */
static RunMode FLAGS_run_mode = RunModeSize;

/* Generate names for DecodeOpsKind values. */
static const char* DecodeOpsKindName(DecodeOpsKind kind) {
  switch (kind) {
    case DECODE_OPS_DEFAULT_64: return "DECODE_OPS_DEFAULT_64";
    case DECODE_OPS_DEFAULT_32: return "DECODE_OPS_DEFAULT_32";
    case DECODE_OPS_FORCE_64:   return "DECODE_OPS_FORCE_64";
    default: assert(0);
  }

  /* NOTREACHED */
  return NULL;
}

/* For handling prefixes we consume the prefix byte and then
 * record the state in this record. Note: In general, we define two versions
 * of each instruction - 0 for X86_32 and 1 for X86_64.
 */
typedef struct OpMetaInfo {
  const char* disfmt;         /* as a short string, for printing */
  NaClInstType insttype;
  bool mrmbyte;         /* 0 or 1 */
  uint8_t immtype;      /* IMM_UNKNOWN .. IMM_DEFAULT_4 */
  uint8_t opinmrm;      /* 0 .. 8 */
} OpMetaInfo;

/* The decoder input table is an array of instruction definitions, */
/* defined using OpMetaInfo as defined in ncdecode.h               */

/* one byte opcode tables */
OpMetaInfo* g_Op1ByteTable[NCDTABLESIZE];

/* Defines encodings of prefix bytes. */
const char* g_PrefixTable[NCDTABLESIZE];

/* two byte opcode tables */
/* byte prefixed with byte OF */
OpMetaInfo* g_Op0FXXMetaTable[NCDTABLESIZE];
/* byte prefixed with bytes F2 OF */
OpMetaInfo* g_OpF20FXXTable[NCDTABLESIZE];
/* byte prefixed with bytes F3 OF */
OpMetaInfo* g_OpF30FXXTable[NCDTABLESIZE];
/* byte prefixed with bytes 66 OF */
OpMetaInfo* g_Op660FXXTable[NCDTABLESIZE];

/* three byte opcode tables */
/* byte prefixed with bytes 0F 0F */
OpMetaInfo* g_Op0F0FTable[NCDTABLESIZE];
/* byte prefixed with bytes 0F 38 */
OpMetaInfo* g_Op0F38Table[NCDTABLESIZE];
/* byte prefixed with bytes 66 0F 38 */
OpMetaInfo* g_Op660F38Table[NCDTABLESIZE];
/* byte prefixed with bytes F2 OF 38 */
OpMetaInfo* g_OpF20F38Table[NCDTABLESIZE];
/* byte prefixed with bytes 0F 3A */
OpMetaInfo* g_Op0F3ATable[NCDTABLESIZE];
/* byte prefixed with bytes 66 oF 3A */
OpMetaInfo* g_Op660F3ATable[NCDTABLESIZE];
/* tables for opcodes in ModRM */
OpMetaInfo* g_ModRMOpTable[kNaClMRMGroupsRange][kModRMOpcodeGroupSize];
/* x87 opcode tables */
OpMetaInfo* g_Op87D8[NCDTABLESIZE];
OpMetaInfo* g_Op87D9[NCDTABLESIZE];
OpMetaInfo* g_Op87DA[NCDTABLESIZE];
OpMetaInfo* g_Op87DB[NCDTABLESIZE];
OpMetaInfo* g_Op87DC[NCDTABLESIZE];
OpMetaInfo* g_Op87DD[NCDTABLESIZE];
OpMetaInfo* g_Op87DE[NCDTABLESIZE];
OpMetaInfo* g_Op87DF[NCDTABLESIZE];

/* Define the stategy to decode operands for a one byte instruction,
   when running in 64-bit mode.
 */
DecodeOpsKind g_OpDecodeOpsKind[NCDTABLESIZE];


/* Operand size associated with byte (when prefixed with byte 0F)
 * when in 64 bit mode.
 */
DecodeOpsKind g_Op0FDecodeOpsKind[NCDTABLESIZE];

/* Defines operand size of MRM opcode extensions for a group,
 * when in 64 bit mode.
 */
DecodeOpsKind
  g_ModRMOpDecodeOpsKind[kNaClMRMGroupsRange][kModRMOpcodeGroupSize];

/* Model an undefined instruction. */
static OpMetaInfo DecodeUndefinedInstInfo = {
  "undefined", NACLi_UNDEFINED, 0, IMM_NONE, 0
};

/* Note: in general all errors in this module will be fatal.
 * To debug: use gdb or your favorite debugger.
 */
static void fatal(const char* s) {
  fprintf(stderr, "%s\n", s);
  fprintf(stderr, "fatal error, cannot recover\n");
  exit(-1);
}

/* note that this function accepts only one string in s */
static char* aprintf(const char* fmt, const char* s) {
  char *rstring = NULL;
  /* there will be one spare byte, because %s is replaced, but it's ok */
  size_t length = strlen(fmt) + strlen(s);
  rstring = malloc(length);
  if (rstring != NULL) {
    SNPRINTF(rstring, length, fmt, s);
  } else {
    fprintf(stderr, "In aprintf (%s, %s):\n", fmt, s);
    fatal("malloc failed");
  }
  return rstring;
}

/* operandsize == 0 implies operand size determined by operand size attributes
 * opbytes: length of opcode, one or two bytes
 * mrmbyte: 0 or 1, 1 if mrmbyte is present
 * immbytes: bytes of immediate: 1, 2, 4
 * itype: decoder treatment, an NaClInstType as in ncdecode.h
 * disfmt: format string for disassembly
 */
static OpMetaInfo* NewMetaDefn(
    const bool mrmbyte, const uint8_t immtype,
    const NaClInstType itype, const char* disfmt) {
  OpMetaInfo *imd = (OpMetaInfo *)malloc(sizeof(*imd));
  if (imd == NULL) {
    fatal("NewMetaDefn: malloc failed");
  }
  imd->insttype = itype;
  imd->disfmt = disfmt;
  imd->mrmbyte = mrmbyte;
  imd->immtype = immtype;
  imd->opinmrm = 0;
  return imd;
}

/* The names of 32-bit general purpose registers. */
#define NUM_GP_X86_32_REGS 8
static const char* gp_x86_32_regs[NUM_GP_X86_32_REGS] = {
  "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi"
};

/* The corresponding names for general purpose registers
 * when generating x86-64.
 */
static const char* corresp_gp_x86_64_regs[] = {
  "%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi"
};

/* Converts the format string, written for x86-32, converted to the given
 * run mode.
 */
static const char* ModedGenRegs(const char* format, RunMode mode) {
  switch (mode) {
    case X86_32:
      return format;
    case X86_64: {
        /* Note: Assumes that the character size of the corresponding x86-64
         * bit general registers have the same size as the x86-32 bit
         * counterparts.
         */
        char* rformat = strdup(format);
        if (rformat != NULL) {
          int i;
          char* substr = rformat;
          for (i = 0; i < NUM_GP_X86_32_REGS; ++i) {
            size_t name_size = strlen(gp_x86_32_regs[i]);
            char* loc;
            assert(name_size == strlen(corresp_gp_x86_64_regs[i]));
            loc = strstr(substr, gp_x86_32_regs[i]);
            while (loc != NULL) {
              memcpy(loc, corresp_gp_x86_64_regs[i], name_size);
              loc = strstr(loc + name_size,  gp_x86_32_regs[i]);
            }
          }
        } else {
          fprintf(stderr, "In ModedGenRegs(%s, %s):\n",
                  format, RunModeName(mode));
          fatal("strdup failed");
        }
        return rformat;
      }
    default:
      assert(0);
  }
  /* NOTREACHED */
  return NULL;
}

/* Define the encoding for a one byte opcode, for the given run mode. */
static void EncodeModedOp(
    const uint8_t byte, const bool mrmbyte,
    const uint8_t immtype,
    const NaClInstType itype, const char* disfmt,
    const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_Op1ByteTable[byte] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}

/* Define the encoding for a one byte opcode, for all run modes. */
static void EncodeOp(const uint8_t byte, const bool mrmbyte,
                          const uint8_t immtype,
                          const NaClInstType itype, const char* disfmt) {
  EncodeModedOp(byte, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Define the prefix name for the given opcode, for the given run mode. */
static void EncodeModedPrefixName(const uint8_t byte, const char* name,
                                  const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_PrefixTable[byte] = name;
  }
}

/* Define the prefix name for the given opcode, for all run modes. */
static void EncodePrefixName(const uint8_t byte, const char* name) {
  EncodeModedPrefixName(byte, name, FLAGS_run_mode);
}

/* Define the encoding for a one byte opcode, for all run modes, fixing
 * the names of general purpose registers to match the run mode.
 */
static void EncodeOpRegs(const uint8_t byte, const bool mrmbyte,
                              const uint8_t immtype,
                              const NaClInstType itype, const char* disfmt) {
  EncodeModedOp(byte, mrmbyte, immtype, itype,
                ModedGenRegs(disfmt, FLAGS_run_mode), FLAGS_run_mode);
}

static void UpdateDecodeOpsKind(
    const char* fn_name,
    DecodeOpsKind* gtbl,
    uint8_t byte,
    DecodeOpsKind new_kind) {
  DecodeOpsKind was_kind = gtbl[byte];
  if (was_kind != new_kind && was_kind != DECODE_OPS_DEFAULT_32) {
    printf("*ERROR*: Incompatable update to %s[%02x] from %s to %s\n",
           fn_name, byte, DecodeOpsKindName(was_kind),
           DecodeOpsKindName(new_kind));
  }
  gtbl[byte] = new_kind;
}

/* Mark one byte opcode to define instruction that defaults to a 64-bit operand
 * size and can't encode 32 bit operand size, when run in 64-bit mode.
 */
static void SetOpDefaultSize64(const uint8_t byte) {
  UpdateDecodeOpsKind("SetOpDefaultSize64",
                      g_OpDecodeOpsKind,
                      byte,
                      DECODE_OPS_DEFAULT_64);
}

/* Mark one byte opcode to define instructions whose operand size must be
 * 64-bit, when run in 64-bit mode. (prefixes that change operand size are
 * ignored for the instruction when in 64-bit mode).
 */
static void SetOpForceSize64(const uint8_t byte) {
  UpdateDecodeOpsKind("SetOpForceSize64",
                      g_OpDecodeOpsKind,
                      byte,
                      DECODE_OPS_FORCE_64);
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * byte OF), for the given run mode.
 */
static void EncodeModedOp0F(const uint8_t byte, const bool mrmbyte,
                            const uint8_t immtype,
                            const NaClInstType itype, const char* disfmt,
                            const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_Op0FXXMetaTable[byte] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}

/* Define the encoding for a byte opcode (when the byte is prefixed with with
 * byte 0F), for all run modes.
 */
static void EncodeOp0F(const uint8_t byte, const bool mrmbyte,
                       const uint8_t immtype,
                       const NaClInstType itype, const char* disfmt) {
  EncodeModedOp0F(byte, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Mark byte opcode (when the byte is prefixed with byte 0F) to define
 * instructions that defaults to a 64-bit operand sizse and can't encode 32
 * bit operand size, when run in 64-bit mode.
 */
static void SetOp0FDefaultSize64(const uint8_t byte) {
  UpdateDecodeOpsKind("SetOp0FDefaultSize64",
                      g_Op0FDecodeOpsKind,
                      byte,
                      DECODE_OPS_DEFAULT_64);
}

/* Mark byte opcode (when the byte is prefixed with byte 0F) to define
 * instructions whose operand size must be 64-bit, when run in 64-bit mode.
 * (prefixes that change operand size are ignored for the instruction when
 * in 64-bit mode).
 */
static void SetOp0FForceSize64(const uint8_t byte) {
  UpdateDecodeOpsKind("SetOp0FForceSize64",
                      g_Op0FDecodeOpsKind,
                      byte,
                      DECODE_OPS_FORCE_64);
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes 66 0F), for the given run mode.
 */
static void EncodeModedOp660F(const uint8_t byte, const bool mrmbyte,
                              const uint8_t immtype,
                              const NaClInstType itype, const char* disfmt,
                              const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_Op660FXXTable[byte] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}

/* Define the encodintg for a byte opcode (when the byte is prefixed with
 * bytes 66 0F), for all run modes.
 */
static void EncodeOp660F(const uint8_t byte2, const bool mrmbyte,
                         const uint8_t immtype,
                         const NaClInstType itype, const char* disfmt) {
  EncodeModedOp660F(byte2, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes F2 0F), for the given run mode.
 */
static void EncodeModedOpF20F(const uint8_t byte, const bool mrmbyte,
                              const uint8_t immtype,
                              const NaClInstType itype, const char* disfmt,
                              const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_OpF20FXXTable[byte] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes F2 OF), for all run modes.
 */
static void EncodeOpF20F(const uint8_t byte, const bool mrmbyte,
                         const uint8_t immtype,
                         const NaClInstType itype, const char* disfmt) {
  EncodeModedOpF20F(byte, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes F3 0F), for the given run mode.
 */
static void EncodeModedOpF30F(const uint8_t byte2, const bool mrmbyte,
                              const uint8_t immtype,
                              const NaClInstType itype, const char* disfmt,
                              const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_OpF30FXXTable[byte2] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes F3 OF), for all run modes.
 */
static void EncodeOpF30F(const uint8_t byte, const bool mrmbyte,
                         const uint8_t immtype,
                         const NaClInstType itype, const char* disfmt) {
  EncodeModedOpF30F(byte, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes 0F 0F), for the given run mode.
 */
static void EncodeModedOp0F0F(const uint8_t byte, const bool mrmbyte,
                              const uint8_t immtype,
                              const NaClInstType itype, const char* disfmt,
                              const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_Op0F0FTable[byte] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes 0F 0F), for all run modes.
 */
static void EncodeOp0F0F(const uint8_t byte, const bool mrmbyte,
                         const uint8_t immtype,
                         const NaClInstType itype, const char* disfmt) {
  EncodeModedOp0F0F(byte, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes 0F 38), for the given run mode.
 */
static void EncodeModedOp0F38(const uint8_t byte, const bool mrmbyte,
                              const uint8_t immtype,
                              const NaClInstType itype, const char* disfmt,
                              const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_Op0F38Table[byte] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes 0F 38), for all run modes.
 */
static void EncodeOp0F38(const uint8_t byte, const bool mrmbyte,
                         const uint8_t immtype,
                         const NaClInstType itype, const char* disfmt) {
  EncodeModedOp0F38(byte, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Defines the encoding for a byte opcode (when the byte is prefixed with
 * bytes 66 0F 38), for the given run mode.
 */
static void EncodeModedOp660F38(const uint8_t byte, const bool mrmbyte,
                                const uint8_t immtype,
                                const NaClInstType itype, const char* disfmt,
                                const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_Op660F38Table[byte] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}

/* Defines the encoding for a byte opcode (when the byte is prefixed with
 * bytes 66 0F 38), for all run modes.
 */
static void EncodeOp660F38(const uint8_t byte, const bool mrmbyte,
                           const uint8_t immtype,
                           const NaClInstType itype, const char* disfmt) {
  EncodeModedOp660F38(byte, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Defines the encoding for a byte opcode (when the byte is prefixed with
 * bytes F2 0F 38), for the given run mode.
 */
static void EncodeModedOpF20F38(const uint8_t byte, const bool mrmbyte,
                                const uint8_t immtype,
                                const NaClInstType itype, const char* disfmt,
                                const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_OpF20F38Table[byte] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}


/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes F2 0F 38), for all run modes.
 */
static void EncodeOpF20F38(const uint8_t byte, const bool mrmbyte,
                           const uint8_t immtype,
                           const NaClInstType itype, const char* disfmt) {
  EncodeModedOpF20F38(byte, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes 0F 3A), for the given run mode.
 */
static void EncodeModedOp0F3A(const uint8_t byte, const bool mrmbyte,
                              const uint8_t immtype,
                              const NaClInstType itype, const char* disfmt,
                              const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_Op0F3ATable[byte] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes 0F 3A), for all run modes.
 */
static void EncodeOp0F3A(const uint8_t byte, const bool mrmbyte,
                         const uint8_t immtype,
                         const NaClInstType itype, const char* disfmt) {
  EncodeModedOp0F3A(byte, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes 66 0F 3A), for the given run mode.
 */
static void EncodeModedOp660F3A(const uint8_t byte, const bool mrmbyte,
                                const uint8_t immtype,
                                const NaClInstType itype, const char* disfmt,
                                const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    g_Op660F3ATable[byte] = NewMetaDefn(mrmbyte, immtype, itype, disfmt);
  }
}

/* Define the encoding for a byte opcode (when the byte is prefixed with
 * bytes 66 0F 3A), for all run modes.
 */
static void EncodeOp660F3A(const uint8_t byte, const bool mrmbyte,
                           const uint8_t immtype,
                           const NaClInstType itype, const char* disfmt) {
  EncodeModedOp660F3A(byte, mrmbyte, immtype, itype, disfmt, FLAGS_run_mode);
}

/* Define the encoding of opcodes in the nnn field (bits 3-5) of the ModR/M
 * byte, for the given instruction group, for the given run mode.
 */
static void EncodeModedModRMOp(const uint8_t group, const uint8_t nnn,
                               const NaClInstType itype, const char* disfmt,
                               const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    OpMetaInfo *idefn = NewMetaDefn(1, IMM_NONE, itype, disfmt);
    g_ModRMOpTable[group][nnn] = idefn;
  }
}

/* Define the encoding of opcodes in the nnn field (bits 3-5) of the ModR/M
 * byte, for the given instruction group, for all run modes.
 */
static void EncodeModRMOp(const uint8_t group, const uint8_t nnn,
                          const NaClInstType itype, const char* disfmt) {
  EncodeModedModRMOp(group, nnn, itype, disfmt, FLAGS_run_mode);
}

/* Define the encoding of opcodes in the nnn field (bits 3-5) of the ModR/M
 * byte, for the given instruction group, for all run modes, fixing the
 * names of general purpose registers to match the run mode.
 */
static void EncodeModRMOpRegs(const uint8_t group, const uint8_t nnn,
                              const NaClInstType itype, const char* disfmt) {
  EncodeModedModRMOp(group, nnn, itype,
                     ModedGenRegs(disfmt, FLAGS_run_mode), FLAGS_run_mode);
}

/* Mark MRM opcode extension to define instructions that defauilts to a 64-bit
 * operand size and can't encode 32 bit operand size, when in 64-bit mode.
 */
static void SetModRMOpDefaultSize64(
    const uint8_t group, const uint8_t nnn) {
  UpdateDecodeOpsKind("SetModRMOpDefaultSize64",
                      g_ModRMOpDecodeOpsKind[group],
                      nnn,
                      DECODE_OPS_DEFAULT_64);
}

/* Mark MRM opcode extension to define instructions whose operand size must be
 * 64-bit, when in 64-bit mode. (prefixes that change operand size are ignored
 * for the instruction when in 64-bit mode).
 */
static void SetModRMOpForceSize64(
    const uint8_t group, const uint8_t nnn) {
  UpdateDecodeOpsKind("SetModRMOpDefaultSize64",
                      g_ModRMOpDecodeOpsKind[group],
                      nnn,
                      DECODE_OPS_FORCE_64);
}

/* Associate a group to use to determine the opcode extensions to use for a
 * one byte opcode, for the given run mode. The extensions are defined by the
 * nnn field (bits 3-5) of the ModR/M byte.
 */
static void SetModedOpOpInMRM(const uint8_t byte, const uint8_t group,
                                   const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    assert(g_Op1ByteTable[byte] != &DecodeUndefinedInstInfo);
    g_Op1ByteTable[byte]->opinmrm = group;
  }
}

/* Associate a group to use to determine the Opcode extensions to use for a
 * one byte opcode, for all run modes. The extensions are defined by the
 * nnn field (bits 3-5) of the ModR/M byte.
 */
static void SetOpOpInMRM(const uint8_t byte, const uint8_t group) {
  SetModedOpOpInMRM(byte, group, FLAGS_run_mode);
}

/* Associate a group to use to determine the opcode extensions to use for a
 * byte opcode (when the byte is prefixed with byte 0F), for the given run mode.
 * The extensions are defined by the nnn field (bits 3-5) of the ModR/M byte.
 */
static void SetModedOp0FOpInMRM(const uint8_t byte, const uint8_t group,
                                const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    assert(g_Op0FXXMetaTable[byte] != &DecodeUndefinedInstInfo);
    g_Op0FXXMetaTable[byte]->opinmrm = group;
  }
}

/* Associate a group to use to determine the opcode extensions to use for a
 * byte opcode (when the byte is prefixed with byte 0F), for all run modes.
 * The extensions are defined by the nnn field (bits 3-5) of the ModR/M byte.
 */
static void SetOp0FOpInMRM(const uint8_t byte, const uint8_t group) {
  SetModedOp0FOpInMRM(byte, group, FLAGS_run_mode);
}

/* Associate a group to use to determine the opcode extensions to use for
 * a byte opcode (when the byte is prefixed by byte 66), for the given run mode.
 * The extensions are defined by the nnn field (bits 3-5) of the ModR/M byte.
 */
static void SetModedOp66OpInMRM(const uint8_t byte, const uint8_t group,
                                const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    assert(g_Op660FXXTable[byte] != &DecodeUndefinedInstInfo);
    g_Op660FXXTable[byte]->opinmrm = group;
  }
}

/* Associate a group to use to determine the opcode extensions to use for
 * a byte opcode (when the byte is prefixed by byte 66), for all run modes.
 * The extensions are defined by the nnn field (bits 3-5) of the ModR/M byte.
 */
static void SetOp66OpInMRM(const uint8_t byte, const uint8_t group) {
  SetModedOp66OpInMRM(byte, group, FLAGS_run_mode);
}

/* Define ALU operand variants (6) starting at the given byte base. */
static void ALUOperandVariants00_05(const uint8_t base,
                                    const NaClInstType itype,
                                    const char* ocstr) {
  EncodeOp(base,   1, IMM_NONE, itype, aprintf("%s  $Eb, $Gb", ocstr));
  EncodeOp(base+1, 1, IMM_NONE, itype, aprintf("%s  $Ev, $Gv", ocstr));
  EncodeOp(base+2, 1, IMM_NONE, itype, aprintf("%s  $Gb, $Eb", ocstr));
  EncodeOp(base+3, 1, IMM_NONE, itype, aprintf("%s  $Gv, $Ev", ocstr));
  EncodeOp(base+4, 0, IMM_FIXED1, itype, aprintf("%s  %%al, $Ib", ocstr));
  EncodeOpRegs(base+5, 0, IMM_DATAV, itype, aprintf("%s  %%eax, $Iz", ocstr));
}

/* Define the One register variants (8) starting at the given byte base. */
static void OneRegVariants00_07(const uint8_t base,
                                const NaClInstType itype,
                                const char* ocstr,
                                bool has_mode_64) {
  int i;
  for (i = 0; i < NUM_GP_X86_32_REGS; ++i) {
    char* regname_format = aprintf("%%s  %%%s", gp_x86_32_regs[i]);
    char* format = aprintf(regname_format, ocstr);
    free(regname_format);
    EncodeModedOp(base+i, 0, IMM_NONE, itype, format, X86_32);
    if (has_mode_64) {
      char* regname_format = aprintf("%%s  %%%s", corresp_gp_x86_64_regs[i]);
      char* format = aprintf(regname_format, ocstr);
      free(regname_format);
      EncodeModedOp(base+i, 0, IMM_NONE, itype, format, X86_64);
      SetOpDefaultSize64(base+i);
    } else {
      /* Rex prefix marker */
      EncodeModedOp(base+i, 0, IMM_NONE, NACLi_ILLEGAL, "[rex]", X86_64);
      EncodeModedPrefixName(base+i, "kPrefixREX", X86_64);
    }
  }
}

/* TODO(gregoryd) - this function generates an error since it is not used */
#if 0
static char* asmprint(char** asmstr, const char* opcode,
                      const char* reg, const char* dest) {
  /* Note: add 20, just to be safe, in case the format string gets changed. */
  int asmstr_length = strlen(opcode) + strlen(reg) + strlen(dest) + 20;
  *asmstr = (char*) malloc(asmstr_length);
  if (asmstr == NULL) {
    fatal("unable to malloc in asmprint");
  }
  SNPRINTF(asm_str, "%s %s, %s", opcode, reg, dest);
  return *asmstr;
}
#endif

static void InitializeGlobalTables(void) {
  int i, j;
  /* pre-initialize g_Op1ByteTable */
  for (i = 0; i < NCDTABLESIZE; i++) {
    g_Op1ByteTable[i] = &DecodeUndefinedInstInfo;
    g_PrefixTable[i] = "0";
    g_Op0FXXMetaTable[i] = &DecodeUndefinedInstInfo;
    g_Op0F38Table[i] = &DecodeUndefinedInstInfo;
    g_Op660FXXTable[i] = &DecodeUndefinedInstInfo;
    g_OpF30FXXTable[i] = &DecodeUndefinedInstInfo;
    g_Op0F0FTable[i] = &DecodeUndefinedInstInfo;
    g_OpF20FXXTable[i] = &DecodeUndefinedInstInfo;
    g_Op660F38Table[i] = &DecodeUndefinedInstInfo;
    g_OpF20F38Table[i] = &DecodeUndefinedInstInfo;
    g_Op0F3ATable[i] = &DecodeUndefinedInstInfo;
    g_Op660F3ATable[i] = &DecodeUndefinedInstInfo;
    g_Op87D8[i] = &DecodeUndefinedInstInfo;
    g_Op87D9[i] = &DecodeUndefinedInstInfo;
    g_Op87DA[i] = &DecodeUndefinedInstInfo;
    g_Op87DB[i] = &DecodeUndefinedInstInfo;
    g_Op87DC[i] = &DecodeUndefinedInstInfo;
    g_Op87DD[i] = &DecodeUndefinedInstInfo;
    g_Op87DE[i] = &DecodeUndefinedInstInfo;
    g_Op87DF[i] = &DecodeUndefinedInstInfo;
    g_OpDecodeOpsKind[i] = DECODE_OPS_DEFAULT_32;
    g_Op0FDecodeOpsKind[i] = DECODE_OPS_DEFAULT_32;
  }
  for (i = 0; i < kNaClMRMGroupsRange; i++) {
    for (j = 0; j < kModRMOpcodeGroupSize; j++) {
      g_ModRMOpTable[i][j] = &DecodeUndefinedInstInfo;
      g_ModRMOpDecodeOpsKind[i][j] = DECODE_OPS_DEFAULT_32;
    }
  }
}

/* Create default x87 operations for the given run mode. */
static void EncodeModed87Op(OpMetaInfo* g87tab[NCDTABLESIZE],
                            const char* set1[8],
                            const char* set2[64],
                            const RunMode mode) {
  if (FLAGS_run_mode == mode) {
    int j, reg;
    uint8_t i;

    for (i = 0; i < 0xc0; i++) {
      reg = modrm_reg(i);
      if (set1[reg] == NULL) {
        g87tab[i] = NewMetaDefn(1, IMM_NONE, NACLi_INVALID, "invalid");
      } else {
        g87tab[i] = NewMetaDefn(1, IMM_NONE, NACLi_X87, set1[reg]);
      }
    }
    for (j = 0xc0; j < 0x100; j++) {
      if (set2[j - 0xc0] == NULL) {
        g87tab[j] = NewMetaDefn(1, IMM_NONE, NACLi_INVALID, "invalid");
      } else if (strcmp("fsincos", set2[j - 0xc0]) == 0) {
        g87tab[j] = NewMetaDefn(1, IMM_NONE, NACLi_X87_FSINCOS,
                                set2[j - 0xc0]);
      } else {
        g87tab[j] = NewMetaDefn(1, IMM_NONE, NACLi_X87, set2[j - 0xc0]);
      }
    }
  }
}

/* Create default x87 operations for all run modes. */
static void Encode87Op(OpMetaInfo* g87tab[NCDTABLESIZE],
                       const char* set1[8],
                       const char* set2[64]) {
  EncodeModed87Op(g87tab, set1, set2, FLAGS_run_mode);
}

static void BuildSSE4Tables(void) {
  TODO(brad, "extend hcf to check three byte instructions");
  EncodeOp660F(0x38, 1, IMM_NONE, NACLi_3BYTE, "SSE4");
  EncodeOp660F(0x3A, 1, IMM_FIXED1, NACLi_3BYTE, "SSE4");
  EncodeOpF20F(0x38, 0, IMM_NONE, NACLi_3BYTE, "SSE4");

  EncodeOp660F38(0x00, 1, IMM_NONE, NACLi_SSSE3, "pshufb $V, $W");
  EncodeOp660F38(0x01, 1, IMM_NONE, NACLi_SSSE3, "phaddw $V, $W");
  EncodeOp660F38(0x02, 1, IMM_NONE, NACLi_SSSE3, "phaddd $V, $W");
  EncodeOp660F38(0x03, 1, IMM_NONE, NACLi_SSSE3, "phaddsw $V, $W");
  EncodeOp660F38(0x04, 1, IMM_NONE, NACLi_SSSE3, "pmaddubsw $V, $W");
  EncodeOp660F38(0x05, 1, IMM_NONE, NACLi_SSSE3, "phsubw $V, $W");
  EncodeOp660F38(0x06, 1, IMM_NONE, NACLi_SSSE3, "phsubd $V, $W");
  EncodeOp660F38(0x07, 1, IMM_NONE, NACLi_SSSE3, "phsubsw $V, $W");

  EncodeOp660F38(0x10, 1, IMM_NONE, NACLi_SSE41, "pblendvb $V, $W");
  EncodeOp660F38(0x14, 1, IMM_NONE, NACLi_SSE41, "blendvps $V, $W");
  EncodeOp660F38(0x15, 1, IMM_NONE, NACLi_SSE41, "blendvpd $V, $W");
  EncodeOp660F38(0x17, 1, IMM_NONE, NACLi_SSE41, "ptest $V, $W");

  EncodeOp660F38(0x20, 1, IMM_NONE, NACLi_SSE41, "pmovsxbw $V, $U/M");
  EncodeOp660F38(0x21, 1, IMM_NONE, NACLi_SSE41, "pmovsxbd $V, $U/M");
  EncodeOp660F38(0x22, 1, IMM_NONE, NACLi_SSE41, "pmovsxbq $V, $U/M");
  EncodeOp660F38(0x23, 1, IMM_NONE, NACLi_SSE41, "pmovsxwd $V, $U/M");
  EncodeOp660F38(0x24, 1, IMM_NONE, NACLi_SSE41, "pmovsxwq $V, $U/M");
  EncodeOp660F38(0x25, 1, IMM_NONE, NACLi_SSE41, "pmovsxdq $V, $U/M");

  EncodeOp660F38(0x30, 1, IMM_NONE, NACLi_SSE41, "pmovzxbw $V, $U/M");
  EncodeOp660F38(0x31, 1, IMM_NONE, NACLi_SSE41, "pmovzxbd $V, $U/M");
  EncodeOp660F38(0x32, 1, IMM_NONE, NACLi_SSE41, "pmovzxbq $V, $U/M");
  EncodeOp660F38(0x33, 1, IMM_NONE, NACLi_SSE41, "pmovzxwd $V, $U/M");
  EncodeOp660F38(0x34, 1, IMM_NONE, NACLi_SSE41, "pmovzxwq $V, $U/M");
  EncodeOp660F38(0x35, 1, IMM_NONE, NACLi_SSE41, "pmovzxdq $V, $U/M");
  EncodeOp660F38(0x37, 1, IMM_NONE, NACLi_SSE42, "pcmpgtq $V, $U/M");

  EncodeOp660F38(0x40, 1, IMM_NONE, NACLi_SSE41, "pmulld $V, $W");
  EncodeOp660F38(0x41, 1, IMM_NONE, NACLi_SSE41, "phminposuw $V, $W");

  EncodeOp660F38(0x80, 1, IMM_NONE, NACLi_INVALID, "NVEPT $G, $M");
  EncodeOp660F38(0x81, 1, IMM_NONE, NACLi_INVALID, "NVVPID $G, $M");

  EncodeOp0F38(0xf0, 1, IMM_NONE, NACLi_MOVBE, "MOVBE $G, $M");
  EncodeOp0F38(0xf1, 1, IMM_NONE, NACLi_MOVBE, "MOVBE $M, $G");
  EncodeOpF20F38(0xf0, 1, IMM_NONE, NACLi_SSE42, "CRC32 $Gd, $Eb");
  EncodeOpF20F38(0xf1, 1, IMM_NONE, NACLi_SSE42, "CRC32 $Gd, $Ev");

  EncodeOp660F38(0x08, 1, IMM_NONE, NACLi_SSSE3, "psignb $V, $W");
  EncodeOp660F38(0x09, 1, IMM_NONE, NACLi_SSSE3, "psignw $V, $W");
  EncodeOp660F38(0x0a, 1, IMM_NONE, NACLi_SSSE3, "psignd $V, $W");
  EncodeOp660F38(0x0b, 1, IMM_NONE, NACLi_SSSE3, "pmulhrsw $V, $W");

  EncodeOp660F38(0x1c, 1, IMM_NONE, NACLi_SSSE3, "pabsb $V, $W");
  EncodeOp660F38(0x1d, 1, IMM_NONE, NACLi_SSSE3, "pabsw $V, $W");
  EncodeOp660F38(0x1e, 1, IMM_NONE, NACLi_SSSE3, "pabsd $V, $W");

  EncodeOp660F38(0x28, 1, IMM_NONE, NACLi_SSE41, "pmuldq $V, $W");
  EncodeOp660F38(0x29, 1, IMM_NONE, NACLi_SSE41, "pcmpeqq $V, $W");
  EncodeOp660F38(0x2a, 1, IMM_NONE, NACLi_SSE41, "movntdqa $V, $W");
  EncodeOp660F38(0x2b, 1, IMM_NONE, NACLi_SSE41, "packusdw $V, $W");

  EncodeOp660F38(0x38, 1, IMM_NONE, NACLi_SSE41, "pminsb $V, $W");
  EncodeOp660F38(0x39, 1, IMM_NONE, NACLi_SSE41, "pminsd $V, $W");
  EncodeOp660F38(0x3a, 1, IMM_NONE, NACLi_SSE41, "pminuw $V, $W");
  EncodeOp660F38(0x3b, 1, IMM_NONE, NACLi_SSE41, "pminud $V, $W");
  EncodeOp660F38(0x3c, 1, IMM_NONE, NACLi_SSE41, "pmaxsb $V, $W");
  EncodeOp660F38(0x3d, 1, IMM_NONE, NACLi_SSE41, "pmaxsd $V, $W");
  EncodeOp660F38(0x3e, 1, IMM_NONE, NACLi_SSE41, "pmaxuw $V, $W");
  EncodeOp660F38(0x3f, 1, IMM_NONE, NACLi_SSE41, "pmaxud $V, $W");

  EncodeOp660F3A(0x14, 1, IMM_FIXED1, NACLi_SSE41, "pextrb $R/M, $V, $Ib");
  EncodeOp660F3A(0x15, 1, IMM_FIXED1, NACLi_SSE41, "pextrw $R/M, $V, $Ib");
  EncodeOp660F3A(0x16, 1, IMM_FIXED1, NACLi_SSE41, "pextrd/q $E, $V, $Ib");
  EncodeOp660F3A(0x17, 1, IMM_FIXED1, NACLi_SSE41, "extractps $E, $V, $Ib");

  EncodeOp660F3A(0x20, 1, IMM_FIXED1, NACLi_SSE41, "pinsrb $V, $R/M, $Ib");
  EncodeOp660F3A(0x21, 1, IMM_FIXED1, NACLi_SSE41, "insertps $V, $U/M, $Ib");
  EncodeOp660F3A(0x22, 1, IMM_FIXED1, NACLi_SSE41, "pinsrd/q $V, $E, $Ib");

  EncodeOp660F3A(0x40, 1, IMM_FIXED1, NACLi_SSE41, "dpps $V, $W, $Ib");
  EncodeOp660F3A(0x41, 1, IMM_FIXED1, NACLi_SSE41, "dppd $V, $W, $Ib");
  EncodeOp660F3A(0x42, 1, IMM_FIXED1, NACLi_SSE41, "mpsadbw $V, $W, $Ib");

  EncodeOp660F3A(0x60, 1, IMM_FIXED1, NACLi_SSE42, "pcmpestrm $V, $W, $Ib");
  EncodeOp660F3A(0x61, 1, IMM_FIXED1, NACLi_SSE42, "pcmpestri $V, $W, $Ib");
  EncodeOp660F3A(0x62, 1, IMM_FIXED1, NACLi_SSE42, "pcmpistrm $V, $W, $Ib");
  EncodeOp660F3A(0x63, 1, IMM_FIXED1, NACLi_SSE42, "pcmpistri $V, $W, $Ib");

  EncodeOp660F3A(0x08, 1, IMM_FIXED1, NACLi_SSE41, "roundps $V, $W, $Ib");
  EncodeOp660F3A(0x09, 1, IMM_FIXED1, NACLi_SSE41, "roundpd $V, $W, $Ib");
  EncodeOp660F3A(0x0a, 1, IMM_FIXED1, NACLi_SSE41, "roundss $V, $W, $Ib");
  EncodeOp660F3A(0x0b, 1, IMM_FIXED1, NACLi_SSE41, "roundsd $V, $W, $Ib");
  EncodeOp660F3A(0x0c, 1, IMM_FIXED1, NACLi_SSE41, "blendps $V, $W, $Ib");
  EncodeOp660F3A(0x0d, 1, IMM_FIXED1, NACLi_SSE41, "blendpd $V, $W, $Ib");
  EncodeOp660F3A(0x0e, 1, IMM_FIXED1, NACLi_SSE41, "pblendw $V, $W, $Ib");
  EncodeOp660F3A(0x0f, 1, IMM_FIXED1, NACLi_SSSE3, "palignr $V, $W, $Ib");
}

static void Buildx87Tables(void) {
  int i;
  /* since these are so repetative I'm using a little bit of a hack */
  /* to make this more concise.                                     */
  static const char* k87D8Table1[8] =
    {"fadd", "fmul", "fcom", "fcomp", "fsub", "fsubr", "fdiv", "fdivr"};
  static const char* k87D8Table2[64] = {
    "fadd", "fadd", "fadd", "fadd", "fadd", "fadd", "fadd", "fadd",
    "fmul", "fmul", "fmul", "fmul", "fmul", "fmul", "fmul", "fmul",
    "fcom", "fcom", "fcom", "fcom", "fcom", "fcom", "fcom", "fcom",
    "fcomp", "fcomp", "fcomp", "fcomp", "fcomp", "fcomp", "fcomp", "fcomp",
    "fsub", "fsub", "fsub", "fsub", "fsub", "fsub", "fsub", "fsub",
    "fsubr", "fsubr", "fsubr", "fsubr", "fsubr", "fsubr", "fsubr", "fsubr",
    "fdiv", "fdiv", "fdiv", "fdiv", "fdiv", "fdiv", "fdiv", "fdiv",
    "fdivr", "fdivr", "fdivr", "fdivr", "fdivr", "fdivr", "fdivr", "fdivr"};

  static const char* k87D9Table1[8] =
    {"fld", NULL, "fst", "fstp", "fldenv", "fldcw", "fnstenv", "fnstcw"};
  static const char* k87D9Table2[64] = {
    "fld", "fld", "fld", "fld", "fld", "fld", "fld", "fld",
    "fxch", "fxch", "fxch", "fxch", "fxch", "fxch", "fxch", "fxch",
    "fnop", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    "fchs", "fabs", NULL, NULL, "ftst", "fxam", NULL, NULL,
    "fld1", "fldl2t", "fldl2e", "fldpi", "fldlg2", "fldln2", "fldz", NULL,
    "f2xm1", "fyl2x", "fptan", "fpatan",
    "fxtract", "fprem1", "fdecstp", "fincstp",
    "fprem", "fyl2xp1", "fsqrt", "fsincos",
    "frndint", "fscale", "fsin", "fcos" };

  static const char* k87DATable1[8] =
    {"fiadd", "fimul", "ficom", "ficomp", "fisub", "fisubr", "fidiv", "fidivr"};
  static const char* k87DATable2[64] = {
    "fcmovb", "fcmovb", "fcmovb", "fcmovb",
    "fcmovb", "fcmovb", "fcmovb", "fcmovb",
    "fcmove", "fcmove", "fcmove", "fcmove",
    "fcmove", "fcmove", "fcmove", "fcmove",
    "fcmovbe", "fcmovbe", "fcmovbe", "fcmovbe",
    "fcmovbe", "fcmovbe", "fcmovbe", "fcmovbe",
    "fcmovu", "fcmovu", "fcmovu", "fcmovu",
    "fcmovu", "fcmovu", "fcmovu", "fcmovu",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, "fucompp", NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

  static const char* k87DBTable1[8] =
    {"fild", "fisttp", "fist", "fistp", NULL, "fld", NULL, "fstp"};
  static const char* k87DBTable2[64] = {
    "fcmovnb", "fcmovnb", "fcmovnb", "fcmovnb",
    "fcmovnb", "fcmovnb", "fcmovnb", "fcmovnb",
    "fcmovne", "fcmovne", "fcmovne", "fcmovne",
    "fcmovne", "fcmovne", "fcmovne", "fcmovne",
    "fcmovnbe", "fcmovnbe", "fcmovnbe", "fcmovnbe",
    "fcmovnbe", "fcmovnbe", "fcmovnbe", "fcmovnbe",
    "fcmovnu", "fcmovnu", "fcmovnu", "fcmovnu",
    "fcmovnu", "fcmovnu", "fcmovnu", "fcmovnu",
    NULL, NULL, "fnclex", "fninit", NULL, NULL, NULL, NULL,
    "fucomi", "fucomi", "fucomi", "fucomi",
    "fucomi", "fucomi", "fucomi", "fucomi",
    "fcomi", "fcomi", "fcomi", "fcomi",
    "fcomi", "fcomi", "fcomi", "fcomi",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

  static const char* k87DCTable1[8] =
    {"fadd", "fmul", "fcom", "fcomp", "fsub", "fsubr", "fdiv", "fdivr"};
  static const char* k87DCTable2[64] = {
    "fadd", "fadd", "fadd", "fadd", "fadd", "fadd", "fadd", "fadd",
    "fmul", "fmul", "fmul", "fmul", "fmul", "fmul", "fmul", "fmul",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    "fsubr", "fsubr", "fsubr", "fsubr", "fsubr", "fsubr", "fsubr", "fsubr",
    "fsub", "fsub", "fsub", "fsub", "fsub", "fsub", "fsub", "fsub",
    "fdivr", "fdivr", "fdivr", "fdivr", "fdivr", "fdivr", "fdivr", "fdivr",
    "fdiv", "fdiv", "fdiv", "fdiv", "fdiv", "fdiv", "fdiv", "fdiv" };

  static const char* k87DDTable1[8] =
    {"fld", "fisttp", "fst", "fstp", "frstor", NULL, "fnsave", "fnstsw"};
  static const char* k87DDTable2[64] = {
    "ffree", "ffree", "ffree", "ffree", "ffree", "ffree", "ffree", "ffree",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    "fst", "fst", "fst", "fst", "fst", "fst", "fst", "fst",
    "fstp", "fstp", "fstp", "fstp", "fstp", "fstp", "fstp", "fstp",
    "fucom", "fucom", "fucom", "fucom", "fucom", "fucom", "fucom", "fucom",
    "fucomp", "fucomp", "fucomp", "fucomp",
    "fucomp", "fucomp", "fucomp", "fucomp",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

  static const char* k87DETable1[8] =
    {"fiadd", "fimul", "ficom", "ficomp", "fisub", "fisubr", "fidiv", "fidivr"};
  static const char* k87DETable2[64] = {
    "faddp", "faddp", "faddp", "faddp", "faddp", "faddp", "faddp", "faddp",
    "fmulp", "fmulp", "fmulp", "fmulp", "fmulp", "fmulp", "fmulp", "fmulp",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, "fcompp", NULL, NULL, NULL, NULL, NULL, NULL,
    "fsubrp", "fsubrp", "fsubrp", "fsubrp",
    "fsubrp", "fsubrp", "fsubrp", "fsubrp",
    "fsubp", "fsubp", "fsubp", "fsubp",
    "fsubp", "fsubp", "fsubp", "fsubp",
    "fdivrp", "fdivrp", "fdivrp", "fdivrp",
    "fdivrp", "fdivrp", "fdivrp", "fdivrp",
    "fdivp", "fdivp", "fdivp", "fdivp",
    "fdivp", "fdivp", "fdivp", "fdivp"};

  static const char* k87DFTable1[8] =
    {"fild", "fisttp", "fist", "fistp", "fbld", "fild", "fbstp", "fistp"};
  static const char* k87DFTable2[64] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    "fnstsw", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    "fucomip", "fucomip", "fucomip", "fucomip",
    "fucomip", "fucomip", "fucomip", "fucomip",
    "fcomip", "fcomip", "fcomip", "fcomip",
    "fcomip", "fcomip", "fcomip", "fcomip",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

  Encode87Op(g_Op87D8, k87D8Table1, k87D8Table2);
  Encode87Op(g_Op87D9, k87D9Table1, k87D9Table2);
  Encode87Op(g_Op87DA, k87DATable1, k87DATable2);
  Encode87Op(g_Op87DB, k87DBTable1, k87DBTable2);
  Encode87Op(g_Op87DC, k87DCTable1, k87DCTable2);
  Encode87Op(g_Op87DD, k87DDTable1, k87DDTable2);
  Encode87Op(g_Op87DE, k87DETable1, k87DETable2);
  Encode87Op(g_Op87DF, k87DFTable1, k87DFTable2);
  /* fix instruction type for x87 conditional moves */
  for (i = 0; i < 8; i++) {
    g_Op87DA[0xc0 + i]->insttype = NACLi_FCMOV;
    g_Op87DA[0xc8 + i]->insttype = NACLi_FCMOV;
    g_Op87DA[0xd0 + i]->insttype = NACLi_FCMOV;
    g_Op87DA[0xd8 + i]->insttype = NACLi_FCMOV;
    g_Op87DB[0xc0 + i]->insttype = NACLi_FCMOV;
    g_Op87DB[0xc8 + i]->insttype = NACLi_FCMOV;
    g_Op87DB[0xd0 + i]->insttype = NACLi_FCMOV;
    g_Op87DB[0xd8 + i]->insttype = NACLi_FCMOV;
  }
}

/* The root of all matching nop trie nodes. These
 * nodes are specific encodings that the decoder
 * will recognize as a NOP, if the corresponding
 * sequence of bytes occur.
 */
static NCNopTrieNode* nc_nop_root = NULL;

static struct OpInfo nc_nop_info = { NACLi_NOP , 0 , IMM_NONE , 0 };

/* Simple (fast hack) routine to extract a byte value from a character string.
 */
static int NCExtractByte(const char* sequence, int index) {
  char buffer[3];
  int i;
  for (i = 0; i < 2; ++i) {
    char ch = sequence[index + i];
    if ('\0' == ch) {
      fatal(aprintf(
              "Odd number of characters in opcode sequence: '%s'\n",
              sequence));
    }
    buffer[i] = ch;
  }
  buffer[2] = '\0';
  return strtoul(buffer, NULL, 16);
}

/* Define the given sequence of (hex) bytes as a nop instruction with
 * the given nop_info.
 */
static void NCDefNopLikeOp(const char* sequence, struct OpInfo* nop_info) {
  NCNopTrieNode** next = &nc_nop_root;
  NCNopTrieNode* last = NULL;
  int i = 0;
  while (sequence[i]) {
    int byte = NCExtractByte(sequence, i);
    if (*next == NULL) {
      /* Create node and advance. */
      *next = (NCNopTrieNode*) malloc(sizeof(NCNopTrieNode));
      (*next)->matching_byte = byte;
      (*next)->matching_opinfo = NULL;
      (*next)->success = NULL;
      (*next)->fail = NULL;
    }
    if (byte == (*next)->matching_byte) {
      last = *next;
      next = &((*next)->success);
      i += 2;
    } else {
      /* try next possible match */
      next = &((*next)->fail);
    }
  }
  /* Next points to matching node, add nop info. */
  last->matching_opinfo = nop_info;
}

/* Define the given sequence of (hex) bytes as a nop. */
static void NCDefNop(const char* sequence) {
  NCDefNopLikeOp(sequence, &nc_nop_info);
}

/* Define set of explicitly defined nop byte sequences. */
static void NCDefNops(void) {
  /* Note: The following could be recognized as nops, but are already
   * parsed and accepted by the validator:
   *
   * 90                         nop
   * 66 90                      nop
   * 89 f6                      mov %esi, %esi
   * 8d 74 26 00                lea %esi, 0x0[%esi]
   * 8d 76 00                   lea %esi, 0x0[%esi]
   * 8d b6 00 00 00 00          lea %esi, 0x0[%esi]
   * 8d b4 26 00 00 00 00       lea %esi, 0x0[%esi]
   * 8d bc 27 00 00 00 00       lea %edi, 0x0[%edi]
   * 8d bf 00 00 00 00          lea %edi, 0x0[%edi]
   * 0f 1f 00                   nop
   * 0f 1f 40 00                nop
   * 0f 1f 44 00 00             nop
   * 0f 1f 80 00 00 00 00       nop
   * 0f 1f 84 00 00 00 00 00    nop
   */
  NCDefNop("660f1f440000");
  NCDefNop("660f1f840000000000");
  NCDefNop("662e0f1f840000000000");
  NCDefNop("66662e0f1f840000000000");
  NCDefNop("6666662e0f1f840000000000");
  NCDefNop("666666662e0f1f840000000000");
  NCDefNop("66666666662e0f1f840000000000");
  NCDefNop("6666666666662e0f1f840000000000");
}

static void BuildMetaTables(void) {
   InitializeGlobalTables();

   NCDefNops();

   if (NULL == nc_nop_root) {
     /* Create dummy node so that it is non-empty. */
     nc_nop_root = (NCNopTrieNode*) malloc(sizeof(NCNopTrieNode));
     nc_nop_root->matching_byte = 0;
     nc_nop_root->matching_opinfo = NULL;
     nc_nop_root->success = NULL;
     nc_nop_root->fail = NULL;
   }

   /* now add the real contents */
   /* Eight opcode groups with a regular pattern... */
   ALUOperandVariants00_05(0x00, NACLi_386L, "add");
   ALUOperandVariants00_05(0x08, NACLi_386L, "or");
   ALUOperandVariants00_05(0x10, NACLi_386L, "adc");
   ALUOperandVariants00_05(0x18, NACLi_386L, "sbb");
   ALUOperandVariants00_05(0x20, NACLi_386L, "and");
   ALUOperandVariants00_05(0x28, NACLi_386L, "sub");
   ALUOperandVariants00_05(0x30, NACLi_386L, "xor");
   ALUOperandVariants00_05(0x38, NACLi_386, "cmp");


   /* Fill in gaps between 00 and 0x40 */
   EncodeModedOp(0x06, 0, IMM_NONE, NACLi_ILLEGAL, "push %es", X86_32);
   EncodeModedOp(0x16, 0, IMM_NONE, NACLi_ILLEGAL, "push %ss", X86_32);
   EncodeOp(0x26, 0, IMM_NONE, NACLi_ILLEGAL, "[seg %es]");
   EncodePrefixName(0x26, "kPrefixSEGES");
   EncodeOp(0x36, 0, IMM_NONE, NACLi_ILLEGAL, "[seg %ss]");
   EncodePrefixName(0x36, "kPrefixSEGSS");
   EncodeModedOp(0x07, 0, IMM_NONE, NACLi_ILLEGAL, "pop %es", X86_32);
   EncodeModedOp(0x17, 0, IMM_NONE, NACLi_ILLEGAL, "pop %ss", X86_32);
   EncodeModedOp(0x27, 0, IMM_NONE, NACLi_ILLEGAL, "daa", X86_32);
   EncodeModedOp(0x37, 0, IMM_NONE, NACLi_ILLEGAL, "aaa",
                      X86_32);  /* deprecated */
   EncodeModedOp(0x0e, 0, IMM_NONE, NACLi_ILLEGAL, "push %cs", X86_32);
   EncodeModedOp(0x1e, 0, IMM_NONE, NACLi_ILLEGAL, "push %ds", X86_32);
   EncodeOp(0x2e, 0, IMM_NONE, NACLi_ILLEGAL, "[seg %cs]");
   EncodePrefixName(0x2e, "kPrefixSEGCS");
   EncodeOp(0x3e, 0, IMM_NONE, NACLi_ILLEGAL, "[seg %ds]");
   EncodePrefixName(0x3e, "kPrefixSEGDS");

   /* 0x0f is an escape to two-byte instructions, below... */
   EncodeOp(0x0f, 0, IMM_NONE, NACLi_UNDEFINED, "[two-byte opcode]");
   EncodeModedOp(0x1f, 0, IMM_NONE, NACLi_ILLEGAL, "pop %ds", X86_32);
   EncodeOp(0x2f, 0, IMM_NONE, NACLi_ILLEGAL, "das");
   EncodeOp(0x3f, 0, IMM_NONE, NACLi_ILLEGAL, "aas");  /* deprecated */

   /* another easy pattern, 0x40-0x5f */
   /* inc and dec are deprecated in x86-64; NaCl discourages their use. */
   OneRegVariants00_07(0x40, NACLi_386L, "inc", FALSE);
   OneRegVariants00_07(0x48, NACLi_386L, "dec", FALSE);
   OneRegVariants00_07(0x50, NACLi_386, "push", TRUE);
   OneRegVariants00_07(0x58, NACLi_386, "pop", TRUE);

   /* 0x60-0x6f */
  /* also PUSHAD */
   EncodeModedOp(0x60, 0, IMM_NONE, NACLi_ILLEGAL, "pusha", X86_32);
   /* 0x61 - also POPAD */
   EncodeModedOp(0x61, 0, IMM_NONE, NACLi_ILLEGAL, "popa", X86_32);
   /* 0x62 - deprecated */
   EncodeModedOp(0x62, 1, IMM_NONE, NACLi_ILLEGAL, "bound $Gv, $Ma", X86_32);
   /* system */
   EncodeModedOp(0x63, 1, IMM_NONE, NACLi_SYSTEM, "arpl $Ew, $Gw", X86_32);
   EncodeModedOp(0x63, 1, IMM_NONE, NACLi_SYSTEM,"movsxd $Gv, $Ev", X86_64);
   EncodeOp(0x64, 0, IMM_NONE, NACLi_ILLEGAL, "[seg fs]");
   EncodePrefixName(0x64, "kPrefixSEGFS");
   EncodeOp(0x65, 0, IMM_NONE, NACLi_ILLEGAL, "[seg gs]");
   EncodePrefixName(0x65, "kPrefixSEGGS");
   EncodeOp(0x66, 0, IMM_NONE, NACLi_ILLEGAL, "[data16]");
   EncodePrefixName(0x66, "kPrefixDATA16");
   EncodeOp(0x67, 0, IMM_NONE, NACLi_ILLEGAL, "[addr size]");
   EncodePrefixName(0x67, "kPrefixADDR16");
   EncodeOp(0x68, 0, IMM_DATAV, NACLi_386, "push $Iz");
   SetOpDefaultSize64(0x68);
   EncodeOp(0x69, 1, IMM_DATAV, NACLi_386, "imul $Gv, $Ev, $Iz");
   EncodeOp(0x6a, 0, IMM_FIXED1, NACLi_386, "push $Ib");
   SetOpDefaultSize64(0x6a);
   EncodeOp(0x6b, 1, IMM_FIXED1, NACLi_386, "imul $Gv, $Ev, $Ib");
   EncodeOp(0x6c, 0, IMM_NONE, NACLi_ILLEGAL, "insb $Y, $D");
   EncodeOp(0x6d, 0, IMM_NONE, NACLi_ILLEGAL, "insw/d $Y, $D");
   EncodeOp(0x6e, 0, IMM_NONE, NACLi_ILLEGAL, "outsb $D, $X");
   EncodeOp(0x6f, 0, IMM_NONE, NACLi_ILLEGAL, "outsw/d $D, $X");

   /* 0x70-0x77: jumps */
   EncodeOp(0x70, 0, IMM_FIXED1, NACLi_JMP8, "jo $Jb");
   SetOpForceSize64(0x70);
   EncodeOp(0x71, 0, IMM_FIXED1, NACLi_JMP8, "jno $Jb");
   SetOpForceSize64(0x71);
   EncodeOp(0x72, 0, IMM_FIXED1, NACLi_JMP8, "jb $Jb");
   SetOpForceSize64(0x72);
   EncodeOp(0x73, 0, IMM_FIXED1, NACLi_JMP8, "jnb $Jb");
   SetOpForceSize64(0x73);
   EncodeOp(0x74, 0, IMM_FIXED1, NACLi_JMP8, "jz $Jb");
   SetOpForceSize64(0x74);
   EncodeOp(0x75, 0, IMM_FIXED1, NACLi_JMP8, "jnz $Jb");
   SetOpForceSize64(0x75);
   EncodeOp(0x76, 0, IMM_FIXED1, NACLi_JMP8, "jbe $Jb");
   SetOpForceSize64(0x76);
   EncodeOp(0x77, 0, IMM_FIXED1, NACLi_JMP8, "jnbe $Jb");
   SetOpForceSize64(0x77);
   EncodeOp(0x78, 0, IMM_FIXED1, NACLi_JMP8, "js $Jb");
   SetOpForceSize64(0x78);
   EncodeOp(0x79, 0, IMM_FIXED1, NACLi_JMP8, "jns $Jb");
   SetOpForceSize64(0x79);
   EncodeOp(0x7a, 0, IMM_FIXED1, NACLi_JMP8, "jp $Jb");
   SetOpForceSize64(0x7a);
   EncodeOp(0x7b, 0, IMM_FIXED1, NACLi_JMP8, "jnp $Jb");
   SetOpForceSize64(0x7b);
   EncodeOp(0x7c, 0, IMM_FIXED1, NACLi_JMP8, "jl $Jb");
   SetOpForceSize64(0x7c);
   EncodeOp(0x7d, 0, IMM_FIXED1, NACLi_JMP8, "jge $Jb");
   SetOpForceSize64(0x7d);
   EncodeOp(0x7e, 0, IMM_FIXED1, NACLi_JMP8, "jle $Jb");
   SetOpForceSize64(0x7e);
   EncodeOp(0x7f, 0, IMM_FIXED1, NACLi_JMP8, "jg $Jb");
   SetOpForceSize64(0x7f);

   /* 0x80-0x8f: Gpr1, test, xchg, mov, lea, mov, pop */
   EncodeOp(0x80, 1, IMM_FIXED1, NACLi_OPINMRM, "$group1 $Eb, $Ib");
   SetOpOpInMRM(0x80, GROUP1);
   EncodeOp(0x81, 1, IMM_DATAV, NACLi_OPINMRM, "$group1 $Ev, $Iz");
   SetOpOpInMRM(0x81, GROUP1);

   /* The AMD manual shows 0x82 as a synonym for 0x80,
    * however these are all illegal in 64-bit mode so we omit them here
    * too.
    */
   EncodeModedOp(0x82, 1, IMM_FIXED1, NACLi_ILLEGAL, "undef", X86_32);

   /* table disagrees with objdump on 0x83? */
   EncodeOp(0x83, 1, IMM_FIXED1, NACLi_OPINMRM, "$group1 $Ev, $Ib");
   SetOpOpInMRM(0x83, GROUP1);
   EncodeOp(0x84, 1, IMM_NONE, NACLi_386, "test $E, $G");
   EncodeOp(0x85, 1, IMM_NONE, NACLi_386, "test $E, $G");
   EncodeOp(0x86, 1, IMM_NONE, NACLi_386L, "xchg $E, $G");
   EncodeOp(0x87, 1, IMM_NONE, NACLi_386L, "xchg $E, $G");
   EncodeOp(0x88, 1, IMM_NONE, NACLi_386, "mov $Eb, $Gb");
   EncodeOp(0x89, 1, IMM_NONE, NACLi_386, "mov $Ev, $Gv");
   EncodeOp(0x8a, 1, IMM_NONE, NACLi_386, "mov $Gb, $Eb");
   EncodeOp(0x8b, 1, IMM_NONE, NACLi_386, "mov $Gv, $Ev");
   EncodeOp(0x8c, 1, IMM_NONE, NACLi_ILLEGAL, "mov $E, $S");
   EncodeOp(0x8d, 1, IMM_NONE, NACLi_386, "lea $G, $M");
   EncodeOp(0x8e, 1, IMM_NONE, NACLi_ILLEGAL, "mov $S, $E");
   EncodeOp(0x8f, 1, IMM_NONE, NACLi_OPINMRM, "$group1a $Ev");
   SetOpOpInMRM(0x8f, GROUP1A);
   SetOpDefaultSize64(0x8f);

   /* 0x90-0x9f */
   /* TODO(karl, encode 64 mode with rXX/xn based on REX.b) */
   EncodeOp(0x90, 0, IMM_NONE, NACLi_386R, "nop");
   EncodeOpRegs(0x91, 0, IMM_NONE, NACLi_386L, "xchg %eax, %ecx");
   EncodeOpRegs(0x92, 0, IMM_NONE, NACLi_386L, "xchg %eax, %edx");
   EncodeOpRegs(0x93, 0, IMM_NONE, NACLi_386L, "xchg %eax, %ebx");
   EncodeOpRegs(0x94, 0, IMM_NONE, NACLi_386L, "xchg %eax, %esp");
   EncodeOpRegs(0x95, 0, IMM_NONE, NACLi_386L, "xchg %eax, %ebp");
   EncodeOpRegs(0x96, 0, IMM_NONE, NACLi_386L, "xchg %eax, %esi");
   EncodeOpRegs(0x97, 0, IMM_NONE, NACLi_386L, "xchg %eax, %edi");
   EncodeOp(0x98, 0, IMM_NONE, NACLi_386, "cbw");  /* cwde cdqe */
   EncodeOp(0x99, 0, IMM_NONE, NACLi_386, "cwd");  /* cdq cqo */
   EncodeModedOp(0x9a, 0, IMM_FARPTR, NACLi_ILLEGAL, "lcall $A", X86_32);
   EncodeOp(0x9b, 0, IMM_NONE, NACLi_X87, "wait");
   EncodeOp(0x9c, 0, IMM_NONE, NACLi_ILLEGAL, "pushf $F");
   SetOpDefaultSize64(0x9c);
   EncodeOp(0x9d, 0, IMM_NONE, NACLi_ILLEGAL, "popf $F");
   SetOpDefaultSize64(0x9d);
   EncodeOp(0x9e, 0, IMM_NONE, NACLi_386, "sahf");
   EncodeOp(0x9f, 0, IMM_NONE, NACLi_386, "lahf");

   /* 0xa0-0xaf */
   EncodeOp(0xa0, 0, IMM_ADDRV, NACLi_386, "mov %al, $O");
   EncodeOpRegs(0xa1, 0, IMM_ADDRV, NACLi_386, "mov %eax, $O");
   EncodeOp(0xa2, 0, IMM_ADDRV, NACLi_386, "mov $O, %al");
   EncodeOpRegs(0xa3, 0, IMM_ADDRV, NACLi_386, "mov $O, %eax");
   EncodeOp(0xa4, 0, IMM_NONE, NACLi_386R, "movsb $X, $Y");
   EncodeOp(0xa5, 0, IMM_NONE, NACLi_386R, "movsw  $X, $Y");
   EncodeOp(0xa6, 0, IMM_NONE, NACLi_386RE, "cmpsb $X, $Y");
   EncodeOp(0xa7, 0, IMM_NONE, NACLi_386RE, "cmpsw $X, $Y");
   EncodeOp(0xa8, 0, IMM_FIXED1, NACLi_386, "test %al, $I");
   EncodeOpRegs(0xa9, 0, IMM_DATAV, NACLi_386, "test %eax, $I");
   EncodeOp(0xaa, 0, IMM_NONE, NACLi_386R, "stosb $Y, %al");
   EncodeOpRegs(0xab, 0, IMM_NONE, NACLi_386R, "stosw $Y, $eax");
   /* ISE reviewers suggested omitting lods and scas */
   EncodeOp(0xac, 0, IMM_NONE, NACLi_ILLEGAL, "lodsb %al, $X");
   EncodeOpRegs(0xad, 0, IMM_NONE, NACLi_ILLEGAL, "lodsw %eax, $X");
   EncodeOp(0xae, 0, IMM_NONE, NACLi_386RE, "scasb %al, $X");
   EncodeOpRegs(0xaf, 0, IMM_NONE, NACLi_386RE, "scasw %eax, $X");

   /* 0xb0-0xbf */
   TODO(karl, "add mode64 versions");
   TODO(karl,
        "form is rXX/xn or xl/rnl or xh/rnl based on REX.b when in mode64");
   EncodeModedOp(0xb0, 0, IMM_FIXED1, NACLi_386, "mov %al, $Ib", X86_32);
   EncodeModedOp(0xb1, 0, IMM_FIXED1, NACLi_386, "mov %cl, $Ib", X86_32);
   EncodeModedOp(0xb2, 0, IMM_FIXED1, NACLi_386, "mov %dl, $Ib", X86_32);
   EncodeModedOp(0xb3, 0, IMM_FIXED1, NACLi_386, "mov %bl, $Ib", X86_32);
   EncodeModedOp(0xb4, 0, IMM_FIXED1, NACLi_386, "mov %ah, $Ib", X86_32);
   EncodeModedOp(0xb5, 0, IMM_FIXED1, NACLi_386, "mov %ch, $Ib", X86_32);
   EncodeModedOp(0xb6, 0, IMM_FIXED1, NACLi_386, "mov %dh, $Ib", X86_32);
   EncodeModedOp(0xb7, 0, IMM_FIXED1, NACLi_386, "mov %bh, $Ib", X86_32);

   EncodeOp(0xb8, 0, IMM_MOV_DATAV, NACLi_386, "mov %eax, $Iv");
   EncodeOp(0xb9, 0, IMM_MOV_DATAV, NACLi_386, "mov %ecx, $Iv");
   EncodeOp(0xba, 0, IMM_MOV_DATAV, NACLi_386, "mov %edx, $Iv");
   EncodeOp(0xbb, 0, IMM_MOV_DATAV, NACLi_386, "mov %ebx, $Iv");
   EncodeOp(0xbc, 0, IMM_MOV_DATAV, NACLi_386, "mov %esp, $Iv");
   EncodeOp(0xbd, 0, IMM_MOV_DATAV, NACLi_386, "mov %ebp, $Iv");
   EncodeOp(0xbe, 0, IMM_MOV_DATAV, NACLi_386, "mov %esi, $Iv");
   EncodeOp(0xbf, 0, IMM_MOV_DATAV, NACLi_386, "mov %edi, $Iv");

   /* 0xc0-0xcf */
   EncodeOp(0xc0, 1, IMM_FIXED1, NACLi_OPINMRM, "$group2 $Eb, $Ib");
   SetOpOpInMRM(0xc0, GROUP2);
   EncodeOp(0xc1, 1, IMM_FIXED1, NACLi_OPINMRM, "$group2 $Ev, $Ib");
   SetOpOpInMRM(0xc1, GROUP2);
   EncodeOp(0xc2, 0, IMM_FIXED2, NACLi_RETURN, "ret $Iw");
   SetOpForceSize64(0xc2);
   EncodeOp(0xc3, 0, IMM_NONE, NACLi_RETURN, "ret");
   SetOpForceSize64(0xc3);
   EncodeModedOp(0xc4, 1, IMM_NONE, NACLi_ILLEGAL, "les $G, $M", X86_32);
   EncodeModedOp(0xc5, 1, IMM_NONE, NACLi_ILLEGAL, "lds $G, $M", X86_32);
   EncodeOp(0xc6, 1, IMM_FIXED1, NACLi_OPINMRM, "$group11 $Eb, $Ib");
   SetOpOpInMRM(0xc6, GROUP11);
   EncodeOp(0xc7, 1, IMM_DATAV, NACLi_OPINMRM, "$group11 $Ev, $Iz");
   SetOpOpInMRM(0xc7, GROUP11);
   EncodeOp(0xc8, 0, IMM_FIXED3, NACLi_ILLEGAL, "enter $I, $I");
   EncodeOp(0xc9, 0, IMM_NONE, NACLi_386, "leave");
   SetOpDefaultSize64(0xc9);
   EncodeOp(0xca, 0, IMM_FIXED2, NACLi_RETURN, "ret (far)");
   EncodeOp(0xcb, 0, IMM_NONE, NACLi_RETURN, "ret (far)");
   EncodeOp(0xcc, 0, IMM_NONE, NACLi_ILLEGAL, "int3");
   EncodeOp(0xcd, 0, IMM_FIXED1, NACLi_ILLEGAL, "int $Iv");
   EncodeModedOp(0xce, 0, IMM_NONE, NACLi_ILLEGAL, "into", X86_32);
   EncodeOp(0xcf, 0, IMM_NONE, NACLi_SYSTEM, "iret");
   /* 0xd0-0xdf */
   EncodeOp(0xd0, 1, IMM_NONE, NACLi_OPINMRM, "$group2 $Eb, 1");
   SetOpOpInMRM(0xd0, GROUP2);
   EncodeOp(0xd1, 1, IMM_NONE, NACLi_OPINMRM, "$group2 $Ev, 1");
   SetOpOpInMRM(0xd1, GROUP2);
   EncodeOp(0xd2, 1, IMM_NONE, NACLi_OPINMRM, "$group2 $Eb, %cl");
   SetOpOpInMRM(0xd2, GROUP2);
   EncodeOp(0xd3, 1, IMM_NONE, NACLi_OPINMRM, "$group2 $Ev, %cl");
   SetOpOpInMRM(0xd3, GROUP2);

   /* ISE reviewers suggested omision of AAM, AAD */
   /* 0xd4-0xd5 - deprecated */
   EncodeModedOp(0xd4, 0, IMM_FIXED1, NACLi_ILLEGAL, "aam", X86_32);
   EncodeModedOp(0xd5, 0, IMM_FIXED1, NACLi_ILLEGAL, "aad", X86_32);
   TODO(karl,
        "Intel manual (see comments above) has a blank entry"
        "for opcode 0xd6, which states that blank entries in"
        "the tables correspond to reserved (undefined) values"
        "Should we treat this accordingly?")
   EncodeOp(0xd6, 0, IMM_NONE, NACLi_ILLEGAL, "salc");

   /* ISE reviewers suggested this omision */
   EncodeOp(0xd7, 0, IMM_NONE, NACLi_ILLEGAL, "xlat");
   EncodeOp(0xd8, 1, IMM_NONE, NACLi_X87, "x87");
   EncodeOp(0xd9, 1, IMM_NONE, NACLi_X87, "x87");
   EncodeOp(0xda, 1, IMM_NONE, NACLi_X87, "x87");
   EncodeOp(0xdb, 1, IMM_NONE, NACLi_X87, "x87");
   EncodeOp(0xdc, 1, IMM_NONE, NACLi_X87, "x87");
   EncodeOp(0xdd, 1, IMM_NONE, NACLi_X87, "x87");
   EncodeOp(0xde, 1, IMM_NONE, NACLi_X87, "x87");
   EncodeOp(0xdf, 1, IMM_NONE, NACLi_X87, "x87");

   /* 0xe0-0xef */
   /* ISE reviewers suggested making loopne, loope, loop, jcxz illegal */
   /* There are faster alternatives on modern x86 implementations. */
   EncodeOp(0xe0, 0, IMM_FIXED1, NACLi_ILLEGAL, "loopne $Jb");
   SetOpForceSize64(0xe0);
   EncodeOp(0xe1, 0, IMM_FIXED1, NACLi_ILLEGAL, "loope $Jb");
   SetOpForceSize64(0xe1);
   EncodeOp(0xe2, 0, IMM_FIXED1, NACLi_ILLEGAL, "loop $Jb");
   SetOpForceSize64(0xe2);
   EncodeOp(0xe3, 0, IMM_FIXED1, NACLi_ILLEGAL, "jcxz $Jb");
   SetOpForceSize64(0xe3);

   /* I/O instructions */
   EncodeOp(0xe4, 0, IMM_FIXED1, NACLi_ILLEGAL, "in %al, $I");
   EncodeOp(0xe5, 0, IMM_FIXED1, NACLi_ILLEGAL, "in %eax, $I");
   EncodeOp(0xe6, 0, IMM_FIXED1, NACLi_ILLEGAL, "out %al, $I");
   EncodeOp(0xe7, 0, IMM_FIXED1, NACLi_ILLEGAL, "out %eax, $I");
   EncodeOp(0xe8, 0, IMM_DATAV, NACLi_JMPZ, "call $Jz");
   SetOpForceSize64(0xe8);
   EncodeOp(0xe9, 0, IMM_DATAV, NACLi_JMPZ, "jmp $Jz");
   SetOpForceSize64(0xe9);
   EncodeModedOp(0xea, 0, IMM_FARPTR, NACLi_ILLEGAL, "ljmp $A", X86_32);
   EncodeOp(0xeb, 0, IMM_FIXED1, NACLi_JMP8, "jmp $Jb");
   SetOpForceSize64(0xeb);
   EncodeOp(0xec, 0, IMM_NONE, NACLi_ILLEGAL, "in %al, %dx");
   EncodeOp(0xed, 0, IMM_NONE, NACLi_ILLEGAL, "in %eax, %dx");
   EncodeOp(0xee, 0, IMM_NONE, NACLi_ILLEGAL, "out %dx, %al");
   EncodeOp(0xef, 0, IMM_NONE, NACLi_ILLEGAL, "out %dx, %eax");

   /* 0xf0-0xff */
   EncodeOp(0xf0, 0, IMM_NONE, NACLi_ILLEGAL, "[lock]");
   EncodePrefixName(0xf0, "kPrefixLOCK");
   EncodeOp(0xf1, 0, IMM_NONE, NACLi_ILLEGAL, "int1");
   TODO(karl,
        "Intel manual (see comments above) has a blank entry"
        "for opcode 0xf2, which states that blank entries in"
        "the tables correspond to reserved (undefined) values"
        "Should we treat this accordingly?")
   EncodeOp(0xf2, 0, IMM_NONE, NACLi_ILLEGAL, "[repne]");
   EncodePrefixName(0xf2, "kPrefixREPNE");
   EncodeOp(0xf3, 0, IMM_NONE, NACLi_ILLEGAL, "[rep]");
   EncodePrefixName(0xf3, "kPrefixREP");

   /* NaCl uses the hlt instruction for bytes that should never be */
   /* executed. hlt causes immediate termination of the module. */
   EncodeOp(0xf4, 0, IMM_NONE, NACLi_386, "hlt");
   EncodeOp(0xf5, 0, IMM_NONE, NACLi_386, "cmc");

   /* Note: /0 and /1 also have an immediate */
   EncodeOp(0xf6, 1, IMM_GROUP3_F6, NACLi_OPINMRM, "$group3 $Eb");
   SetOpOpInMRM(0xf6, GROUP3);
   EncodeOp(0xf7, 1, IMM_GROUP3_F7, NACLi_OPINMRM, "$group3 $Ev");
   SetOpOpInMRM(0xf7, GROUP3);
   EncodeOp(0xf8, 0, IMM_NONE, NACLi_386, "clc");
   EncodeOp(0xf9, 0, IMM_NONE, NACLi_386, "stc");
   EncodeOp(0xfa, 0, IMM_NONE, NACLi_SYSTEM, "cli");
   EncodeOp(0xfb, 0, IMM_NONE, NACLi_SYSTEM, "sti");

   /* cld and std are generated by gcc, used for mem move operations */
   EncodeOp(0xfc, 0, IMM_NONE, NACLi_386, "cld");
   EncodeOp(0xfd, 0, IMM_NONE, NACLi_386, "std");
   EncodeOp(0xfe, 1, IMM_NONE, NACLi_OPINMRM, "$group4 $Eb");
   SetOpOpInMRM(0xfe, GROUP4);

   /* Note: /3 and /5 are $Mp rather than $Ev */
   EncodeOp(0xff, 1, IMM_NONE, NACLi_OPINMRM, "$group5 $Ev");
   SetOpOpInMRM(0xff, GROUP5);

   /* Opcodes encoded in the modrm field */
   /* Anything not done explicitly is marked illegal */
   /* group1 */
   EncodeModRMOp(GROUP1, 0, NACLi_386L, "add");
   EncodeModRMOp(GROUP1, 1, NACLi_386L, "or");
   EncodeModRMOp(GROUP1, 2, NACLi_386L, "adc");
   EncodeModRMOp(GROUP1, 3, NACLi_386L, "sbb");
   EncodeModRMOp(GROUP1, 4, NACLi_386L, "and");
   EncodeModRMOp(GROUP1, 5, NACLi_386L, "sub");
   EncodeModRMOp(GROUP1, 6, NACLi_386L, "xor");
   EncodeModRMOp(GROUP1, 7, NACLi_386, "cmp");

   /* group1a */
   EncodeModRMOp(GROUP1A, 0, NACLi_386, "pop $Ev");

   /* all other group1a opcodes are illegal */
   /* group2 */
   EncodeModRMOp(GROUP2, 0, NACLi_386, "rol");
   EncodeModRMOp(GROUP2, 1, NACLi_386, "ror");
   EncodeModRMOp(GROUP2, 2, NACLi_386, "rcl");
   EncodeModRMOp(GROUP2, 3, NACLi_386, "rcr");
   EncodeModRMOp(GROUP2, 4, NACLi_386, "shl");  /* sal */
   EncodeModRMOp(GROUP2, 5, NACLi_386, "shr");  /* sar */

   /* note 2, 6 is illegal according to Intel, shl according to AMD  */
   EncodeModRMOp(GROUP2, 7, NACLi_386, "sar");

   /* group3 */
   EncodeModRMOp(GROUP3, 0, NACLi_386, "test $I");

   /* this is such a weird case ... just put a special case in ncdecode.c */
   /* note 3, 1 is handled by a special case in the decoder */
   /* SetModRMOpImmType(3, 0, IMM_FIXED1); */
   EncodeModRMOp(GROUP3, 2, NACLi_386L, "not");
   EncodeModRMOp(GROUP3, 3, NACLi_386L, "neg");
   EncodeModRMOpRegs(GROUP3, 4, NACLi_386, "mul %eax");
   EncodeModRMOpRegs(GROUP3, 5, NACLi_386, "imul %eax");
   EncodeModRMOpRegs(GROUP3, 6, NACLi_386, "div %eax");
   EncodeModRMOpRegs(GROUP3, 7, NACLi_386, "idiv %eax");

   /* group4 */
   EncodeModRMOp(GROUP4, 0, NACLi_386L, "inc");
   EncodeModRMOp(GROUP4, 1, NACLi_386L, "dec");

   /* group5 */
   EncodeModRMOp(GROUP5, 0, NACLi_386L, "inc");
   EncodeModRMOp(GROUP5, 1, NACLi_386L, "dec");
   EncodeModRMOp(GROUP5, 2, NACLi_INDIRECT, "call *");  /* call indirect */
   SetModRMOpForceSize64(GROUP5, 2);
   EncodeModRMOp(GROUP5, 3, NACLi_ILLEGAL, "lcall *");  /* far call */
   EncodeModRMOp(GROUP5, 4, NACLi_INDIRECT, "jmp *");   /* jump indirect */
   SetModRMOpForceSize64(GROUP5, 4);
   EncodeModRMOp(GROUP5, 5, NACLi_ILLEGAL, "ljmp *");   /* far jmp */
   EncodeModRMOp(GROUP5, 6, NACLi_386, "push");
   SetModRMOpDefaultSize64(GROUP5, 6);

   /* group6 */
   EncodeModRMOp(GROUP6, 0, NACLi_SYSTEM, "sldt");
   EncodeModRMOp(GROUP6, 1, NACLi_SYSTEM, "str");
   EncodeModRMOp(GROUP6, 2, NACLi_SYSTEM, "lldt");
   EncodeModRMOp(GROUP6, 3, NACLi_SYSTEM, "ltr");
   EncodeModRMOp(GROUP6, 4, NACLi_SYSTEM, "verr");
   EncodeModRMOp(GROUP6, 5, NACLi_SYSTEM, "verw");

   /* group7 */
   EncodeModRMOp(GROUP7, 0, NACLi_SYSTEM, "sgdt");
   EncodeModRMOp(GROUP7, 1, NACLi_SYSTEM, "sidt"); /* monitor mwait */
   EncodeModRMOp(GROUP7, 2, NACLi_SYSTEM, "lgdt");
   EncodeModRMOp(GROUP7, 3, NACLi_SYSTEM, "lidt");
   EncodeModRMOp(GROUP7, 4, NACLi_SYSTEM, "smsw");
   EncodeModRMOp(GROUP7, 6, NACLi_SYSTEM, "lmsw");
   EncodeModRMOp(GROUP7, 7, NACLi_SYSTEM, "invlpg"); /* swapgs rdtscp */

   /* group8 */
   /* ISE reviewers suggested omitting bt* */
   EncodeModRMOp(GROUP8, 4, NACLi_ILLEGAL, "bt");   /* deprecated */
   EncodeModRMOp(GROUP8, 5, NACLi_ILLEGAL, "bts");  /* deprecated */
   EncodeModRMOp(GROUP8, 6, NACLi_ILLEGAL, "btr");  /* deprecated */
   EncodeModRMOp(GROUP8, 7, NACLi_ILLEGAL, "btc");  /* deprecated */

   /* group9 */
   /* If the effective operand size is 16 or 32 bits, cmpxchg8b is used. */
   /* If the size is 64 bits, cmpxchg16b is used, (64-bit mode only)     */
   /* These instructions do support the LOCK prefix */
   EncodeModRMOp(GROUP9, 1, NACLi_CMPXCHG8B, "cmpxchg8b");

   /* group10 - all illegal */
   /* group11 */
   EncodeModRMOp(GROUP11, 0, NACLi_386, "mov");

   /* group12 */
   EncodeModRMOp(GROUP12, 2, NACLi_MMXSSE2, "psrlw");
   EncodeModRMOp(GROUP12, 4, NACLi_MMXSSE2, "psraw");
   EncodeModRMOp(GROUP12, 6, NACLi_MMXSSE2, "psllw");

   /* group13 */
   EncodeModRMOp(GROUP13, 2, NACLi_MMXSSE2, "psrld");
   EncodeModRMOp(GROUP13, 4, NACLi_MMXSSE2, "psrad");
   EncodeModRMOp(GROUP13, 6, NACLi_MMXSSE2, "pslld");

   /* group14 */
   EncodeModRMOp(GROUP14, 2, NACLi_MMXSSE2, "psrlq");
   EncodeModRMOp(GROUP14, 3, NACLi_SSE2x, "psrldq");
   EncodeModRMOp(GROUP14, 6, NACLi_MMXSSE2, "psllq");
   EncodeModRMOp(GROUP14, 7, NACLi_SSE2x, "pslldq");

   /* group15 */
   EncodeModRMOp(GROUP15, 0, NACLi_ILLEGAL, "fxsave");
   EncodeModRMOp(GROUP15, 1, NACLi_ILLEGAL, "fxrstor");
   EncodeModRMOp(GROUP15, 2, NACLi_SSE, "ldmxcsr");
   EncodeModRMOp(GROUP15, 3, NACLi_SSE, "stmxcsr");
   EncodeModRMOp(GROUP15, 4, NACLi_ILLEGAL, "invalid");
   EncodeModRMOp(GROUP15, 5, NACLi_SSE2, "lfence");
   EncodeModRMOp(GROUP15, 6, NACLi_SSE2, "mfence");
   EncodeModRMOp(GROUP15, 7, NACLi_SFENCE_CLFLUSH, "sfence/clflush");

   /* group16 - SSE prefetch instructions */
   EncodeModRMOp(GROUP16, 0, NACLi_SSE, "prefetch NTA");
   EncodeModRMOp(GROUP16, 1, NACLi_SSE, "prefetch T0");
   EncodeModRMOp(GROUP16, 2, NACLi_SSE, "prefetch T1");
   EncodeModRMOp(GROUP16, 3, NACLi_SSE, "prefetch T1");
   EncodeModRMOp(GROUP16, 4, NACLi_ILLEGAL, "NOP (prefetch)");
   EncodeModRMOp(GROUP16, 5, NACLi_ILLEGAL, "NOP (prefetch)");
   EncodeModRMOp(GROUP16, 6, NACLi_ILLEGAL, "NOP (prefetch)");
   EncodeModRMOp(GROUP16, 7, NACLi_ILLEGAL, "NOP (prefetch)");

   /* groupp: prefetch - requires longmode or 3DNow! */
   /* It may be the case that these can also be enabled by CPUID_ECX_PRE; */
   /* This enabling is not supported by the validator at this time. */
   EncodeModRMOp(GROUPP, 0, NACLi_3DNOW, "prefetch exclusive");
   EncodeModRMOp(GROUPP, 1, NACLi_3DNOW, "prefetch modified");
   EncodeModRMOp(GROUPP, 2, NACLi_ILLEGAL, "[prefetch reserved]");
   EncodeModRMOp(GROUPP, 3, NACLi_ILLEGAL, "prefetch modified");
   EncodeModRMOp(GROUPP, 4, NACLi_ILLEGAL, "[prefetch reserved]");
   EncodeModRMOp(GROUPP, 5, NACLi_ILLEGAL, "[prefetch reserved]");
   EncodeModRMOp(GROUPP, 6, NACLi_ILLEGAL, "[prefetch reserved]");
   EncodeModRMOp(GROUPP, 7, NACLi_ILLEGAL, "[prefetch reserved]");

   /* encode opbyte 2; first byte is 0x0f */
   /* holes are undefined instructions    */
   /* This code used to allow the data16 (0x16) prefix to be used with */
   /* any two-byte opcode, until it caused a bug. Now all allowed      */
   /* prefixes need to be added explicitly.                            */
   /* See http://code.google.com/p/nativeclient/issues/detail?id=50    */
   /* Note: /0 and /1 have $Mw/Rv instead of $Ew */
   EncodeOp0F(0x00, 1, IMM_NONE, NACLi_OPINMRM, "$group6 $Ew");
   SetOp0FOpInMRM(0x0, GROUP6);

   /* Group7 is all privileged/system instructions */
   EncodeOp0F(0x01, 1, IMM_NONE, NACLi_OPINMRM, "$group7");
   SetOp0FOpInMRM(0x01, GROUP7);
   EncodeOp0F(0x02, 1, IMM_NONE, NACLi_SYSTEM, "lar $G, $E");
   EncodeOp0F(0x03, 1, IMM_NONE, NACLi_ILLEGAL, "lsl $Gv, $Ew");

   /* Intel table states that this only applies in 64-bit mode. */
   EncodeModedOp0F(0x05, 0, IMM_NONE, NACLi_SYSCALL, "syscall", X86_64);
   EncodeOp0F(0x06, 0, IMM_NONE, NACLi_SYSTEM, "clts");

   /* Intel table states that this only applies in 64-bit mode. */
   EncodeModedOp0F(0x07, 0, IMM_NONE, NACLi_ILLEGAL, "sysret", X86_64);
   EncodeOp0F(0x08, 0, IMM_NONE, NACLi_SYSTEM, "invd");
   EncodeOp0F(0x09, 0, IMM_NONE, NACLi_SYSTEM, "wbinvd");
   /* UD2 always generates a #UD exception sl (like HLT) is always safe. */
   EncodeOp0F(0x0b, 0, IMM_NONE, NACLi_386, "ud2");

   TODO(karl, "Intel table states that 0x0d is a 'NOP Ev'");
   EncodeOp0F(0x0d, 1, IMM_NONE, NACLi_OPINMRM, "$groupP (prefetch)");
   SetOp0FOpInMRM(0x0d, GROUPP);

   TODO(karl, "Intel table states that this is undefined");
   EncodeOp0F(0x0e, 0, IMM_NONE, NACLi_3DNOW, "femms");

   /* 3DNow instruction encodings use a MODRM byte and a 1-byte  */
   /* immediate which defines the opcode.                        */
   TODO(karl, "Intel table states that this is undefined");
   EncodeOp0F(0x0f, 1, IMM_FIXED1, NACLi_3DNOW, "3DNow");

   /* 0x10-17 appear below with the other newer opcodes ... */
   EncodeOp0F(0x18, 1, IMM_NONE, NACLi_OPINMRM, "$group16");
   SetOp0FOpInMRM(0x18, GROUP16);

   /* this nop takes an MRM byte */
   EncodeOp0F(0x1f, 1, IMM_NONE, NACLi_386, "nop");
  EncodeOp0F(0x20, 1, IMM_NONE, NACLi_SYSTEM, "mov $C, $R");
  EncodeOp0F(0x21, 1, IMM_NONE, NACLi_SYSTEM, "mov $D, $R");
  EncodeOp0F(0x22, 1, IMM_NONE, NACLi_SYSTEM, "mov $R, $C");
  EncodeOp0F(0x23, 1, IMM_NONE, NACLi_SYSTEM, "mov $R, $D");

  /* These two seem to be a mistake */
  /*  EncodeOp0F(0x24, 0, IMM_NONE, NACLi_SYSTEM, "mov $T, $R"); */
  /*  EncodeOp0F(0x26, 0, IMM_NONE, NACLi_SYSTEM, "mov $R, $T"); */
  /* 0x28-2f appear below with the other newer opcodes ... */
  /* 0x30-0x38 */
  EncodeOp0F(0x30, 0, IMM_NONE, NACLi_RDMSR, "wrmsr");
  EncodeOp0F(0x31, 0, IMM_NONE, NACLi_RDTSC, "rdtsc");
  EncodeOp0F(0x32, 0, IMM_NONE, NACLi_RDMSR, "rdmsr");
  EncodeOp0F(0x33, 0, IMM_NONE, NACLi_SYSTEM, "rdpmc");
  EncodeOp0F(0x34, 0, IMM_NONE, NACLi_SYSENTER, "sysenter");
  EncodeOp0F(0x35, 0, IMM_NONE, NACLi_SYSENTER, "sysexit");
  TODO(karl, "should this be added?");
  EncodeOp0F(0x37, 0, IMM_NONE, NACLi_ILLEGAL, "getsec");
  EncodeOp0F(0x38, 1, IMM_NONE, NACLi_3BYTE, "SSSE3, SSE4");
  EncodeOp0F(0x3a, 1, IMM_FIXED1, NACLi_3BYTE, "SSSE3, SSE4");

  /* 0x40-0x48 */
  EncodeOp0F(0x40, 1, IMM_NONE, NACLi_CMOV, "cmovo $Gv, $Ev");
  EncodeOp0F(0x41, 1, IMM_NONE, NACLi_CMOV, "cmovno $Gv, $Ev");
  EncodeOp0F(0x42, 1, IMM_NONE, NACLi_CMOV, "cmovb $Gv, $Ev");
  EncodeOp0F(0x43, 1, IMM_NONE, NACLi_CMOV, "cmovnb $Gv, $Ev");
  EncodeOp0F(0x44, 1, IMM_NONE, NACLi_CMOV, "cmovz $Gv, $Ev");
  EncodeOp0F(0x45, 1, IMM_NONE, NACLi_CMOV, "cmovnz $Gv, $Ev");
  EncodeOp0F(0x46, 1, IMM_NONE, NACLi_CMOV, "cmovbe $Gv, $Ev");
  EncodeOp0F(0x47, 1, IMM_NONE, NACLi_CMOV, "cmovnbe $Gv, $Ev");
  EncodeOp0F(0x48, 1, IMM_NONE, NACLi_CMOV, "cmovs $Gv, $Ev");
  EncodeOp0F(0x49, 1, IMM_NONE, NACLi_CMOV, "cmovns $Gv, $Ev");
  EncodeOp0F(0x4a, 1, IMM_NONE, NACLi_CMOV, "cmovp $Gv, $Ev");
  EncodeOp0F(0x4b, 1, IMM_NONE, NACLi_CMOV, "cmovnp $Gv, $Ev");
  EncodeOp0F(0x4c, 1, IMM_NONE, NACLi_CMOV, "cmovl $Gv, $Ev");
  EncodeOp0F(0x4d, 1, IMM_NONE, NACLi_CMOV, "cmovnl $Gv, $Ev");
  EncodeOp0F(0x4e, 1, IMM_NONE, NACLi_CMOV, "cmovle $Gv, $Ev");
  EncodeOp0F(0x4f, 1, IMM_NONE, NACLi_CMOV, "cmovnle $Gv, $Ev");

  /* repeat for 0x66 prefix */
  EncodeOp660F(0x40, 1, IMM_NONE, NACLi_CMOV, "cmovo $Gv, $Ev");
  EncodeOp660F(0x41, 1, IMM_NONE, NACLi_CMOV, "cmovno $Gv, $Ev");
  EncodeOp660F(0x42, 1, IMM_NONE, NACLi_CMOV, "cmovb $Gv, $Ev");
  EncodeOp660F(0x43, 1, IMM_NONE, NACLi_CMOV, "cmovnb $Gv, $Ev");
  EncodeOp660F(0x44, 1, IMM_NONE, NACLi_CMOV, "cmovz $Gv, $Ev");
  EncodeOp660F(0x45, 1, IMM_NONE, NACLi_CMOV, "cmovnz $Gv, $Ev");
  EncodeOp660F(0x46, 1, IMM_NONE, NACLi_CMOV, "cmovbe $Gv, $Ev");
  EncodeOp660F(0x47, 1, IMM_NONE, NACLi_CMOV, "cmovnbe $Gv, $Ev");
  EncodeOp660F(0x48, 1, IMM_NONE, NACLi_CMOV, "cmovs $Gv, $Ev");
  EncodeOp660F(0x49, 1, IMM_NONE, NACLi_CMOV, "cmovns $Gv, $Ev");
  EncodeOp660F(0x4a, 1, IMM_NONE, NACLi_CMOV, "cmovp $Gv, $Ev");
  EncodeOp660F(0x4b, 1, IMM_NONE, NACLi_CMOV, "cmovnp $Gv, $Ev");
  EncodeOp660F(0x4c, 1, IMM_NONE, NACLi_CMOV, "cmovl $Gv, $Ev");
  EncodeOp660F(0x4d, 1, IMM_NONE, NACLi_CMOV, "cmovnl $Gv, $Ev");
  EncodeOp660F(0x4e, 1, IMM_NONE, NACLi_CMOV, "cmovle $Gv, $Ev");
  EncodeOp660F(0x4f, 1, IMM_NONE, NACLi_CMOV, "cmovnle $Gv, $Ev");

  /* 0x80-0x8f */
  EncodeOp0F(0x80, 0, IMM_DATAV, NACLi_JMPZ, "jo $Jz");
  SetOp0FForceSize64(0x80);
  EncodeOp0F(0x81, 0, IMM_DATAV, NACLi_JMPZ, "jno $Jz");
  SetOp0FForceSize64(0x81);
  EncodeOp0F(0x82, 0, IMM_DATAV, NACLi_JMPZ, "jb $Jz");
  SetOp0FForceSize64(0x82);
  EncodeOp0F(0x83, 0, IMM_DATAV, NACLi_JMPZ, "jnb $Jz");
  SetOp0FForceSize64(0x83);
  EncodeOp0F(0x84, 0, IMM_DATAV, NACLi_JMPZ, "jz $Jz");
  SetOp0FForceSize64(0x84);
  EncodeOp0F(0x85, 0, IMM_DATAV, NACLi_JMPZ, "jnz $Jz");
  SetOp0FForceSize64(0x85);
  EncodeOp0F(0x86, 0, IMM_DATAV, NACLi_JMPZ, "jbe $Jz");
  SetOp0FForceSize64(0x86);
  EncodeOp0F(0x87, 0, IMM_DATAV, NACLi_JMPZ, "jnbe $Jz");
  SetOp0FForceSize64(0x87);
  EncodeOp0F(0x88, 0, IMM_DATAV, NACLi_JMPZ, "js $Jz");
  SetOp0FForceSize64(0x88);
  EncodeOp0F(0x89, 0, IMM_DATAV, NACLi_JMPZ, "jns $Jz");
  SetOp0FForceSize64(0x89);
  EncodeOp0F(0x8a, 0, IMM_DATAV, NACLi_JMPZ, "jp $Jz");
  SetOp0FForceSize64(0x8a);
  EncodeOp0F(0x8b, 0, IMM_DATAV, NACLi_JMPZ, "jnp $Jz");
  SetOp0FForceSize64(0x8b);
  EncodeOp0F(0x8c, 0, IMM_DATAV, NACLi_JMPZ, "jl $Jz");
  SetOp0FForceSize64(0x8c);
  EncodeOp0F(0x8d, 0, IMM_DATAV, NACLi_JMPZ, "jge $Jz");
  SetOp0FForceSize64(0x8d);
  EncodeOp0F(0x8e, 0, IMM_DATAV, NACLi_JMPZ, "jle $Jz");
  SetOp0FForceSize64(0x8e);
  EncodeOp0F(0x8f, 0, IMM_DATAV, NACLi_JMPZ, "jg $Jz");
  SetOp0FForceSize64(0x8f);

  /* 0x90-0x9f */
  EncodeOp0F(0x90, 1, IMM_NONE, NACLi_386, "seto $Eb");
  EncodeOp0F(0x91, 1, IMM_NONE, NACLi_386, "setno $Eb");
  EncodeOp0F(0x92, 1, IMM_NONE, NACLi_386, "setb $Eb");
  EncodeOp0F(0x93, 1, IMM_NONE, NACLi_386, "setnb $Eb");
  EncodeOp0F(0x94, 1, IMM_NONE, NACLi_386, "setz $Eb");
  EncodeOp0F(0x95, 1, IMM_NONE, NACLi_386, "setnz $Eb");
  EncodeOp0F(0x96, 1, IMM_NONE, NACLi_386, "setbe $Eb");
  EncodeOp0F(0x97, 1, IMM_NONE, NACLi_386, "setnbe $Eb");
  EncodeOp0F(0x98, 1, IMM_NONE, NACLi_386, "sets $Eb");
  EncodeOp0F(0x99, 1, IMM_NONE, NACLi_386, "setns $Eb");
  EncodeOp0F(0x9a, 1, IMM_NONE, NACLi_386, "setp $Eb");
  EncodeOp0F(0x9b, 1, IMM_NONE, NACLi_386, "setnp $Eb");
  EncodeOp0F(0x9c, 1, IMM_NONE, NACLi_386, "setl $Eb");
  EncodeOp0F(0x9d, 1, IMM_NONE, NACLi_386, "setge $Eb");
  EncodeOp0F(0x9e, 1, IMM_NONE, NACLi_386, "setle $Eb");
  EncodeOp0F(0x9f, 1, IMM_NONE, NACLi_386, "setg $Eb");

  /* 0xa0-0xaf */
  EncodeOp0F(0xa0, 0, IMM_NONE, NACLi_ILLEGAL, "push %fs");
  SetOp0FDefaultSize64(0xa0);
  EncodeOp0F(0xa1, 0, IMM_NONE, NACLi_ILLEGAL, "pop %fs");
  SetOp0FDefaultSize64(0xa1);
  EncodeOp0F(0xa2, 0, IMM_NONE, NACLi_386, "cpuid");
  EncodeOp0F(0xa3, 1, IMM_NONE, NACLi_ILLEGAL, "bt $Ev, $Gv");

  /* ISE reviewers suggested omitting shld */
  EncodeOp0F(0xa4, 1, IMM_FIXED1, NACLi_386, "shld $Ev, $Gv, $Ib");
  EncodeOp0F(0xa5, 1, IMM_NONE, NACLi_386, "shld $Ev, $Gv, %cl");
  EncodeOp0F(0xa6, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp0F(0xa7, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp0F(0xa8, 0, IMM_NONE, NACLi_ILLEGAL, "push %gs");
  SetOp0FDefaultSize64(0xa8);
  EncodeOp0F(0xa9, 0, IMM_NONE, NACLi_ILLEGAL, "pop %gs");
  SetOp0FDefaultSize64(0xa9);
  EncodeOp0F(0xaa, 0, IMM_NONE, NACLi_SYSTEM, "rsm");
  EncodeOp0F(0xab, 1, IMM_NONE, NACLi_ILLEGAL, "bts $Ev, $Gv");

  /* ISE reviewers suggested omitting shrd */
  EncodeOp0F(0xac, 1, IMM_FIXED1, NACLi_386, "shrd $Ev, $Gv, $Ib");
  EncodeOp0F(0xad, 1, IMM_NONE, NACLi_386, "shrd $Ev, $Gv, %cl");

  /* fxsave fxrstor ldmxcsr stmxcsr clflush */
  /* lfence mfence sfence */
  /* Note: size of memory operand varies with MRM. */
  EncodeOp0F(0xae, 1, IMM_NONE, NACLi_OPINMRM, "$group15 $M");
  SetOp0FOpInMRM(0xae, GROUP15);
  EncodeOp0F(0xaf, 1, IMM_NONE, NACLi_386, "imul $Gv, $Ev");
  EncodeOp660F(0xaf, 1, IMM_NONE, NACLi_386, "imul $Gv, $Ev");

  /* 0xb0-0xbf */
  /* Note NaCl doesn't allow 16-bit cmpxchg */
  EncodeOp0F(0xb0, 1, IMM_NONE, NACLi_386L, "cmpxchg $E, $G");
  EncodeOp0F(0xb1, 1, IMM_NONE, NACLi_386L, "cmpxchg $E, $G");
  EncodeOp0F(0xb2, 1, IMM_NONE, NACLi_ILLEGAL, "lss $Mp");
  EncodeOp0F(0xb3, 1, IMM_NONE, NACLi_ILLEGAL, "btr $Ev, $Gv");
  EncodeOp0F(0xb4, 1, IMM_NONE, NACLi_ILLEGAL, "lfs $Mp");
  EncodeOp0F(0xb5, 1, IMM_NONE, NACLi_ILLEGAL, "lgs $Mp");
  EncodeOp0F(0xb6, 1, IMM_NONE, NACLi_386, "movzx $Gv, $Eb");
  EncodeOp660F(0xb6, 1, IMM_NONE, NACLi_386, "movzx $Gv, $Eb");
  EncodeOp0F(0xb7, 1, IMM_NONE, NACLi_386, "movzx $Gv, $Ew");
  EncodeOp660F(0xb7, 1, IMM_NONE, NACLi_386, "movzx $Gv, $Ew");

  TODO(karl, "Define effect of bswap in 64-bit mode (which can be"
       "up to 4 registers");
  EncodeModedOp0F(0xc8, 0, IMM_NONE, NACLi_386, "bswap %eax", X86_32);
  EncodeModedOp0F(0xc9, 0, IMM_NONE, NACLi_386, "bswap %ecx", X86_32);
  EncodeModedOp0F(0xca, 0, IMM_NONE, NACLi_386, "bswap %edx", X86_32);
  EncodeModedOp0F(0xcb, 0, IMM_NONE, NACLi_386, "bswap %ebx", X86_32);
  EncodeModedOp0F(0xcc, 0, IMM_NONE, NACLi_386, "bswap %esp", X86_32);
  EncodeModedOp0F(0xcd, 0, IMM_NONE, NACLi_386, "bswap %ebp", X86_32);
  EncodeModedOp0F(0xce, 0, IMM_NONE, NACLi_386, "bswap %esi", X86_32);
  EncodeModedOp0F(0xcf, 0, IMM_NONE, NACLi_386, "bswap %edi", X86_32);

  /* Instructions from ?? 0F 10 to ?? 0F 1F */
  EncodeOp0F(0x10, 1, IMM_NONE, NACLi_SSE, "movups $Vps, $Wps");
  EncodeOpF30F(0x10, 1, IMM_NONE, NACLi_SSE, "movss $Vss, $Wss");
  EncodeOp660F(0x10, 1, IMM_NONE, NACLi_SSE2, "movupd $Vpd, $Wpd");
  EncodeOpF20F(0x10, 1, IMM_NONE, NACLi_SSE2, "movsd $Vsd, $Wsd");

  EncodeOp0F(0x11, 1, IMM_NONE, NACLi_SSE, "movups $Wps, $Vps");
  EncodeOpF30F(0x11, 1, IMM_NONE, NACLi_SSE, "movss $Wss, $Vss");
  EncodeOp660F(0x11, 1, IMM_NONE, NACLi_SSE2, "movupd $Wpd, $Vpd");
  EncodeOpF20F(0x11, 1, IMM_NONE, NACLi_SSE2, "movsd $Wsd, $Vsd");

  EncodeOp0F(0x12, 1, IMM_NONE, NACLi_SSE,
             "movlps $Vps, $Mq");   /* or movhlps */
  EncodeOpF30F(0x12, 1, IMM_NONE, NACLi_SSE3, "movsldup $Vps, $Wps");
  EncodeOp660F(0x12, 1, IMM_NONE, NACLi_SSE2, "movlpd $Vps, $Mq");
  EncodeOpF20F(0x12, 1, IMM_NONE, NACLi_SSE3, "movddup $Vpd, $Wsd");

  EncodeOp0F(0x13, 1, IMM_NONE, NACLi_SSE, "movlps $Mq, $Vps");
  EncodeOpF30F(0x13, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x13, 1, IMM_NONE, NACLi_SSE2, "movlpd $Mq, $Vsd");
  EncodeOpF20F(0x13, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x14, 1, IMM_NONE, NACLi_SSE, "unpcklps $Vps, $Wq");
  EncodeOpF30F(0x14, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x14, 1, IMM_NONE, NACLi_SSE2, "unpcklpd $Vpd, $Wq");
  EncodeOpF20F(0x14, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x15, 1, IMM_NONE, NACLi_SSE, "unpckhps $Vps, $Wq");
  EncodeOpF30F(0x15, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x15, 1, IMM_NONE, NACLi_SSE2, "unpckhpd $Vpd, $Wq");
  EncodeOpF20F(0x15, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x16, 1, IMM_NONE, NACLi_SSE,
             "movhps $Vps, $Mq");    /* or movlhps */
  EncodeOpF30F(0x16, 1, IMM_NONE, NACLi_SSE3, "movshdup $Vps, $Wps");
  EncodeOp660F(0x16, 1, IMM_NONE, NACLi_SSE2, "movhpd $Vsd, $Mq");
  EncodeOpF20F(0x16, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x17, 1, IMM_NONE, NACLi_SSE, "movhps $Mq, $Vps");
  EncodeOpF30F(0x17, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x17, 1, IMM_NONE, NACLi_SSE2, "movhpd $Mq, $Vpd");
  EncodeOpF20F(0x17, 0, IMM_NONE, NACLi_INVALID, "invalid");

  /* Instructions from ?? 0F 28 to ?? 0F 2F */
  EncodeOp0F(0x28, 1, IMM_NONE, NACLi_SSE, "movaps $Vps, $Wps");
  EncodeOpF30F(0x28, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x28, 1, IMM_NONE, NACLi_SSE2, "movapd $Vpd, $Wpd");
  EncodeOpF20F(0x28, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x29, 1, IMM_NONE, NACLi_SSE, "movaps $Wps, $Vps");
  EncodeOpF30F(0x29, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x29, 1, IMM_NONE, NACLi_SSE2, "movapd $Wpd, $Vpd");
  EncodeOpF20F(0x29, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x2a, 1, IMM_NONE, NACLi_SSE, "cvtpi2ps $Vps, $Qq");
  EncodeOpF30F(0x2a, 1, IMM_NONE, NACLi_SSE, "cvtsi2ss $Vss, $Ed");
  EncodeOp660F(0x2a, 1, IMM_NONE, NACLi_SSE2, "cvtpi2pd $Vpd $Qq");
  EncodeOpF20F(0x2a, 1, IMM_NONE, NACLi_SSE2, "cvtsi2sd $Vsd, $Ed");

  EncodeOp0F(0x2b, 1, IMM_NONE, NACLi_SSE, "movntps $Mdq, $Vps");
  EncodeOpF30F(0x2b, 1, IMM_NONE, NACLi_SSE4A, "movntss $Md, $Vss");
  EncodeOp660F(0x2b, 1, IMM_NONE, NACLi_SSE2, "movntpd $Mdq, $Vpd");
  EncodeOpF20F(0x2b, 1, IMM_NONE, NACLi_SSE4A, "movntsd $Mq, $Vsd");

  EncodeOp0F(0x2c, 1, IMM_NONE, NACLi_SSE, "cvttps2pi $Pq, $Wps");
  EncodeOpF30F(0x2c, 1, IMM_NONE, NACLi_SSE, "cvttss2si $Gd, $Wss");
  EncodeOp660F(0x2c, 1, IMM_NONE, NACLi_SSE2, "cvttpd2pi $Pq, $Wpd");
  EncodeOpF20F(0x2c, 1, IMM_NONE, NACLi_SSE2, "cvttsd2si $Gd, $Wsd");

  EncodeOp0F(0x2d, 1, IMM_NONE, NACLi_SSE, "cvtps2pi $Pq, $Wps");
  EncodeOpF30F(0x2d, 1, IMM_NONE, NACLi_SSE, "cvtss2si $Gd, $Wss");
  EncodeOp660F(0x2d, 1, IMM_NONE, NACLi_SSE2, "cvtpd2pi $Pq, $Wpd");
  EncodeOpF20F(0x2d, 1, IMM_NONE, NACLi_SSE2, "cvtsd2si $Gd, $Wsd");

  EncodeOp0F(0x2e, 1, IMM_NONE, NACLi_SSE, "ucomiss $Vss, $Wss");
  EncodeOpF30F(0x2e, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x2e, 1, IMM_NONE, NACLi_SSE2, "ucomisd $Vps, $Wps");
  EncodeOpF20F(0x2e, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x2f, 1, IMM_NONE, NACLi_SSE, "comiss $Vps, $Wps");
  EncodeOpF30F(0x2f, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x2f, 1, IMM_NONE, NACLi_SSE2, "comisd $Vpd, $Wsd");
  EncodeOpF20F(0x2f, 0, IMM_NONE, NACLi_INVALID, "invalid");

  /* Others from ?? 0F 39 to ?? 0F 3F are invalid             */

  /* Instructions from ?? 0F 50 to ?? 0F 7F */
  EncodeOp0F(0x50, 1, IMM_NONE, NACLi_SSE, "movmskps $Gd, $VRps");
  EncodeOpF30F(0x50, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x50, 1, IMM_NONE, NACLi_SSE2, "movmskpd $Gd, $VRpd");
  EncodeOpF20F(0x50, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x51, 1, IMM_NONE, NACLi_SSE, "sqrtps $Vps, $Wps");
  EncodeOpF30F(0x51, 1, IMM_NONE, NACLi_SSE, "sqrtss $Vss, $Wss");
  EncodeOp660F(0x51, 1, IMM_NONE, NACLi_SSE2, "sqrtpd $Vpd, $Wpd");
  EncodeOpF20F(0x51, 1, IMM_NONE, NACLi_SSE2, "sqrtsd $Vsd, $Wsd");

  EncodeOp0F(0x52, 1, IMM_NONE, NACLi_SSE, "rsqrtps $Vps, $Wps");
  EncodeOpF30F(0x52, 1, IMM_NONE, NACLi_SSE, "rsqrtss $Vss, $Wss");
  EncodeOp660F(0x52, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF20F(0x52, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x53, 1, IMM_NONE, NACLi_SSE, "rcpps $Vps, $Wps");
  EncodeOpF30F(0x53, 1, IMM_NONE, NACLi_SSE, "rcpss $Vss, $Wss");
  EncodeOp660F(0x53, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF20F(0x53, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x54, 1, IMM_NONE, NACLi_SSE, "andps $Vps, $Wps");
  EncodeOpF30F(0x54, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x54, 1, IMM_NONE, NACLi_SSE2, "andpd $Vpd, $Wpd");
  EncodeOpF20F(0x54, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x55, 1, IMM_NONE, NACLi_SSE, "andnps $Vps, $Wps");
  EncodeOpF30F(0x55, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x55, 1, IMM_NONE, NACLi_SSE2, "andnpd $Vpd, $Wpd");
  EncodeOpF20F(0x55, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x56, 1, IMM_NONE, NACLi_SSE, "orps $Vps, $Wps");
  EncodeOpF30F(0x56, 1, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x56, 1, IMM_NONE, NACLi_SSE2, "orpd $Vpd, $Wpd");
  EncodeOpF20F(0x56, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x57, 1, IMM_NONE, NACLi_SSE, "xorps $Vps, $Wps");
  EncodeOpF30F(0x57, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x57, 1, IMM_NONE, NACLi_SSE2, "xorpd $Vpd, $Wpd");
  EncodeOpF20F(0x57, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x58, 1, IMM_NONE, NACLi_SSE, "addps $Vps, $Wps");
  EncodeOpF30F(0x58, 1, IMM_NONE, NACLi_SSE, "addss $Vss, $Wss");
  EncodeOp660F(0x58, 1, IMM_NONE, NACLi_SSE2, "addpd $Vpd, $Wpd");
  EncodeOpF20F(0x58, 1, IMM_NONE, NACLi_SSE2, "addsd $Vsd, $Wsd");

  EncodeOp0F(0x59, 1, IMM_NONE, NACLi_SSE, "mulps $Vps, $Wps");
  EncodeOpF30F(0x59, 1, IMM_NONE, NACLi_SSE, "mulss $Vss, $Wss");
  EncodeOp660F(0x59, 1, IMM_NONE, NACLi_SSE2, "mulpd $Vpd, $Wpd");
  EncodeOpF20F(0x59, 1, IMM_NONE, NACLi_SSE2, "mulsd $Vsd, $Wsd");

  EncodeOp0F(0x5a, 1, IMM_NONE, NACLi_SSE2, "cvtps2pd $Vpd, $Wps");
  EncodeOpF30F(0x5a, 1, IMM_NONE, NACLi_SSE2, "cvtss2sd $Vsd, $Wss");
  EncodeOp660F(0x5a, 1, IMM_NONE, NACLi_SSE2, "cvtpd2ps $Vps, $Wpd");
  EncodeOpF20F(0x5a, 1, IMM_NONE, NACLi_SSE2, "cvtsd2ss $Vss, $Wsd");

  EncodeOp0F(0x5b, 1, IMM_NONE, NACLi_SSE2, "cvtdq2ps $Vps, $Wdq");
  EncodeOpF30F(0x5b, 1, IMM_NONE, NACLi_SSE2, "cvttps2dq $Vdq, $Wps");
  EncodeOp660F(0x5b, 1, IMM_NONE, NACLi_SSE2, "cvtps2dq $Vdq, $Wps");
  EncodeOpF20F(0x5b, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x5c, 1, IMM_NONE, NACLi_SSE, "subps $Vps, $Wps");
  EncodeOpF30F(0x5c, 1, IMM_NONE, NACLi_SSE, "subss $Vss, $Wss");
  EncodeOp660F(0x5c, 1, IMM_NONE, NACLi_SSE2, "subpd $Vpd, $Wpd");
  EncodeOpF20F(0x5c, 1, IMM_NONE, NACLi_SSE2, "subsd $Vsd, $Wsd");

  EncodeOp0F(0x5d, 1, IMM_NONE, NACLi_SSE, "minps $Vps, $Wps");
  EncodeOpF30F(0x5d, 1, IMM_NONE, NACLi_SSE, "minss $Vss, $Wss");
  EncodeOp660F(0x5d, 1, IMM_NONE, NACLi_SSE2, "minpd $Vpd, $Wpd");
  EncodeOpF20F(0x5d, 1, IMM_NONE, NACLi_SSE2, "minsd $Vsd, $Wsd");

  EncodeOp0F(0x5e, 1, IMM_NONE, NACLi_SSE, "divps $Vps, $Wps");
  EncodeOpF30F(0x5e, 1, IMM_NONE, NACLi_SSE, "divss $Vss, $Wss");
  EncodeOp660F(0x5e, 1, IMM_NONE, NACLi_SSE2, "divpd $Vpd, $Wpd");
  EncodeOpF20F(0x5e, 1, IMM_NONE, NACLi_SSE2, "divsd $Vsd, $Wsd");

  EncodeOp0F(0x5f, 1, IMM_NONE, NACLi_SSE, "maxps $Vps, $Wps");
  EncodeOpF30F(0x5f, 1, IMM_NONE, NACLi_SSE, "maxss $Vss, $Wss");
  EncodeOp660F(0x5f, 1, IMM_NONE, NACLi_SSE2, "maxpd $Vpd, $Wpd");
  EncodeOpF20F(0x5f, 1, IMM_NONE, NACLi_SSE2, "maxsd $Vsd, $Wsd");

  EncodeOp0F(0x60, 1, IMM_NONE, NACLi_MMX, "punpcklbw $Pq, $Qd");
  EncodeOpF30F(0x60, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x60, 1, IMM_NONE, NACLi_SSE2, "punpcklbw $Vdq, $Wq");
  EncodeOpF20F(0x60, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x61, 1, IMM_NONE, NACLi_MMX, "punpcklwd $Pq, $Qd");
  EncodeOpF30F(0x61, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x61, 1, IMM_NONE, NACLi_SSE2, "punpcklwd $Vdq, $Wq");
  EncodeOpF20F(0x61, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x62, 1, IMM_NONE, NACLi_MMX, "punpckldq $Pq, $Qd");
  EncodeOpF30F(0x62, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x62, 1, IMM_NONE, NACLi_SSE2, "punpckldq $Vdq, $Wq");
  EncodeOpF20F(0x62, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x63, 1, IMM_NONE, NACLi_MMX, "packsswb $Pq, $Qq");
  EncodeOpF30F(0x63, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x63, 1, IMM_NONE, NACLi_SSE2, "packsswb $Vdq, $Wdq");
  EncodeOpF20F(0x63, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x64, 1, IMM_NONE, NACLi_MMX, "pcmpgtb $Pq, $Qq");
  EncodeOpF30F(0x64, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x64, 1, IMM_NONE, NACLi_SSE2, "pcmpgtb $Vdq, $Wdq");
  EncodeOpF20F(0x64, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x65, 1, IMM_NONE, NACLi_MMX, "pcmpgtw $Pq, $Qq");
  EncodeOpF30F(0x65, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x65, 1, IMM_NONE, NACLi_SSE2, "pcmpgtw $Vdq, $Wdq");
  EncodeOpF20F(0x65, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x66, 1, IMM_NONE, NACLi_MMX, "pcmpgtd $Pq, $Qq");
  EncodeOpF30F(0x66, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x66, 1, IMM_NONE, NACLi_SSE2, "pcmpgtd $Vdq, $Wdq");
  EncodeOpF20F(0x66, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x67, 1, IMM_NONE, NACLi_MMX, "packuswb $Pq, $Qq");
  EncodeOpF30F(0x67, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x67, 1, IMM_NONE, NACLi_SSE2, "packuswb $Vdq, $Wdq");
  EncodeOpF20F(0x67, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x68, 1, IMM_NONE, NACLi_MMX, "punpckhbw $Pq, $Qd");
  EncodeOpF30F(0x68, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x68, 1, IMM_NONE, NACLi_SSE2, "punpckhbw $Vdq, $Wq");
  EncodeOpF20F(0x68, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x69, 1, IMM_NONE, NACLi_MMX, "punpckhwd $Pq, $Qd");
  EncodeOpF30F(0x69, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x69, 1, IMM_NONE, NACLi_SSE2, "punpckhwd $Vdq, $Wq");
  EncodeOpF20F(0x69, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x6a, 1, IMM_NONE, NACLi_MMX, "punpckhdq $Pq, $Qd");
  EncodeOpF30F(0x6a, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x6a, 1, IMM_NONE, NACLi_SSE2, "punpckhdq $Vdq, $Wq");
  EncodeOpF20F(0x6a, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x6b, 1, IMM_NONE, NACLi_MMX, "packssdw $Pq, $Qq");
  EncodeOpF30F(0x6b, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x6b, 1, IMM_NONE, NACLi_SSE2, "packssdw $Vdq, $Wdq");
  EncodeOpF20F(0x6b, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x6c, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0x6c, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x6c, 1, IMM_NONE, NACLi_SSE2, "punpcklqdq $Vdq, $Wq");
  EncodeOpF20F(0x6c, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x6d, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0x6d, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x6d, 1, IMM_NONE, NACLi_SSE2, "punpckhqdq $Vdq, $Wq");
  EncodeOpF20F(0x6d, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x6e, 1, IMM_NONE, NACLi_MMX, "movd $Pq, $Ed");
  EncodeOpF30F(0x6e, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x6e, 1, IMM_NONE, NACLi_SSE2, "movd $Vdq, $Edq");
  EncodeOpF20F(0x6e, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x6f, 1, IMM_NONE, NACLi_MMX, "movq $Pq, $Qq");
  EncodeOpF30F(0x6f, 1, IMM_NONE, NACLi_SSE2, "movdqu $Vdq, $Wdq");
  EncodeOp660F(0x6f, 1, IMM_NONE, NACLi_SSE2, "movdqa $Vdq, $Wdq");
  EncodeOpF20F(0x6f, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x70, 1, IMM_FIXED1, NACLi_SSE, "pshufw $Pq, $Qq, $Ib");
  EncodeOpF30F(0x70, 1, IMM_FIXED1, NACLi_SSE2, "pshufhw $Vq, $Wq, $Ib");
  EncodeOp660F(0x70, 1, IMM_FIXED1, NACLi_SSE2, "pshufd $Vdq, $Wdq, $Ib");
  EncodeOpF20F(0x70, 1, IMM_FIXED1, NACLi_SSE2, "pshuflw $Vq, $Wq, $Ib");

  EncodeOp0F(0x71, 1, IMM_FIXED1, NACLi_OPINMRM, "$group12 $PRq, $Ib");
  EncodeOpF30F(0x71, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x71, 1, IMM_FIXED1, NACLi_OPINMRM, "$group12 $VRdq, $Ib");
  EncodeOpF20F(0x71, 0, IMM_NONE, NACLi_INVALID, "invalid");
  SetOp0FOpInMRM(0x71, GROUP12);
  SetOp66OpInMRM(0x71, GROUP12);

  EncodeOp0F(0x72, 1, IMM_FIXED1, NACLi_OPINMRM, "$group13 $PRq, $Ib");
  EncodeOpF30F(0x72, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x72, 1, IMM_FIXED1, NACLi_OPINMRM, "$group13 $VRdq, $Ib");
  EncodeOpF20F(0x72, 0, IMM_NONE, NACLi_INVALID, "invalid");
  SetOp0FOpInMRM(0x72, GROUP13);
  SetOp66OpInMRM(0x72, GROUP13);

  EncodeOp0F(0x73, 1, IMM_FIXED1, NACLi_OPINMRM, "$group14 $PRq, $Ib");
  EncodeOpF30F(0x73, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x73, 1, IMM_FIXED1, NACLi_OPINMRM, "$group14 $VRdq, $Ib");
  EncodeOpF20F(0x73, 0, IMM_NONE, NACLi_INVALID, "invalid");
  SetOp0FOpInMRM(0x73, GROUP14);
  SetOp66OpInMRM(0x73, GROUP14);

  EncodeOp0F(0x74, 1, IMM_NONE, NACLi_MMX, "pcmpeqb $Pq, $Qq");
  EncodeOpF30F(0x74, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x74, 1, IMM_NONE, NACLi_SSE2, "pcmpeqb $Vdq, $Wdq");
  EncodeOpF20F(0x74, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x75, 1, IMM_NONE, NACLi_MMX, "pcmpeqw $Pq, $Qq");
  EncodeOpF30F(0x75, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x75, 1, IMM_NONE, NACLi_SSE2, "pcmpeqw $Vdq, $Wdq");
  EncodeOpF20F(0x75, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x76, 1, IMM_NONE, NACLi_MMX, "pcmpeqd $Pq, $Qq");
  EncodeOpF30F(0x76, 1, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x76, 1, IMM_NONE, NACLi_SSE2, "pcmpeqd $Vdq, $Wdq");
  EncodeOpF20F(0x76, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x77, 0, IMM_NONE, NACLi_MMX, "emms");
  EncodeOpF30F(0x77, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x77, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF20F(0x77, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x78, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0x78, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x78, 1, IMM_FIXED1, NACLi_OPINMRM, "$group17 $Vdq, $Ib, $Ib");
  SetOp66OpInMRM(0x78, GROUP17);
  EncodeOpF20F(0x78, 1, IMM_FIXED2, NACLi_SSE4A,
               "insertq $Vdq, $VRq, $Ib, $Ib");

  EncodeOp0F(0x79, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0x79, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x79, 1, IMM_NONE, NACLi_SSE4A, "extrq $Vdq, $VRq");
  EncodeOpF20F(0x79, 1, IMM_NONE, NACLi_SSE4A, "insertq $Vdq, $VRdq");

  EncodeOp0F(0x7a, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0x7a, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x7a, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF20F(0x7a, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x7b, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0x7b, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x7b, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF20F(0x7b, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x7c, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0x7c, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x7c, 1, IMM_NONE, NACLi_SSE3, "haddpd $Vpd, $Wpd");
  EncodeOpF20F(0x7c, 1, IMM_NONE, NACLi_SSE3, "haddps $Vps, $Wps");

  EncodeOp0F(0x7d, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0x7d, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0x7d, 1, IMM_NONE, NACLi_SSE3, "hsubpd $Vpd, $Wpd");
  EncodeOpF20F(0x7d, 1, IMM_NONE, NACLi_SSE3, "hsubps $Vps, $Wps");

  EncodeOp0F(0x7e, 1, IMM_NONE, NACLi_MMX, "movd $Ed, $Pd");
  EncodeOpF30F(0x7e, 1, IMM_NONE, NACLi_SSE2, "movq $Vq, $Wq");
  EncodeOp660F(0x7e, 1, IMM_NONE, NACLi_SSE2, "movd $Ed, $Vd");
  EncodeOpF20F(0x7e, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0x7f, 1, IMM_NONE, NACLi_MMX, "movq $Qq, $Pq");
  EncodeOpF30F(0x7f, 1, IMM_NONE, NACLi_SSE2, "movdqu $Wdq, $Vdq");
  EncodeOp660F(0x7f, 1, IMM_NONE, NACLi_SSE2, "movdqa $Wdq, $Vdq");
  EncodeOpF20F(0x7f, 0, IMM_NONE, NACLi_INVALID, "invalid");

  /* Instructions from ?? 0F b8 to ?? 0F bf */
  EncodeOp0F(0xb8, 0, IMM_NONE, NACLi_INVALID, "reserved");
  EncodeOpF30F(0xb8, 1, IMM_NONE, NACLi_POPCNT, "popcnt");
  EncodeOpF20F(0xb8, 0, IMM_NONE, NACLi_INVALID, "reserved");

  EncodeOp0F(0xb9, 1, IMM_NONE, NACLi_OPINMRM, "$group10");
  EncodeOpF30F(0xb9, 0, IMM_NONE, NACLi_INVALID, "reserved");
  EncodeOpF20F(0xb9, 0, IMM_NONE, NACLi_INVALID, "reserved");

  EncodeOp0F(0xba, 1, IMM_FIXED1, NACLi_OPINMRM, "$group8 $Ev, $Ib");
  EncodeOpF30F(0xba, 0, IMM_FIXED1, NACLi_INVALID, "reserved");
  EncodeOpF20F(0xba, 0, IMM_NONE, NACLi_INVALID, "reserved");
  SetOp0FOpInMRM(0xba, GROUP8);

  EncodeOp0F(0xbb, 1, IMM_NONE, NACLi_ILLEGAL, "btc $Ev, $Gv");
  EncodeOpF30F(0xbb, 0, IMM_NONE, NACLi_INVALID, "reserved");
  EncodeOpF20F(0xbb, 0, IMM_NONE, NACLi_INVALID, "reserved");

  EncodeOp0F(0xbc, 1, IMM_NONE, NACLi_386, "bsf $Gv, $Ev");
  /* tzcnt is treated as a bsf on machines that don't have tzcnt.
   * Hence, even though its conditional on NACLi_LZCNT, we act
   * like it can be used on all processors.
   */
  EncodeOpF30F(0xbc, 1, IMM_NONE, NACLi_386, "tzcnt $Gv, $Ev");
  EncodeOpF20F(0xbc, 0, IMM_NONE, NACLi_INVALID, "reserved");

  EncodeOp0F(0xbd, 1, IMM_NONE, NACLi_386, "bsr $Gv, $Ev");
  /* lzcnt is treated as a bsr on machines that don't have lzcnt.
   * Hence, even though its conditional on NACLi_LZCNT, we act
   * like it can be used on all processors.
   */
  EncodeOpF30F(0xbd, 1, IMM_NONE, NACLi_386, "lzcnt $Gv, $Ev");
  EncodeOpF20F(0xbd, 0, IMM_NONE, NACLi_INVALID, "reserved");

  EncodeOp0F(0xbe, 1, IMM_NONE, NACLi_386, "movsx $Gv, $Eb");
  EncodeOp660F(0xbe, 1, IMM_NONE, NACLi_386, "movsx $Gv, $Eb");
  EncodeOpF30F(0xbe, 1, IMM_NONE, NACLi_INVALID, "reserved");
  EncodeOpF20F(0xbe, 1, IMM_NONE, NACLi_INVALID, "reserved");

  EncodeOp0F(0xbf, 1, IMM_NONE, NACLi_386, "movsx $Gv, $Ew");
  EncodeOp660F(0xbf, 1, IMM_NONE, NACLi_386, "movsx $Gv, $Ew");
  EncodeOpF30F(0xbf, 0, IMM_NONE, NACLi_INVALID, "reserved");
  EncodeOpF20F(0xbf, 0, IMM_NONE, NACLi_INVALID, "reserved");

  /* Instructions from ?? 0F C0 to ?? 0F FF */
  /* xadd and cmpxchg appear to be the only instructions designed   */
  /* for use with multiple prefix bytes. We may need to create      */
  /* a special case for this if somebody actually needs it.         */
  EncodeOp0F(0xc0, 1, IMM_NONE, NACLi_386L, "xadd $E, $G");
  EncodeOp0F(0xc1, 1, IMM_NONE, NACLi_386L, "xadd $E, $G");

  EncodeOp0F(0xc2, 1, IMM_FIXED1, NACLi_SSE, "cmpps $V, $W, $I");
  EncodeOpF30F(0xc2, 1, IMM_FIXED1, NACLi_SSE, "cmpss $V, $W, $I");
  EncodeOp660F(0xc2, 1, IMM_FIXED1, NACLi_SSE2, "cmppd $V, $W, $I");
  EncodeOpF20F(0xc2, 1, IMM_FIXED1, NACLi_SSE2, "cmpsd $V, $W, $I");

  EncodeOp0F(0xc3, 1, IMM_NONE, NACLi_SSE2, "movnti $Md, $Gd");
  EncodeOpF30F(0xc3, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xc3, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF20F(0xc3, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xc4, 1, IMM_FIXED1, NACLi_SSE, "pinsrw $Pq, $Ew, $Ib");
  EncodeOpF30F(0xc4, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xc4, 1, IMM_FIXED1, NACLi_SSE2, "pinsrw $Vdq, $Ew, $Ib");
  EncodeOpF20F(0xc4, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xc5, 1, IMM_FIXED1, NACLi_SSE, "pextrw $Gd, $PRq, $Ib");
  EncodeOpF30F(0xc5, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xc5, 1, IMM_FIXED1, NACLi_SSE2, "pextrw $Gd, $VRdq, $Ib");
  EncodeOpF20F(0xc5, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xc6, 1, IMM_FIXED1, NACLi_SSE, "shufps $Vps, $Wps, $Ib");
  EncodeOpF30F(0xc6, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xc6, 1, IMM_FIXED1, NACLi_SSE2, "shufpd $Vpd, $Wpd, $Ib");
  EncodeOpF20F(0xc6, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xc7, 1, IMM_NONE, NACLi_OPINMRM, "$group9 $Mq");
  SetOp0FOpInMRM(0xc7, GROUP9);

  EncodeOp0F(0xd0, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0xd0, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xd0, 1, IMM_NONE, NACLi_SSE3, "addsubpd $Vpd, $Wpd");
  EncodeOpF20F(0xd0, 1, IMM_NONE, NACLi_SSE3, "addsubps $Vps, $Wps");

  EncodeOp0F(0xd1, 1, IMM_NONE, NACLi_MMX, "psrlw $Pq, $Qq");
  EncodeOpF30F(0xd1, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xd1, 1, IMM_NONE, NACLi_SSE2, "psrlw $Vdq, $Wdq");
  EncodeOpF20F(0xd1, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xd2, 1, IMM_NONE, NACLi_MMX, "psrld $Pq, $Qq");
  EncodeOpF30F(0xd2, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xd2, 1, IMM_NONE, NACLi_SSE2, "psrld $Vdq, $Wdq");
  EncodeOpF20F(0xd2, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xd3, 1, IMM_NONE, NACLi_MMX, "psrlq $Pq, $Qq");
  EncodeOpF30F(0xd3, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xd3, 1, IMM_NONE, NACLi_SSE2, "psrlq $Vdq, $Wdq");
  EncodeOpF20F(0xd3, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xd4, 1, IMM_NONE, NACLi_SSE2, "paddq $Pq, $Qq");
  EncodeOpF30F(0xd4, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xd4, 1, IMM_NONE, NACLi_SSE2, "paddq $Vdq, $Wdq");
  EncodeOpF20F(0xd4, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xd5, 1, IMM_NONE, NACLi_MMX, "pmullw $Pq, $Qq");
  EncodeOpF30F(0xd5, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xd5, 1, IMM_NONE, NACLi_SSE2, "pmullw $Vdq, $Wdq");
  EncodeOpF20F(0xd5, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xd6, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0xd6, 1, IMM_NONE, NACLi_SSE2, "movq2dq $Vdq, $PRq");
  EncodeOp660F(0xd6, 1, IMM_NONE, NACLi_SSE2, "movq $Wq, $Vq");
  EncodeOpF20F(0xd6, 1, IMM_NONE, NACLi_SSE2, "movdq2q $Pq, $VRq");

  EncodeOp0F(0xd7, 1, IMM_NONE, NACLi_SSE, "pmovmskb $Gd, $PRq");
  EncodeOpF30F(0xd7, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xd7, 1, IMM_NONE, NACLi_SSE2, "pmovmskb $Gd, $VRdq");
  EncodeOpF20F(0xd7, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xd8, 1, IMM_NONE, NACLi_MMX, "psubusb $Pq, $Qq");
  EncodeOpF30F(0xd8, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xd8, 1, IMM_NONE, NACLi_SSE2, "psubusb $Vdq, $Wdq");
  EncodeOpF20F(0xd8, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xd9, 1, IMM_NONE, NACLi_MMX, "psubusw $Pq, $Qq");
  EncodeOpF30F(0xd9, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xd9, 1, IMM_NONE, NACLi_SSE2, "psubusw $Vdq, $Wdq");
  EncodeOpF20F(0xd9, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xda, 1, IMM_NONE, NACLi_SSE, "pminub $Pq, $Qq");
  EncodeOpF30F(0xda, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xda, 1, IMM_NONE, NACLi_SSE2, "pminub $Vdq, $Wdq");
  EncodeOpF20F(0xda, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xdb, 1, IMM_NONE, NACLi_MMX, "pand $Pq, $Qq");
  EncodeOpF30F(0xdb, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xdb, 1, IMM_NONE, NACLi_SSE2, "pand $Vdq, $Wdq");
  EncodeOpF20F(0xdb, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xdc, 1, IMM_NONE, NACLi_MMX, "paddusb $Pq, $Qq");
  EncodeOpF30F(0xdc, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xdc, 1, IMM_NONE, NACLi_SSE2, "paddusb $Vdq, $Wdq");
  EncodeOpF20F(0xdc, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xdd, 1, IMM_NONE, NACLi_MMX, "paddusw $Pq, $Qq");
  EncodeOpF30F(0xdd, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xdd, 1, IMM_NONE, NACLi_SSE2, "paddusw $Vdq, $Wdq");
  EncodeOpF20F(0xdd, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xde, 1, IMM_NONE, NACLi_SSE, "pmaxub $Pq, $Qq");
  EncodeOpF30F(0xde, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xde, 1, IMM_NONE, NACLi_SSE2, "pmaxub $Vdq, $Wdq");
  EncodeOpF20F(0xde, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xdf, 1, IMM_NONE, NACLi_MMX, "pandn $Pq, $Qq");
  EncodeOpF30F(0xdf, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xdf, 1, IMM_NONE, NACLi_SSE2, "pandn $Vdq, $Wdq");
  EncodeOpF20F(0xdf, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xe0, 1, IMM_NONE, NACLi_SSE, "pavgb $Pq, $Qq");
  EncodeOpF30F(0xe0, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xe0, 1, IMM_NONE, NACLi_SSE2, "pavgb $Vdq, $Wdq");
  EncodeOpF20F(0xe0, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xe1, 1, IMM_NONE, NACLi_MMX, "psraw $Pq, $Qq");
  EncodeOpF30F(0xe1, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xe1, 1, IMM_NONE, NACLi_SSE2, "psraw $Vdq, $Wdq");
  EncodeOpF20F(0xe1, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xe2, 1, IMM_NONE, NACLi_MMX, "psrad $Pq, $Qq");
  EncodeOpF30F(0xe2, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xe2, 1, IMM_NONE, NACLi_SSE2, "psrad $Vdq, $Wdq");
  EncodeOpF20F(0xe2, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xe3, 1, IMM_NONE, NACLi_SSE, "pavgw $Pq, $Qq");
  EncodeOpF30F(0xe3, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xe3, 1, IMM_NONE, NACLi_SSE2, "pavgw $Vdq, $Wdq");
  EncodeOpF20F(0xe3, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xe4, 1, IMM_NONE, NACLi_SSE, "pmulhuw $Pq, $Qq");
  EncodeOpF30F(0xe4, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xe4, 1, IMM_NONE, NACLi_SSE2, "pmulhuw $Vdq, $Wdq");
  EncodeOpF20F(0xe4, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xe5, 1, IMM_NONE, NACLi_MMX, "pmulhw $Pq, $Qq");
  EncodeOpF30F(0xe5, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xe5, 1, IMM_NONE, NACLi_SSE2, "pmulhw $Vdq, $Wdq");
  EncodeOpF20F(0xe5, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xe6, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0xe6, 1, IMM_NONE, NACLi_SSE2, "cvtdq2pd $Vpd, $Wq");
  EncodeOp660F(0xe6, 1, IMM_NONE, NACLi_SSE2, "cvttpd2dq $Vq, $Wpd");
  EncodeOpF20F(0xe6, 1, IMM_NONE, NACLi_SSE2, "cvtpd2dq $Vq, $Wpd");

  EncodeOp0F(0xe7, 1, IMM_NONE, NACLi_SSE, "movntq $Mq, $Pq");
  EncodeOpF30F(0xe7, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xe7, 1, IMM_NONE, NACLi_SSE2, "movntdq $Mdq, $Vdq");
  EncodeOpF20F(0xe7, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xe8, 1, IMM_NONE, NACLi_MMX, "psubsb $Pq, $Qq");
  EncodeOpF30F(0xe8, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xe8, 1, IMM_NONE, NACLi_SSE2, "psubsb $Vdq, $Wdq");
  EncodeOpF20F(0xe8, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xe9, 1, IMM_NONE, NACLi_MMX, "psubsw $Pq, $Qq");
  EncodeOpF30F(0xe9, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xe9, 1, IMM_NONE, NACLi_SSE2, "psubsw $Vdq, $Wdq");
  EncodeOpF20F(0xe9, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xea, 1, IMM_NONE, NACLi_SSE, "pminsw $Pq, $Qq");
  EncodeOpF30F(0xea, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xea, 1, IMM_NONE, NACLi_SSE2, "pminsw $Vdq, $Wdq");
  EncodeOpF20F(0xea, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xeb, 1, IMM_NONE, NACLi_MMX, "por $Pq, $Qq");
  EncodeOpF30F(0xeb, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xeb, 1, IMM_NONE, NACLi_SSE2, "por $Vdq, $Wdq");
  EncodeOpF20F(0xeb, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xec, 1, IMM_NONE, NACLi_MMX, "paddsb $Pq, $Qq");
  EncodeOpF30F(0xec, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xec, 1, IMM_NONE, NACLi_SSE2, "paddsb $Vdq, $Wdq");
  EncodeOpF20F(0xec, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xed, 1, IMM_NONE, NACLi_MMX, "paddsw $Pq, $Qq");
  EncodeOpF30F(0xed, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xed, 1, IMM_NONE, NACLi_SSE2, "paddsw $Vdq, $Wdq");
  EncodeOpF20F(0xed, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xee, 1, IMM_NONE, NACLi_SSE, "pmaxsw $Pq, $Qq");
  EncodeOpF30F(0xee, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xee, 1, IMM_NONE, NACLi_SSE2, "pmaxsw $Vdq, $Wdq");
  EncodeOpF20F(0xee, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xef, 1, IMM_NONE, NACLi_MMX, "pxor $Pq, $Qq");
  EncodeOpF30F(0xef, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xef, 1, IMM_NONE, NACLi_SSE2, "pxor $Vdq, $Wdq");
  EncodeOpF20F(0xef, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xf0, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0xf0, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xf0, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF20F(0xf0, 1, IMM_NONE, NACLi_SSE3, "lddqu $Vpd, $Mdq");

  EncodeOp0F(0xf1, 1, IMM_NONE, NACLi_MMX, "psllw $Pq, $Qq");
  EncodeOpF30F(0xf1, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xf1, 1, IMM_NONE, NACLi_SSE2, "psllw $Vdq, $Wdq");
  EncodeOpF20F(0xf1, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xf2, 1, IMM_NONE, NACLi_MMX, "pslld $Pq, $Qq");
  EncodeOpF30F(0xf2, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xf2, 1, IMM_NONE, NACLi_SSE2, "pslld $Vdq, $Wdq");
  EncodeOpF20F(0xf2, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xf3, 1, IMM_NONE, NACLi_MMX, "psllq $Pq, $Qq");
  EncodeOpF30F(0xf3, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xf3, 1, IMM_NONE, NACLi_SSE2, "psllq $Vdq, $Wdq");
  EncodeOpF20F(0xf3, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xf4, 1, IMM_NONE, NACLi_SSE2, "pmuludq $Pq, $Qq");
  EncodeOpF30F(0xf4, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xf4, 1, IMM_NONE, NACLi_SSE2, "pmuludq $Vdq, $Wdq");
  EncodeOpF20F(0xf4, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xf5, 1, IMM_NONE, NACLi_MMX, "pmaddwd $Pq, $Qq");
  EncodeOpF30F(0xf5, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xf5, 1, IMM_NONE, NACLi_SSE2, "pmaddwd $Vdq, $Wdq");
  EncodeOpF20F(0xf5, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xf6, 1, IMM_NONE, NACLi_SSE, "psadbw $Pq, $Qq");
  EncodeOpF30F(0xf6, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xf6, 1, IMM_NONE, NACLi_SSE2, "psadbw $Vdq, $Wdq");
  EncodeOpF20F(0xf6, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xf7, 1, IMM_NONE, NACLi_SSE, "maskmovq $Pq, $PRq");
  EncodeOpF30F(0xf7, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xf7, 1, IMM_NONE, NACLi_SSE2, "maskmovdqu $Vdq, $VRdq");
  EncodeOpF20F(0xf7, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xf8, 1, IMM_NONE, NACLi_MMX, "psubb $Pq, $Qq");
  EncodeOpF30F(0xf8, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xf8, 1, IMM_NONE, NACLi_SSE2, "psubb $Vdq, $Wdq");
  EncodeOpF20F(0xf8, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xf9, 1, IMM_NONE, NACLi_MMX, "psubw $Pq, $Qq");
  EncodeOpF30F(0xf9, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xf9, 1, IMM_NONE, NACLi_SSE2, "psubw $Vdq, $Wdq");
  EncodeOpF20F(0xf9, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xfa, 1, IMM_NONE, NACLi_MMX, "psubd $Pq, $Qq");
  EncodeOpF30F(0xfa, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xfa, 1, IMM_NONE, NACLi_SSE2, "psubd $Vdq, $Wdq");
  EncodeOpF20F(0xfa, 0, IMM_NONE, NACLi_INVALID, "invalid");

  /* Here is an SSE2 instruction that doesn't require 0x66 */
  EncodeOp0F(0xfb, 1, IMM_NONE, NACLi_SSE2, "psubq $Pq, $Qq");
  EncodeOpF30F(0xfb, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xfb, 1, IMM_NONE, NACLi_SSE2, "psubq $Vdq, $Wdq");
  EncodeOpF20F(0xfb, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xfc, 1, IMM_NONE, NACLi_MMX, "paddb $Pq, $Qq");
  EncodeOpF30F(0xfc, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xfc, 1, IMM_NONE, NACLi_SSE2, "paddb $Vdq, $Wdq");
  EncodeOpF20F(0xfc, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xfd, 1, IMM_NONE, NACLi_MMX, "paddw $Pq, $Qq");
  EncodeOpF30F(0xfd, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xfd, 1, IMM_NONE, NACLi_SSE2, "paddw $Vdq, $Wdq");
  EncodeOpF20F(0xfd, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xfe, 1, IMM_NONE, NACLi_MMX, "paddd $Pq, $Qq");
  EncodeOpF30F(0xfe, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xfe, 1, IMM_NONE, NACLi_SSE2, "paddd $Vdq, $Wdq");
  EncodeOpF20F(0xfe, 0, IMM_NONE, NACLi_INVALID, "invalid");

  EncodeOp0F(0xff, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF30F(0xff, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOp660F(0xff, 0, IMM_NONE, NACLi_INVALID, "invalid");
  EncodeOpF20F(0xff, 0, IMM_NONE, NACLi_INVALID, "invalid");

  /* three byte opcodes */
  /* 3DNow! */
  TODO(karl, "find corresponding information for 64-bit");
  EncodeOp0F0F(0x90, 1, IMM_NONE, NACLi_3DNOW, "pfcmpge $P, $Q");
  EncodeOp0F0F(0x94, 1, IMM_NONE, NACLi_3DNOW, "pfmin $P, $Q");
  EncodeOp0F0F(0x96, 1, IMM_NONE, NACLi_3DNOW, "pfrcp $P, $Q");
  EncodeOp0F0F(0x97, 1, IMM_NONE, NACLi_3DNOW, "pfrsqrt $P, $Q");
  EncodeOp0F0F(0xa0, 1, IMM_NONE, NACLi_3DNOW, "pfcmpgt $P, $Q");
  EncodeOp0F0F(0xa4, 1, IMM_NONE, NACLi_3DNOW, "pfmax $P, $Q");
  EncodeOp0F0F(0xa6, 1, IMM_NONE, NACLi_3DNOW, "pfrcpit1 $P, $Q");
  EncodeOp0F0F(0xa7, 1, IMM_NONE, NACLi_3DNOW, "pfrsqit1 $P, $Q");
  EncodeOp0F0F(0xb0, 1, IMM_NONE, NACLi_3DNOW, "pfcmpeq $P, $Q");
  EncodeOp0F0F(0xb4, 1, IMM_NONE, NACLi_3DNOW, "pfmul $P, $Q");
  EncodeOp0F0F(0xb6, 1, IMM_NONE, NACLi_3DNOW, "pfrcpit2 $P, $Q");
  EncodeOp0F0F(0xb7, 1, IMM_NONE, NACLi_3DNOW, "pmulhrw $P, $Q");
  EncodeOp0F0F(0x0c, 1, IMM_NONE, NACLi_E3DNOW, "pi2fw $P, $Q");
  EncodeOp0F0F(0x0d, 1, IMM_NONE, NACLi_3DNOW, "pi2fd $P, $Q");
  EncodeOp0F0F(0x1c, 1, IMM_NONE, NACLi_E3DNOW, "pf2iw $P, $Q");
  EncodeOp0F0F(0x1d, 1, IMM_NONE, NACLi_3DNOW, "pf2id $P, $Q");
  EncodeOp0F0F(0x8a, 1, IMM_NONE, NACLi_E3DNOW, "pfnacc $P, $Q");
  EncodeOp0F0F(0x8e, 1, IMM_NONE, NACLi_E3DNOW, "pfpnacc $P, $Q");
  EncodeOp0F0F(0x9a, 1, IMM_NONE, NACLi_3DNOW, "pfsub $P, $Q");
  EncodeOp0F0F(0x9e, 1, IMM_NONE, NACLi_3DNOW, "pfadd $P, $Q");
  EncodeOp0F0F(0xaa, 1, IMM_NONE, NACLi_3DNOW, "pfsubr $P, $Q");
  EncodeOp0F0F(0xae, 1, IMM_NONE, NACLi_3DNOW, "pfacc $P, $Q");
  EncodeOp0F0F(0xbb, 1, IMM_NONE, NACLi_E3DNOW, "pswapd $P, $Q");
  EncodeOp0F0F(0xbf, 1, IMM_NONE, NACLi_3DNOW, "pavgusb $P, $Q");

  /* SSSE3 */
  EncodeOp0F38(0x00, 1, IMM_NONE, NACLi_SSSE3, "pshufb $P, $Q");
  EncodeOp0F38(0x01, 1, IMM_NONE, NACLi_SSSE3, "phaddw $P, $Q");
  EncodeOp0F38(0x02, 1, IMM_NONE, NACLi_SSSE3, "phaddd $P, $Q");
  EncodeOp0F38(0x03, 1, IMM_NONE, NACLi_SSSE3, "phaddsw $P, $Q");
  EncodeOp0F38(0x04, 1, IMM_NONE, NACLi_SSSE3, "pmaddubsw $P, $Q");
  EncodeOp0F38(0x05, 1, IMM_NONE, NACLi_SSSE3, "phsubw $P, $Q");
  EncodeOp0F38(0x06, 1, IMM_NONE, NACLi_SSSE3, "phsubd $P, $Q");
  EncodeOp0F38(0x07, 1, IMM_NONE, NACLi_SSSE3, "phsubsw $P, $Q");
  EncodeOp0F38(0x08, 1, IMM_NONE, NACLi_SSSE3, "psignb $P, $Q");
  EncodeOp0F38(0x09, 1, IMM_NONE, NACLi_SSSE3, "psignw $P, $Q");
  EncodeOp0F38(0x0a, 1, IMM_NONE, NACLi_SSSE3, "psignd $P, $Q");
  EncodeOp0F38(0x0b, 1, IMM_NONE, NACLi_SSSE3, "pmulhrsw $P, $Q");
  EncodeOp0F38(0x1c, 1, IMM_NONE, NACLi_SSSE3, "pabsb $P, $Q");
  EncodeOp0F38(0x1d, 1, IMM_NONE, NACLi_SSSE3, "pabsw $P, $Q");
  EncodeOp0F38(0x1e, 1, IMM_NONE, NACLi_SSSE3, "pabsd $P, $Q");
  EncodeOp0F3A(0x0f, 1, IMM_FIXED1, NACLi_SSSE3, "palignr $P, $Q, $Ib");

  BuildSSE4Tables();
  Buildx87Tables();
}

static void PrintHeader(FILE* f, const char* argv0, const char* fname) {

  /* Directory neutralize fname, so that it doesn't change depending
   * on who generates the file. Also, don't generate name of who
   * generated it (i.e. argv0), since it is dependent on the
   * build architecture used.
   */
  char* simplified_fname = strstr(fname, "native_client/");
  if (simplified_fname == NULL) fname = (char*) fname;

  fprintf(f, "/* %s\n", simplified_fname);
  fprintf(f, " * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.\n");
  fprintf(f, " * Compiled for %s.\n", RunModeName(FLAGS_run_mode));
  fprintf(f, " *\n");
  fprintf(f, " * You must include ncdecode.h before this file.\n");
  fprintf(f, " */\n\n");
}

/* Print out the contents of the given table for use with ncdecode.c */
static void PrintDecodeTable(FILE* f,
                             OpMetaInfo* gtbl[NCDTABLESIZE],
                             const char* gtbl_name) {
  int opc;
  fprintf(f, "static const struct OpInfo %s[NCDTABLESIZE] = {\n", gtbl_name);
  for (opc = 0; opc < NCDTABLESIZE; opc++) {
    OpMetaInfo* opinfo = gtbl[opc];
    fprintf(f, "  /* %02x */  { %s, %d, %d, %d },\t/* %s */\n", opc,
            NaClInstTypeString(opinfo->insttype),
            opinfo->mrmbyte,
            opinfo->immtype,
            opinfo->opinmrm,
            opinfo->disfmt);
  }
  fprintf(f, "};\n\n");
}

/* Print the contents of the (64-bit) decode ops kind table
 * to the specified file. Argument gtbl_size specifies how many
 * elements (out of a max of NCDTABLESIZE) should be printed.
 */
static void PrintDecodeOpsKindTableBody(FILE* f,
                                        DecodeOpsKind gtbl[NCDTABLESIZE],
                                        size_t gtbl_size) {
  size_t opc;
  assert(gtbl_size <= NCDTABLESIZE);
  fprintf(f, "  {\n");
  for (opc = 0; opc < gtbl_size; opc++) {
    fprintf(f, "    /* %02"NACL_PRIxS" */ %s,\n", opc,
            DecodeOpsKindName(gtbl[opc]));
  };
  fprintf(f, "  }");
}

/* Print out a C definition for the contents of the (64-bit) decode ops kind
 * table to the specified filed.
 */
static void PrintDecodeOpsKindTable(FILE* f,
                                    DecodeOpsKind gtbl[NCDTABLESIZE],
                                    const char* gtbl_name) {
  fprintf(f, "static const DecodeOpsKind %s[NCDTABLESIZE] =", gtbl_name);
  PrintDecodeOpsKindTableBody(f, gtbl, NCDTABLESIZE);
  fprintf(f, ";\n\n");
}

/* Print out which bytes correspond to prefix bytes. */
static void PrintPrefixTable(FILE* f) {
  int opc;
  fprintf(f, "static const uint32_t kPrefixTable[NCDTABLESIZE] = {");
  for (opc = 0; opc < NCDTABLESIZE; opc++) {
    if (0 == opc % 16) {
      fprintf(f, "\n  /* 0x%02x-0x%02x */\n  ", opc, opc + 15);
    }
    fprintf(f, "%s, ", g_PrefixTable[opc]);
  }
  fprintf(f, "\n};\n\n");
}

static int NCNopTrieSize(NCNopTrieNode* node) {
  if (NULL == node) {
    return 0;
  } else {
    return 1 + NCNopTrieSize(node->success) + NCNopTrieSize(node->fail);
  }
}

static void PrintNopTrieNode(FILE* f, NCNopTrieNode* node, int index) {
  if (NULL == node) return;
  fprintf(f, "  /* %5d */ { 0x%02x, %s, ", index,
          (int) node->matching_byte,
          ((NULL == node->matching_opinfo)
           ? "NULL"
           : "(struct OpInfo*)(&kNopInst)"));
  if (NULL == node->success) {
    fprintf(f, "NULL");
  } else {
    fprintf(f, "(NCNopTrieNode*) (kNcNopTrieNode + %d)", index + 1);
  }
  fprintf(f, ", ");
  if (NULL == node->fail) {
    fprintf(f, "NULL");
  } else {
    fprintf(f, "(NCNopTrieNode*) (kNcNopTrieNode + %d)",
            index + 1 + NCNopTrieSize(node->success));
  }
  fprintf(f, "},\n");
  PrintNopTrieNode(f, node->success, index + 1);
  PrintNopTrieNode(f, node->fail, index + 1 + NCNopTrieSize(node->success));
}

static void PrintNopTables(FILE* f) {
  if (NULL != nc_nop_root) {
    fprintf(f,
            "static const struct OpInfo kNopInst = { %s, %d, %d, %d };\n\n",
            NaClInstTypeString(nc_nop_info.insttype),
            nc_nop_info.hasmrmbyte,
            nc_nop_info.immtype,
            nc_nop_info.opinmrm);
  }
  fprintf(f, "static const NCNopTrieNode kNcNopTrieNode[] = {\n");
  PrintNopTrieNode(f, nc_nop_root, 0);
  fprintf(f, "};\n\n");
}

/* Print out the contents of the tables needed by ncdecode.c */
static void PrintDecodeTables(FILE* f) {
  int group, nnn;

  PrintNopTables(f);

  fprintf(f, "static const struct OpInfo kDecodeModRMOp[kNaClMRMGroupsRange]");
  fprintf(f, "[kModRMOpcodeGroupSize] = {\n");
  for (group = 0; group < kNaClMRMGroupsRange; group++) {
    fprintf(f, "  /* group %d */\n", group);
    fprintf(f, "  {\n");
    for (nnn = 0; nnn < kModRMOpcodeGroupSize; nnn++) {
      OpMetaInfo *opinfo = g_ModRMOpTable[group][nnn];
      fprintf(f, "    /* %d, %d */  { %s, %d, %d, %d },\t/* %s */\n",
              group, nnn,
              NaClInstTypeString(opinfo->insttype),
              opinfo->mrmbyte,
              opinfo->immtype,
              opinfo->opinmrm,
              opinfo->disfmt);
    }
    fprintf(f, "  },\n");
  }
  fprintf(f, "};\n\n");

  fprintf(f, "\n/* one byte opcode tables */\n");
  PrintDecodeTable(f, g_Op1ByteTable, "kDecode1ByteOp");

  fprintf(f, "\n/* two byte opcode tables */\n");
  PrintDecodeTable(f, g_Op0FXXMetaTable, "kDecode0FXXOp");
  PrintDecodeTable(f, g_Op660FXXTable, "kDecode660FXXOp");
  PrintDecodeTable(f, g_OpF20FXXTable, "kDecodeF20FXXOp");
  PrintDecodeTable(f, g_OpF30FXXTable, "kDecodeF30FXXOp");

  fprintf(f, "\n/* three byte opcode tables */\n");
  PrintDecodeTable(f, g_Op0F0FTable, "kDecode0F0FOp");
  PrintDecodeTable(f, g_Op0F38Table, "kDecode0F38Op");
  PrintDecodeTable(f, g_Op0F3ATable, "kDecode0F3AOp");
  PrintDecodeTable(f, g_Op660F38Table, "kDecode660F38Op");
  PrintDecodeTable(f, g_OpF20F38Table, "kDecodeF20F38Op");
  PrintDecodeTable(f, g_Op660F3ATable, "kDecode660F3AOp");

  fprintf(f, "\n/* x87 opcode tables*/\n");
  PrintDecodeTable(f, g_Op87D8, "kDecode87D8");
  PrintDecodeTable(f, g_Op87D9, "kDecode87D9");
  PrintDecodeTable(f, g_Op87DA, "kDecode87DA");
  PrintDecodeTable(f, g_Op87DB, "kDecode87DB");
  PrintDecodeTable(f, g_Op87DC, "kDecode87DC");
  PrintDecodeTable(f, g_Op87DD, "kDecode87DD");
  PrintDecodeTable(f, g_Op87DE, "kDecode87DE");
  PrintDecodeTable(f, g_Op87DF, "kDecode87DF");

  PrintPrefixTable(f);

  if (FLAGS_run_mode == X86_64) {
    /* Print out 64-bit decoding rules for instructions. */
    PrintDecodeOpsKindTable(f, g_OpDecodeOpsKind, "kOpDecodeOpsKind");
    PrintDecodeOpsKindTable(f, g_Op0FDecodeOpsKind, "kOp0FDecodeOpsKind");

    /* Now print out 64-bit decoding rules for instruction in
     * the Mod/RM byte, based on the group associated with it.
     */
    fprintf(f, "static const DecodeOpsKind kModRMOpDecodeOpsKind");
    fprintf(f, "[kNaClMRMGroupsRange][kModRMOpcodeGroupSize] = {\n");
    for (group = 0; group < kNaClMRMGroupsRange; group++) {
      fprintf(f, "  /* group %d */\n", group);
      PrintDecodeOpsKindTableBody(f, g_ModRMOpDecodeOpsKind[group],
                                  kModRMOpcodeGroupSize);
      fprintf(f,",\n");
    }
    fprintf(f, "};\n\n");
  }
}

/* Print out disassembly format for each possible byte value, for the
 * given run mode.
 */
static void PrintDisasmTable(FILE *f,
                             OpMetaInfo* gtbl[NCDTABLESIZE],
                             const char* gtbl_name) {
  int opc;
  fprintf(f, "static const char *%s[NCDTABLESIZE] = {\n", gtbl_name);
  for (opc = 0; opc < NCDTABLESIZE; ++opc) {
    fprintf(f, "  /* %02x */  \"%s\",\n", opc, gtbl[opc]->disfmt);
  }
  fprintf(f, "};\n\n");
}

/* Print out the tables to use to disassemble x64 code for the given mode. */
static void PrintDisasmTables(FILE *f) {
  int group, nnn;

  fprintf(f, "static const char *kDisasmModRMOp[]");
  fprintf(f, "[kNaClMRMGroupsRange] = {\n");
  for (group = 0; group < kNaClMRMGroupsRange; group++) {
    fprintf(f, "   {");
    for (nnn = 0; nnn < kModRMOpcodeGroupSize; nnn++) {
      fprintf(f, "  /* %d %d */  \"%s\",\n", group, nnn,
              g_ModRMOpTable[group][nnn]->disfmt);
    }
    fprintf(f, "   },\n");
  }
  fprintf(f, "};\n\n");

  fprintf(f, "\n/* one byte opcode tables */\n");
  PrintDisasmTable(f, g_Op1ByteTable, "kDisasm1ByteOp");

  fprintf(f, "\n/* two byte opcode tables */\n");
  PrintDisasmTable(f, g_Op0FXXMetaTable, "kDisasm0FXXOp");
  PrintDisasmTable(f, g_Op660FXXTable, "kDisasm660FXXOp");
  PrintDisasmTable(f, g_OpF20FXXTable, "kDisasmF20FXXOp");
  PrintDisasmTable(f, g_OpF30FXXTable, "kDisasmF30FXXOp");

  fprintf(f, "\n/* three byte opcode tables */\n");
  PrintDisasmTable(f, g_Op0F0FTable, "kDisasm0F0FOp");
  PrintDisasmTable(f, g_Op0F38Table, "kDisasm0F38Op");
  PrintDisasmTable(f, g_Op660F38Table, "kDisasm660F38Op");
  PrintDisasmTable(f, g_OpF20F38Table, "kDisasmF20F38Op");
  PrintDisasmTable(f, g_Op0F3ATable, "kDisasm0F3AOp");
  PrintDisasmTable(f, g_Op660F3ATable, "kDisasm660F3AOp");

  fprintf(f, "\n/* x87 opcode tables*/\n");
  PrintDisasmTable(f, g_Op87D8, "kDisasm87D8");
  PrintDisasmTable(f, g_Op87D9, "kDisasm87D9");
  PrintDisasmTable(f, g_Op87DA, "kDisasm87DA");
  PrintDisasmTable(f, g_Op87DB, "kDisasm87DB");
  PrintDisasmTable(f, g_Op87DC, "kDisasm87DC");
  PrintDisasmTable(f, g_Op87DD, "kDisasm87DD");
  PrintDisasmTable(f, g_Op87DE, "kDisasm87DE");
  PrintDisasmTable(f, g_Op87DF, "kDisasm87DF");
}

/* Construct the presumptions about what prefixes are allowed by
 * the x86-32 validator.
 *
 * In general, we are quite paranoid about what prefixes can
 * be used and where. For one-byte opcodes, prefixes are restricted
 * based on the NACLi_ type and the masks in
 * BadPrefixMask. For two-byte opcodes, any
 * prefix can be used, but they function more to define the
 * opcode as opposed to modify it; Hence there are separate
 * tables in ncdecodetab.h for the four allowed prefix bytes.
 */
static void InitBadPrefixMask(uint32_t* BadPrefixMask) {
  int i;
  for (i = 0; i < kNaClInstTypeRange; i++) {
    BadPrefixMask[i] = 0xffffffff;  /* all prefixes are bad */
  }
  BadPrefixMask[NACLi_386]   = ~(kPrefixDATA16 | kPrefixSEGGS);
  BadPrefixMask[NACLi_386L]  = ~(kPrefixDATA16 | kPrefixLOCK);
  BadPrefixMask[NACLi_386R]  = ~(kPrefixDATA16 | kPrefixREP);
  BadPrefixMask[NACLi_386RE] = ~(kPrefixDATA16 | kPrefixREP | kPrefixREPNE);
  /* CS and DS prefix bytes are used as branch prediction hints  */
  /* and do not affect target address computation.               */
  BadPrefixMask[NACLi_JMP8]  = ~(kPrefixSEGCS | kPrefixSEGDS);
  BadPrefixMask[NACLi_JMPZ]  = ~(kPrefixSEGCS | kPrefixSEGDS);
  BadPrefixMask[NACLi_CMPXCHG8B] = ~kPrefixLOCK;
}

/* Print out the table of presumptions about what prefixes are allowed
 * by the x86-32 validator.
 */
static void PrintBadPrefixMask(FILE *f) {
  int i;
  uint32_t BadPrefixMask[kNaClInstTypeRange];
  InitBadPrefixMask(BadPrefixMask);
  fprintf(f, "static uint32_t BadPrefixMask[kNaClInstTypeRange] = {\n");
  for (i = 0; i < kNaClInstTypeRange; ++i) {
    fprintf(f, "  0x%"NACL_PRIx32", /* %s */\n", BadPrefixMask[i],
            NaClInstTypeString(i));
  }
  fprintf(f, "};\n");
}

FILE* mustopen(const char* fname, const char* how) {
  FILE* f = fopen(fname, how);
  if (f == NULL) {
    fprintf(stderr, "could not fopen(%s, %s)\n", fname, how);
    fprintf(stderr, "exiting now\n");
    exit(-1);
  }
  return f;
}

/* Recognizes flags in argv, processes them, and then removes them.
 * Returns the updated value for argc.
 */
int GrokFlags(int argc, const char* argv[]) {
  int i;
  int new_argc;
  if (argc == 0) return 0;
  new_argc = 1;
  for (i = 1; i < argc; ++i) {
    if (0 == strcmp("-m32", argv[i])) {
      FLAGS_run_mode = X86_32;
    } else if (0 == strcmp("-m64", argv[i])) {
      FLAGS_run_mode = X86_64;
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

int main(const int argc, const char* argv[]) {
  FILE *f;
  int new_argc = GrokFlags(argc, argv);
  if ((new_argc != 4) || (FLAGS_run_mode == RunModeSize)) {
    fprintf(stderr,
            "ERROR: usage: ncdecode_table <architecture flag> "
            "<ncdecodetab.h> <ncdisasmtab.h> <ncbadprefixmask.h>\n");
    return -1;
  }

  BuildMetaTables();

  f = mustopen(argv[1], "w");
  PrintHeader(f, argv[0], argv[1]);
  PrintDecodeTables(f);
  fclose(f);

  f = mustopen(argv[2], "w");
  PrintHeader(f, argv[0], argv[2]);
  PrintDisasmTables(f);
  fclose(f);

  f = mustopen(argv[3], "w");
  PrintHeader(f, argv[0], argv[3]);
  PrintBadPrefixMask(f);
  fclose(f);

  return 0;
}
