/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * nacl_tester.c
 * Uses the NaCl x86 validator/decoder to implement a NaClEnumeratorDecoder.
 */

#include "native_client/src/trusted/validator/x86/testing/enuminsts/enuminsts.h"

#include <string.h>
#include "native_client/src/trusted/validator_x86/ncenuminsts.h"
#include "native_client/src/trusted/validator/x86/testing/enuminsts/str_utils.h"

#define kBufferSize 1024

/* Defines the virtual table for the nacl decoder. */
struct {
  /* The virtual table that implements this decoder. */
  NaClEnumeratorDecoder _base;
  /* Defines the NaCl state to use to parse instructions. */
  NaClInstStruct *_inst;
  /* Defines if we are ignoring the instruction (i.e. an instruction
   * that isn't xed implemented.
   */
  Bool _ignore_instruction;
  /* Defines the pc address associated with the instruction. */
  NaClPcAddress _pc_address;
  /* If non-empty, the corresponding disassembly. */
  char _disassembly[kBufferSize];
  /* If non-empty, the simplified corresponding assembly, where
   * the opcode byte sequence has been removed.
   */
  char _simplified_disassembly[kBufferSize];
  /* If non-empty, the corresponding instruction mnemonic. */
  char _mnemonic[kBufferSize];
  /* if non-empty, the lowercase mnemonic. */
  char _mnemonic_lower[kBufferSize];
  /* If non-empty, stores the operands of the corresponding instruction. */
  char _operands[kBufferSize];
  /* True if we should translate special opcodes to matching xed nops. */
  Bool _translate_to_xed_nops;
  /* True if we shouldn't accept instructions that aren't also implemented
   * in xed.
   */
  Bool _ignore_instructions_not_xed_implemented;
} nacl_decoder;

static Bool IsInstLegal(NaClEnumerator *enumerator) {
  return !nacl_decoder._ignore_instruction &&
      NaClInstDecodesCorrectly(nacl_decoder._inst);
}

/* Instructions we assume that the NaCl validator accept and are valid, but
 * are not legal according to xed.
 */
static Bool NaClIsntXedImplemented(NaClEnumerator *enumerator) {
  static const char* nacl_but_not_xed[] = {
    "Pf2iw",
    "Pf2id"
  };
  const char* name = NaClOpcodeName(nacl_decoder._inst);
  int i;
  for (i = 0; i < NACL_ARRAY_SIZE(nacl_but_not_xed); ++i) {
    if (0 == strcmp(name, nacl_but_not_xed[i])) {
      return TRUE;
    }
  }
  return FALSE;
}

/* Defines the funcdtion to parse the first instruction. */
static void ParseInst(NaClEnumerator* enumerator, int pc_address) {
  nacl_decoder._inst = NULL;
  nacl_decoder._ignore_instruction = FALSE;
  nacl_decoder._pc_address = pc_address;
  nacl_decoder._disassembly[0] = 0;
  nacl_decoder._simplified_disassembly[0] = 0;
  nacl_decoder._mnemonic[0] = 0;
  nacl_decoder._mnemonic_lower[0] = 0;
  nacl_decoder._operands[0] = 0;
  nacl_decoder._inst =
      NaClParseInst(enumerator->_itext, enumerator->_num_bytes, pc_address);
  if (nacl_decoder._ignore_instructions_not_xed_implemented &&
      IsInstLegal(enumerator)) {
    nacl_decoder._ignore_instruction = NaClIsntXedImplemented(enumerator);
  }
}


/* Returns the disassembled instruction. */
static const char* Disassemble(NaClEnumerator* enumerator) {
  char* stmp;

  /* First see if we have cached it. If so, return it. */
  if (nacl_decoder._disassembly[0] != 0) return nacl_decoder._disassembly;

  stmp = NaClInstToStr(nacl_decoder._inst);
  cstrncpy(nacl_decoder._disassembly, stmp, kBufferSize);
  free(stmp);
  return nacl_decoder._disassembly;
}

/* Returns the lower case name of the instruction. */
static const char* GetInstMnemonicLower(NaClEnumerator* enumerator) {
  char mnemonic[kBufferSize];
  size_t i;

  if (nacl_decoder._mnemonic_lower[0] != 0) return nacl_decoder._mnemonic_lower;

  cstrncpy(mnemonic, NaClOpcodeName(nacl_decoder._inst), kBufferSize);
  for (i = 0; i < kBufferSize; ++i) {
    mnemonic[i] = tolower(mnemonic[i]);
    if (mnemonic[i] == '\0') break;
  }
  cstrncpy(nacl_decoder._mnemonic_lower, mnemonic, kBufferSize);
  return nacl_decoder._mnemonic_lower;
}

