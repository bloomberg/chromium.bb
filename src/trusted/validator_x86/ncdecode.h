/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncdecode.h - table driven decoder for Native Client.
 *
 * This header file contains type declarations and constants
 * used by the decoder input table
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_H_

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_x86/ncinstbuffer.h"

EXTERN_C_BEGIN

struct NCDecoderInst;
struct NCDecoderState;

/* Function type for a decoder action. Returns TRUE if action
 * was applied successfully.
 */
typedef Bool (*NCDecoderStateAction)(const struct NCDecoderInst* dinst);

/* Function type for other decoder state methods. */
typedef void (*NCDecoderStateMethod)(struct NCDecoderState* vstate);

/* Defines the corresponding byte encodings for each of the prefixes. */
#define kValueSEGCS  0x2e
#define kValueSEGSS  0x36
#define kValueSEGFS  0x64
#define kValueSEGGS  0x65
#define kValueDATA16 0x66
#define kValueADDR16 0x67
#define kValueREPNE  0xf2
#define kValueREP    0xf3
#define kValueLOCK   0xf0
#define kValueSEGES  0x26
#define kValueSEGDS  0x3e

/* Using a bit mask here. Hopefully nobody will be offended.
 * Prefix usage: 0x2e and 0x3e are used as branch prediction hints
 *               0x64 and 0x65 needed for TLS
 *               0x26 and 0x36 shouldn't be needed
 * These are #defines, not const ints, because they are used
 * for array initialization
 */
#define kPrefixSEGCS  0x0001  /* 0x2e */
#define kPrefixSEGSS  0x0002  /* 0x36 */
#define kPrefixSEGFS  0x0004  /* 0x64 */
#define kPrefixSEGGS  0x0008  /* 0x65 */
#define kPrefixDATA16 0x0010  /* 0x66 - OKAY */
#define kPrefixADDR16 0x0020  /* 0x67 - disallowed */
#define kPrefixREPNE  0x0040  /* 0xf2 - OKAY */
#define kPrefixREP    0x0080  /* 0xf3 - OKAY */
#define kPrefixLOCK   0x0100  /* 0xf0 - OKAY */
#define kPrefixSEGES  0x0200  /* 0x26 - disallowed */
#define kPrefixSEGDS  0x0400  /* 0x3e - disallowed */
#define kPrefixREX    0x1000  /* 0x40 - 0x4f Rex prefix */

/* Readability macros for bitset testing. */
#define NaClHasBit(set, bit) ((set) & (bit))
#define NaClExcludesBit(set, bit) (~(set) & bit)

/* a new enumerated type for instructions.
 * Note: Each enumerate type is marked with one of the following symbols,
 * defining the validator it us used for:
 *    32 - The x86-32 validator.
 *    64 - The x86-64 validator.
 *    Both - Both the x86-32 and the x86-64 validators.
 * Note: The code for the x86-64 validator is being cleaned up, and there
 * are still uses of the "32" tag for x86 instructions.
 * TODO(karl) - Fix this comment when modeling for the x86-64 has been cleaned
 * up.
 */