/* Defines nacl/xed mnemonic renaming pairs. */
typedef struct {
  const char* nacl_name;
  const char* xed_name;
} NaClToXedPairs;

/* Returns the disassembled instruction with the opcode byte sequence
 * removed.
 */
static const char* SimplifiedDisassembly(NaClEnumerator *enumerator) {
  const char* disassembly;
  const char* mnemonic;
  const char* start;

  /* First see if we have cache it. If so, return it. */
  if (nacl_decoder._simplified_disassembly[0] != 0)
    return nacl_decoder._simplified_disassembly;

  /* Take first guess of simplified assembly. */
  /* Find start of instruction mnemonic, and define as start of simplified
   * disassembly.
   */
  disassembly = Disassemble(enumerator);
  mnemonic = GetInstMnemonicLower(enumerator);
  start = strfind(disassembly, mnemonic);
  if (NULL == start) {
    /* Don't know how to simplify, give up and just use disassembly. */
    cstrncpy(nacl_decoder._simplified_disassembly, disassembly, kBufferSize);
    return;
  }
  cstrncpy(nacl_decoder._simplified_disassembly, start, kBufferSize);
  rstrip(nacl_decoder._simplified_disassembly);

  /* Now handle special cases where we should treat the nacl instruction as a
   * nop, so that they will match xed instructions.
   */
  if (nacl_decoder._translate_to_xed_nops) {
    static const NaClToXedPairs pairs[] = {
      { "xchg %eax, %eax" , "nop" },
      { "xchg %rax, %rax" , "nop" },
      { "xchg %ax, %ax" , "nop" },
    };
    int i;
    char buf[kBufferSize];
    const char* desc = strfind(nacl_decoder._simplified_disassembly, mnemonic);
    if (NULL == desc) return nacl_decoder._simplified_disassembly;
    cstrncpy(buf, desc, kBufferSize);
    rstrip(buf);
    for (i = 0; i < NACL_ARRAY_SIZE(pairs); ++i) {
      if (0 == strcmp(nacl_decoder._simplified_disassembly,
                      pairs[i].nacl_name)) {
        cstrncpy(nacl_decoder._simplified_disassembly,
                 pairs[i].xed_name, kBufferSize);
        cstrncpy(nacl_decoder._mnemonic, pairs[i].xed_name, kBufferSize);
        return;
      }
    }
  }
}

/* Returns the mnemonic name for the disassembled instruction. */
static const char* GetInstMnemonic(NaClEnumerator* enumerator) {
  char mnemonic[kBufferSize];
  const char* disassembly;

  /* First see if we have cached it. If so, return it. */
  if (nacl_decoder._mnemonic[0] != 0) return nacl_decoder._mnemonic;

  /* Force simplifications if needed. Use mnemonic if defined. */
  disassembly = SimplifiedDisassembly(enumerator);
  if (nacl_decoder._mnemonic[0] != 0) return nacl_decoder._mnemonic;

  /* If reached, we haven't cached it, so find the name from the
   * disassembled instruction and cache it.
   */
  cstrncpy(mnemonic, GetInstMnemonicLower(enumerator), kBufferSize);

  /* Now fix mnemonic to corresponding xed name if needed. */
  if ((mnemonic[0] == 'p') && (mnemonic[1] == 'f') && (mnemonic[2] == 'r')) {
    static const NaClToXedPairs pairs[] = {
      { "pfrsqrt", "pfsqrt" },
      { "pfrcpit1", "pfcpit1" }
    };
    int i;
    for (i = 0; i < NACL_ARRAY_SIZE(pairs); ++i) {
      if (0 == strcmp(mnemonic, pairs[i].nacl_name)) {
        const char* start =
            strfind(nacl_decoder._simplified_disassembly, mnemonic);
        if (NULL != start) {
          /* replace nacl_name with xed name in simplified disassembly. */
          cstrncpy(mnemonic, pairs[i].xed_name, kBufferSize);
        }
      }
    }
  }

  /* Install mnemonic and return. */
  cstrncpy(nacl_decoder._mnemonic, mnemonic, kBufferSize);
  return nacl_decoder._mnemonic;
}