typedef enum {
  NACLi_UNDEFINED = 0, /* uninitialized space; should never happen */ /* Both */
  NACLi_ILLEGAL,      /* not allowed in NaCl */                       /* Both */
  NACLi_INVALID,      /* not valid on any known x86 */                /* Both */
  NACLi_SYSTEM,       /* ring-0 instruction, not allowed in NaCl */   /* Both */
  NACLi_NOP,          /* Predefined nop instruction sequence. */      /* 32 */
  NACLi_UD2,          /* Predefined ud2 instruction sequence. */      /* 32 */
  NACLi_386,          /* an allowed instruction on all i386 implementations */
                                                                      /* Both */
                      /* subset of i386 that allows LOCK prefix. NOTE:
                       * This is only used for the 32 bit validator. The new
                       * 64 bit validator uses "InstFlag(OpcodeLockable)"
                       * to communicate this (which separates the CPU ID
                       * information from whether the instruction is lockable.
                       * Hopefully, in future releases, this enumerated type
                       * will be removed.
                       */
  NACLi_386L,                                                         /* 32 */
  NACLi_386R,         /* subset of i386 that allow REP prefix */      /* 32 */
  NACLi_386RE,        /* subset of i386 that allow REPE/REPZ prefixes */
                                                                      /* 32 */
  NACLi_JMP8,                                                         /* 32 */
  NACLi_JMPZ,                                                         /* 32 */
  NACLi_INDIRECT,                                                     /* 32 */
  NACLi_OPINMRM,                                                      /* 32 */
  NACLi_RETURN,                                                       /* 32 */
  NACLi_SFENCE_CLFLUSH,                                               /* Both */
  NACLi_CMPXCHG8B,                                                    /* Both */
  NACLi_CMPXCHG16B,   /* 64-bit mode only, illegal for NaCl */        /* Both */
  NACLi_CMOV,                                                         /* Both */
  NACLi_RDMSR,                                                        /* Both */
  NACLi_RDTSC,                                                        /* Both */
  NACLi_RDTSCP,  /* AMD only */                                       /* Both */
  NACLi_SYSCALL, /* AMD only; equivalent to SYSENTER */               /* Both */
  NACLi_SYSENTER,                                                     /* Both */
  NACLi_X87,                                                          /* Both */
  NACLi_MMX,                                                          /* Both */
  NACLi_MMXSSE2, /* MMX with no prefix, SSE2 with 0x66 prefix */      /* Both */
  NACLi_3DNOW,   /* AMD only */                                       /* Both */
  NACLi_EMMX,    /* Cyrix only; not supported yet */                  /* Both */
  NACLi_E3DNOW,  /* AMD only */                                       /* Both */
  NACLi_SSE,                                                          /* Both */
  NACLi_SSE2,    /* no prefix => MMX; prefix 66 => SSE; */            /* Both */
                 /* f2, f3 not allowed unless used for opcode selection */
  NACLi_SSE2x,   /* SSE2; prefix 66 required!!! */                    /* 32 */
  NACLi_SSE3,                                                         /* Both */
  NACLi_SSE4A,   /* AMD only */                                       /* Both */
  NACLi_SSE41,                                                        /* Both */
  NACLi_SSE42,                                                        /* Both */
  NACLi_MOVBE,                                                        /* Both */
  NACLi_POPCNT,                                                       /* Both */
  NACLi_LZCNT,                                                        /* Both */
  NACLi_LONGMODE,/* AMD only? */                                      /* 32 */
  NACLi_SVM,     /* AMD only */                                       /* Both */
  NACLi_SSSE3,                                                        /* Both */
  NACLi_3BYTE,                                                        /* 32 */
  NACLi_FCMOV,                                                        /* 32 */
  NACLi_VMX,                                                          /* 64 */
  NACLi_FXSAVE   /* SAVE/RESTORE xmm, mmx, and x87 state. */          /* 64 */
  /* NOTE: This enum must be kept consistent with kNaClInstTypeRange   */
  /* (defined below). */
} NaClInstType;
#define kNaClInstTypeRange 47
#ifdef NEEDSNACLINSTTYPESTRING
static const char *kNaClInstTypeString[kNaClInstTypeRange] = {
  "NACLi_UNDEFINED",
  "NACLi_ILLEGAL",
  "NACLi_INVALID",
  "NACLi_SYSTEM",
  "NACLi_NOP",
  "NACLi_UD2",
  "NACLi_386",
  "NACLi_386L",
  "NACLi_386R",
  "NACLi_386RE",
  "NACLi_JMP8",
  "NACLi_JMPZ",
  "NACLi_INDIRECT",
  "NACLi_OPINMRM",
  "NACLi_RETURN",
  "NACLi_SFENCE_CLFLUSH",
  "NACLi_CMPXCHG8B",
  "NACLi_CMPXCHG16B",
  "NACLi_CMOV",
  "NACLi_RDMSR",
  "NACLi_RDTSC",
  "NACLi_RDTSCP",
  "NACLi_SYSCALL",
  "NACLi_SYSENTER",
  "NACLi_X87",
  "NACLi_MMX",
  "NACLi_MMXSSE2",
  "NACLi_3DNOW",
  "NACLi_EMMX",
  "NACLi_E3DNOW",
  "NACLi_SSE",
  "NACLi_SSE2",
  "NACLi_SSE2x",
  "NACLi_SSE3",
  "NACLi_SSE4A",
  "NACLi_SSE41",
  "NACLi_SSE42",
  "NACLi_MOVBE",
  "NACLi_POPCNT",
  "NACLi_LZCNT",
  "NACLi_LONGMODE",
  "NACLi_SVM",
  "NACLi_SSSE3",
  "NACLi_3BYTE",
  "NACLi_FCMOV",
  "NACLi_VMX",
  "NACLi_FXSAVE",
};
#define NaClInstTypeString(itype)  (kNaClInstTypeString[itype])
#endif  /* ifdef NEEDSNACLINSTTYPESTRING */