/* Returns the text for the operands. To be used by the driver
 * to compare accross decoders.
 */
static const char* GetInstOperandsText(NaClEnumerator* enumerator) {
  char sbuf[kBufferSize];
  char operands[kBufferSize];
  const char* disassembly;
  const char* after_mnemonic;

  /* First see if we have cached it. If so, return it. */
  if (nacl_decoder._operands[0] != 0) return nacl_decoder._operands;

  disassembly = SimplifiedDisassembly(enumerator);
  after_mnemonic = strskip(disassembly, GetInstMnemonicLower(enumerator));
  if (NULL == after_mnemonic) after_mnemonic = disassembly;
  cstrncpy(operands, after_mnemonic, kBufferSize);
  strnzapchar(operands, '%');
  strnzapchar(operands, '\n');
  cstrncpy(nacl_decoder._operands, strip(operands), kBufferSize);
  return nacl_decoder._operands;
}

/* Prints out the disassembled instruction. */
static void PrintInst(NaClEnumerator* enumerator) {
  int i;
  uint8_t length = NaClInstLength(nacl_decoder._inst);
  if (enumerator->_print_opcode_bytes_only) {
    for (i = 0; i < length; ++i) {
      printf("%02x", enumerator->_itext[i]);
    }
    printf("\n");
  } else {
    if (enumerator->_print_enumerated_instruction) {
      for (i = 0; i < length; ++i) {
        printf("%02x ", enumerator->_itext[i]);
      }
      printf("#%s %s\n", GetInstMnemonic(enumerator),
             GetInstOperandsText(enumerator));
    } else {
      printf(" NaCl: %s", Disassemble(enumerator));
    }
  }
}

/* Returns the number of bytes in the disassembled instruction. */
static size_t InstLength(NaClEnumerator* enumerator) {
  return (size_t) NaClInstLength(nacl_decoder._inst);
}

/* Returns true if the instruction decodes, and static (single instruction)
 * validator tests pass.
 */
static Bool MaybeInstValidates(NaClEnumerator *enumerator) {
  return NaClInstValidates(enumerator->_itext,
                           NaClInstLength(nacl_decoder._inst),
                           nacl_decoder._pc_address,
                           nacl_decoder._inst);
}

/* Runs the validator on the given code segment. */
static Bool SegmentValidates(NaClEnumerator *enumerator,
                             uint8_t* segment,
                             size_t size,
                             int pc_address) {
  return NaClSegmentValidates(segment, size, pc_address);
}

/* Installs NaCl-specific flags. */
static void InstallFlag(NaClEnumerator* enumerator,
                        const char* flag_name,
                        void* flag_address) {
  if (0 == strcmp(flag_name, "--nops")) {
    nacl_decoder._translate_to_xed_nops = *((Bool*) flag_address);
  } else if (0 == strcmp(flag_name, "--xedimplemented")) {
    nacl_decoder._ignore_instructions_not_xed_implemented =
        *((Bool*) flag_address);
  }
}

/* Generates a decoder for the (sel_ldr) nacl validator. */
NaClEnumeratorDecoder* RegisterNaClDecoder() {
  nacl_decoder._base._id_name = "nacl";
  nacl_decoder._base._legal_only = TRUE;
  nacl_decoder._base._print_only = FALSE;
  nacl_decoder._base._parse_inst_fn = ParseInst;
  nacl_decoder._base._inst_length_fn = InstLength;
  nacl_decoder._base._print_inst_fn = PrintInst;
  nacl_decoder._base._get_inst_mnemonic_fn = GetInstMnemonic;
  nacl_decoder._base._get_inst_num_operands_fn = NULL;
  nacl_decoder._base._get_inst_operands_text_fn = GetInstOperandsText;
  nacl_decoder._base._writes_to_reserved_reg_fn = NULL;
  nacl_decoder._base._is_inst_legal_fn = IsInstLegal;
  nacl_decoder._base._maybe_inst_validates_fn = MaybeInstValidates;
  nacl_decoder._base._segment_validates_fn = SegmentValidates;
  nacl_decoder._base._install_flag_fn = InstallFlag;
  nacl_decoder._base._usage_message =
      "Runs nacl decoder to decode instructions";
  return &nacl_decoder._base;
}