typedef enum {
  NOGROUP = 0,
  GROUP1,
  GROUP2,
  GROUP3,
  GROUP4,
  /* these comments facilitate counting */
  GROUP5,
  GROUP6,
  GROUP7,
  GROUP8,
  GROUP9,
  /* these comments facilitate counting */
  GROUP10,
  GROUP11,
  GROUP12,
  GROUP13,
  GROUP14,
  /* these comments facilitate counting */
  GROUP15,
  GROUP16,
  GROUP17,
  GROUP1A,
  GROUPP
} NaClMRMGroups;
/* kModRMOpcodeGroups doesn't work as a const int since it is used */
/* as an array dimension */
#define kNaClMRMGroupsRange 20

/* Define the maximum value that can be encoded in the modrm mod field. */
#define kModRMOpcodeGroupSize 8

/* Define the maximum register value that can be encoded into the opcode
 * byte.
 */
#define kMaxRegisterIndexInOpcode 7

/* information derived from the opcode, wherever it happens to be */
typedef enum {
  IMM_UNKNOWN = 0,
  IMM_NONE = 1,
  IMM_FIXED1 = 2,
  IMM_FIXED2 = 3,
  IMM_FIXED3 = 4,
  IMM_FIXED4 = 5,
  IMM_DATAV = 6,
  IMM_ADDRV = 7,
  IMM_GROUP3_F6 = 8,
  IMM_GROUP3_F7 = 9,
  IMM_FARPTR = 10,
  IMM_MOV_DATAV,     /* Special case for 64-bits MOVs (b8 through bf). */
  /* Don't add to this enum without update kNCDecodeImmediateTypeRange */
  /* and updating the tables below which are sized using this constant */
} NCDecodeImmediateType;
#define kNCDecodeImmediateTypeRange 12

/* 255 will force an error */
static const uint8_t kImmTypeToSize66[kNCDecodeImmediateTypeRange] =
  { 0, 0, 1, 2, 3, 4, 2, (NACL_TARGET_SUBARCH == 64 ? 8 : 4), 0, 0, 6, 2};
static const uint8_t kImmTypeToSize67[kNCDecodeImmediateTypeRange] =
  { 0, 0, 1, 2, 3, 4, 4, 2, 0, 0, 4, 4};
static const uint8_t kImmTypeToSize[kNCDecodeImmediateTypeRange] =
  { 0, 0, 1, 2, 3, 4, 4, (NACL_TARGET_SUBARCH == 64 ? 8 : 4), 0, 0, 6, 4 };

/* Defines the range of possible (initial) opcodes for x87 instructions. */
static const uint8_t kFirstX87Opcode = 0xd8;
static const uint8_t kLastX87Opcode = 0xdf;

/* Defines the opcode for the WAIT instruction.
 * Note: WAIT is an x87 instruction but not in the coproc opcode space.
 */
static const uint8_t kWAITOp = 0x9b;

#define NCDTABLESIZE 256

static const int kTwoByteOpcodeByte1 = 0x0f;
static const int k3DNowOpcodeByte2 = 0x0f;
static const int kMaxPrefixBytes = 4;

/* Some essential non-macros... */
static INLINE uint8_t modrm_mod(uint8_t modrm) { return ((modrm >> 6) & 0x03);}
static INLINE uint8_t modrm_rm(uint8_t modrm) { return (modrm & 0x07); }
static INLINE uint8_t modrm_reg(uint8_t modrm) { return ((modrm >> 3) & 0x07); }
static INLINE uint8_t modrm_opcode(uint8_t modrm) { return modrm_reg(modrm); }
static INLINE uint8_t sib_ss(uint8_t sib) { return ((sib >> 6) & 0x03); }
static INLINE uint8_t sib_index(uint8_t sib) { return ((sib >> 3) & 0x07); }
static INLINE uint8_t sib_base(uint8_t sib) { return (sib & 0x07); }

/* Defines how to decode operands for byte codes. */
typedef enum {
  /* Assume the default size of the operands is 64-bits (if
   * not specified in prefix bits).
   */
  DECODE_OPS_DEFAULT_64,
  /* Assume the default size of the operands is 32-bits (if
   * not specified in prefix bits).
   */
  DECODE_OPS_DEFAULT_32,
  /* Force the size of the operands to 64 bits (prefix bits are
   * ignored).
   */
  DECODE_OPS_FORCE_64
} DecodeOpsKind;

/* Models information on an x86-32 bit instruction. */
struct OpInfo {
  NaClInstType insttype;
  uint8_t hasmrmbyte;   /* 1 if this inst has an mrm byte, else 0 */
  uint8_t immtype;      /* IMM_NONE, IMM_FIXED1, etc. */
  uint8_t opinmrm;      /* set to 1..8 if you must find opcode in MRM byte */
};

/* Models a node in a trie of NOP instructions. */
typedef struct NCNopTrieNode {
  /* The matching byte for the trie node. */
  uint8_t matching_byte;
  /* The matching modeled nop, if byte matched. */
  struct OpInfo *matching_opinfo;
  /* Node to match remaining bytes. */
  struct NCNopTrieNode* success;
  /* Node to match remaining bytes. */
  struct NCNopTrieNode* fail;
} NCNopTrieNode;

/* Models a parsed x86-32 bit instruction. */
struct InstInfo {
  /* The bytes used to parse the x86-32 instruction (may have added
   * zero filler if the instruction straddles the memory segment).
   */
  NCInstBytes bytes;
  /* The number of prefix bytes in the instruction. */
  uint8_t prefixbytes;  /* 0..4 */
  /* Number of opcode bytes in the instruction. */
  uint8_t num_opbytes;
  /* non-zero if the instruction contains an SIB byte. */
  uint8_t hassibbyte;
  /* The ModRm byte. */
  uint8_t mrm;
  /* A NCDecodeImmediateType describing the type of immediate value(s)
   * the instruction has.
   */
  uint8_t immtype;
  /* The number of bytes that define the immediate value(s). */
  uint8_t immbytes;
  /* The number of displacement bytes defined by the instruction. */
  uint8_t dispbytes;
  /* The set of prefix masks defined by the prefix bytes. */
  uint32_t prefixmask;
  /* True if it has a rex prefix. */
  uint8_t rexprefix;
};

/* Models data collected about the parsed instruction. */
typedef struct NCDecoderInst {
  /* The virtual (pc) address of the instruction. */
  NaClPcAddress vpc;
  /* The instruction rule used to decode the instruction. */
  const struct OpInfo* opinfo;
  /* The low level details of the instructionm, extracted during parsing. */
  struct InstInfo inst;
  /* Pointer to bytes of the parsed instruction (int inst) for easier access. */
  const NCInstBytesPtr inst_bytes;
  /* The decoder state the instruction appears in. */
  struct NCDecoderState* dstate;
  /* Corresopnding index of this instruction wrt to inst_buffer in
   * in the corresponding decoder state NCDecoderState.
   */
  size_t inst_index;
  /* The number of instructions parsed so far (including this instrruction).
   * Used to detect when one tries to get a previous instruction that doesn't
   * exist.
   */
  size_t inst_count;
} NCDecoderInst;

/* Given a (decoded) instruction, return the instruction that appeared
 * n elements before it, or NULL if no such instruction exists.
 *
 * Parameters:
 *    dinst - The instruction to look up relative to.
 *    n - number of elements back to look.
 */
extern NCDecoderInst *PreviousInst(const NCDecoderInst* dinst, int n);

/* Models decoding instructions in a memory region.
 *
 * Note: This struct is modeling a notion of a (virtual) base class to parse
 * a window of k instructions. In this model, we consider NCDecoderState a
 * class that can be (singly) inherited by derived classes. This code
 * assumes that the "this" pointer can be cast to a derived class
 * using a C cast. This implies that derived classes should have the
 * field NCDecoderState as its first field.
 *
 * Typical use is:
 *
 *    NCDecoderState dstate;
 *    NCDecoder inst_buffer[BUF_SIZE]; // window of BUF_SIZE instructions.
 *    NCDecoderStateConstruct(&dstate, mbase, vbase, size,
 *                            inst_buffer, BUF_SIZE);
 *    NCDecoderStateDecode(&dstate);
 *    NCDecoderStateDestruct(&dstate);
 *
 * Note: The old API for this class is further down in this file,
 * and should be considered deprecated.
 */
typedef struct NCDecoderState {
  /* PROTECTED: */

  /* The instruction buffer is an array of instructions, used
   * by the decoder to define a window of decoded instructions.
   * This window automatically moves as instructions are decoded
   * so that one can always see the current decoded instruction,
   * and some (fixed) number of previously decoded instructions.
   */
  NCDecoderInst* inst_buffer;

  /* The number of elements in inst_buffer. Must be greater than zero. */
  size_t inst_buffer_size;

  /* Remaining memory to decode. It is allocated on
   * the stack to make it thread-local, and included here
   * so that all decoder states have access to it.
   */
  NCRemainingMemory memory;

  /* The begining of the memory segment to decode. */
  uint8_t* mbase;

  /* The (virtual) base address of the memory segment. */
  NaClPcAddress vbase;

  /* The number of bytes in the memory segment. */
  NaClMemorySize size;

  /* The index of the current instruction within inst_buffer. */
  size_t cur_inst_index;

  /* Member function to apply actions to a decoded instruction. */
  NCDecoderStateAction action_fn;

  /* Member function to process new segment. */
  NCDecoderStateMethod new_segment_fn;

  /* Member function called to report an error with the validity of the
   * memory segment.
   */
  NCDecoderStateMethod segmentation_error_fn;

  /* Member function called to report other errors while processing the
   * memory segment.
   */
  NCDecoderStateMethod internal_error_fn;
} NCDecoderState;

/*
 * Construct a decoder state.
 *
 * Parameters are:
 *   this  - The instance to be constructed.
 *   mbase - The begining of the memory segment to decode.
 *   vbase - The (virtual) base address of the memory segment.
 *   sz - The number of bytes in the memory segment.
 *
 * Note: Constructors of subclasses of NCDecoderState should
 * call this constructor first, to initialize the decoder state.
 */
extern void NCDecoderStateConstruct(NCDecoderState* tthis,
                                    uint8_t* mbase, NaClPcAddress vbase,
                                    NaClMemorySize sz,
                                    NCDecoderInst* inst_buffer,
                                    size_t inst_buffer_size);

/*
 * Decodes the memory segment associated with the decoder state.
 * Returns TRUE if able to apply action to all decoded instructions.
 *
 * Parameters are:
 *   this  - The decoder state.
 */
extern Bool NCDecoderStateDecode(NCDecoderState* tthis);

/*
 * Destruct a decoder state.
 *
 * Parameters are:
 *   this  - The decoder state.
 *
 * Note: Destructors of subclasses of NCDecoderState should
 * call this destructor last, after the subinstance has been destructed.
 */
extern void NCDecoderStateDestruct(NCDecoderState* tthis);

struct NCDecoderStatePair;

/* Models a method that does a compare/update on a pair of instructions from
 * the pairwise instruction decoder. Returns true if the action succeeded.
 */
typedef Bool (*NCDecoderStatePairAction)(struct NCDecoderStatePair* tthis,
                                         struct NCDecoderInst* dinst_old,
                                         struct NCDecoderInst* dinst_new);

/* Models decoding a pair of instruction segments, compariing/updating
 * them as appropriate. Assumes that two instruction segments are the same,
 * except for some (constant-sized) changes. At the instruction level,
 * the instruction lengths are assumed to be the same. Typically, this is
 * because the one instruction segment was an updated version of a
 * previous instruction segment.
 *
 * Typical use is:
 *
 * NCDecoderState dstate_old;
 * NCDecoderState dstate_new;
 * NCDecoderStatePair dstate_pair;
 * ... Code that constructs dstate_old and dstate_new.
 * NCDecoderStatePair Construct(&dstate_pair, &dstate_old, &dstate_new);
 * NCDecoderStatePairDecode(&dstate_pair);
 * NCDecoderStatePairDestruct(&dstate_pair);
 */
typedef struct NCDecoderStatePair {
  /* PROTECTED: */

  /* The old decoder state. */
  NCDecoderState* old_dstate;

  /* The new decoder state. */
  NCDecoderState* new_dstate;

  /* The (virtual method) action to apply to each instruction. */
  NCDecoderStatePairAction action_fn;
} NCDecoderStatePair;

/*
 * Construct a decoder state pair.
 *
 * Parameters are:
 *    tthis - The decoder state pair to construct.
 *    old_dstate - A constructed old decoder state to use.
 *    new_dstate - A constructed new decoder state to use.
 *
 * Note: Constructors of subclasses of NCDecoderStatePair should
 * call this constructor first, to initialize the decoder pair fields.
 */
extern void NCDecoderStatePairConstruct(NCDecoderStatePair* tthis,
                                        NCDecoderState* old_dstate,
                                        NCDecoderState* new_dstate);

/*
 * Decode the memory segments in each instruction state, applying
 * the appropriate action on each instruction till either:
 * (1) The instruction lengths differ.
 * (2) The action returns false.
 * Returns true if no instruction lengths differ, and the action
 * returns true for all found instructions.
 */
extern Bool NCDecoderStatePairDecode(NCDecoderStatePair* tthis);

/*
 * Destruct a decoder state pair.
 *
 * Note: Destructors of subclasses of NCDecoderStatePair should
 * call this distructor last, after the subinstance has been destructed.
 */
extern void NCDecoderStatePairDestruct(NCDecoderStatePair* tthis);

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDECODE_H_ */
