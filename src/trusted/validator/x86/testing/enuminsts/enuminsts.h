/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * enuminsts.h
 *
 * Defines the general API for defining decoder / validation tools to be
 * tested by the enumeration structure.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_ENUMINSTS_ENUMINST_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_ENUMINSTS_ENUMINST_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/utils/types.h"

/* Structure holding decoder/validation tool to test. */
struct NaClEnumeratorTester;

/* Defines the maximum length of an instruction. */
#define NACL_ENUM_MAX_INSTRUCTION_BYTES 30

/* Defines the maximum number of enumeration testers one can run. */
#define NACL_MAX_ENUM_TESTERS 2

/*
 * Defines the data structure used by the driver to enumerate possible
 * instruction encodings.
 */
typedef struct NaClEnumerator {
  /* Defines the buffer of bytes generated for an enumeration.
   */
  uint8_t _itext[NACL_ENUM_MAX_INSTRUCTION_BYTES];

  /* Defines the actual number of bytes to be tried within _itext. */
  size_t _num_bytes;

  /* Defines the two enumerator testers to apply. */
  struct NaClEnumeratorTester* _tester[NACL_MAX_ENUM_TESTERS];

  /* Defines the number of testers being applied. */
  size_t _num_testers;

  /* Defines flag defining if only opcode bytes should be printed. */
  Bool _print_opcode_bytes_only;

  /*
   * Returns true if the enumerated  instruction should be printed out in
   * a easily to read form. That is, the sequence of opcodes,
   * followed by "#", followed by the instruction text.
   */
  Bool _print_enumerated_instruction;
} NaClEnumerator;

/* Define the (virtual) function to parse the first instruction in the itext
 * array of the enumerator. Assumes that the length of the first instruction
 * must be no larger than the _num_bytes field of the enumerator.
 */
typedef void (*NaClTesterParseInstFn)(NaClEnumerator* enmerator,
                                      int pc_address);

/* Defines the (virtual) function that returns the number of bytes in the
 * disassembled instruction.
 */
typedef size_t (*NaClTesterInstLengthFn)(NaClEnumerator* enumerator);

/* Defines the (virtual) function that prints out the textual description
 * of the parsed instruction.
 */
typedef void (*NaClTesterPrintInstFn)(NaClEnumerator* enumerator);

/* Defines a (virtual) function that prints out the textual description
 * of the parsed instruction conditionally, based on the callback to
 * the NaClTesterSetConditionalPrintingFn (virtual) function (defined below).
 * Returns true if printing occurs.
 */
typedef Bool (*NaClTesterConditionallyPrintInstFn)(NaClEnumerator* enumerator);

/* Defines a (virtual) function to set the behaviour of the virtual
 * NaClTesterConditionallyPrintInstFn (defined above).
 */
typedef void (*NaClTesterSetConditionalPrintingFn)(NaClEnumerator* enumerator,
                                                   Bool new_value);

/* Defines the (virtual) function that returns the instruction mnemonic
 * for the disassembled instruction.
 */
typedef const char* (*NaClTesterGetInstMnemonicFn)(NaClEnumerator* enumerator);

/* Defines the (virtual) function that returns the number of operands in
 * the disassembled instruction.
 */
typedef size_t (*NaClTesterGetInstNumOperandsFn)(NaClEnumerator* enumerator);

/* Defines the (virtual) function that returns a text string describing the
 * operands of the instruciton (i.e. less the instruction mnemonic).
 */
typedef const char*
(*NaClTesterGetInstOperandsTextFn)(NaClEnumerator* enumerator);

/* Defines the (virtual) function that returns true if operand n of the
 * disassembled instruction is a write to one of the registers RSP, RBP,
 * or R15, and the disassembler can prove it (If it can't prove it, it
 * should simply return FALSE).
 */
typedef Bool
(*NaClTesterOperandWritesToReservedRegFn)(NaClEnumerator* enumerator, size_t n);

/* Defines the (virtual) function that tests (to the limit it can) that the
 * insruction is legal, and validates. For any instruction that is legal, but
 * one can't prove that it will validate (in context of surrounding
 * instructions) should return true.
 */
typedef Bool
(*NaClTesterMaybeInstValidatesFn)(NaClEnumerator* enumerator);

/*
 * Defines the structure to hold a decoder/validation tool. Note that
 * the validation part is optional, and does not need to be supplied.
 *
 * Note: This struct acts like a C++ class with single inhertence. Derived
 * classes should define this as the first field in the struct, so that
 * they can be casted up to a NaClEnumeratorTester pointer.
 */
typedef struct NaClEnumeratorTester {
  /* The identifying name for the tester. */
  const char* _id_name;
  /*
   * Parses the first instruction in the itext array of the enumerator. Assumes
   * that the length of the first instruction must be <= the _num_bytes field
   * of the enumerator.
   */
  NaClTesterParseInstFn _parse_inst_fn;
  /*
   * Returns the number of bytes in the disassembled instruction.
   */
  NaClTesterInstLengthFn _inst_length_fn;
  /*
   * Prints out the disassembled instruction.
   */
  NaClTesterPrintInstFn _print_inst_fn;
  /*
   * Prints out the disassembled instruction conditionally, based on
   * calls to the _set_conditional_printing_fn.
   */
  NaClTesterConditionallyPrintInstFn _conditionally_print_inst_fn;
  /*
   * Function to call to define if _set_conditional_printing_fn should
   * conditionally print.
   */
  NaClTesterSetConditionalPrintingFn _set_conditional_printing_fn;
  /*
   * Returns the mnemonic name of the disassembled instruction.
   */
  NaClTesterGetInstMnemonicFn _get_inst_mnemonic_fn;
  /*
   * Returns the number of operands in the disassembled instruction.
   */
  NaClTesterGetInstNumOperandsFn _get_inst_num_operands_fn;
  /*
   * Returns a text string containing the operands of the disassembled
   * instruction.
   */
  NaClTesterGetInstOperandsTextFn _get_inst_operands_text_fn;
  /*
   * Returns true if operand n of the disassembled instruction can be
   * proven to be a write to registers RSP, RBP, or R15.
   */
  NaClTesterOperandWritesToReservedRegFn _writes_to_reserved_reg_fn;
  /*
   * Returns true if the instruction is legal, and to the best we can
   * check, it validates.
   */
  NaClTesterMaybeInstValidatesFn _maybe_inst_validates_fn;
} NaClEnumeratorTester;

/***************************************************************************
 * The following are helper functions to make it easier to write a tester. *
 * The following are a set of simple ASCII string processing routines
 * used to parse/transform Xed and NaCl disassembler output.
 ***************************************************************************/

/* If string s begins with string prefix, return a pointer to the
 * first byte after the prefix. Else return s.
 * PRECONDITION: plen == strlen(prefix)
 */
char *SkipPrefix(char *s, const char *prefix, int plen);

/* Return a pointer to s with leading spaces removed. */
char *strip(char *s);

/* Copy src to dest, stoping at character c. */
void strncpyto(char *dest, const char *src, size_t n, char c);

/* Remove all instances of substring ss in string s, modifying s in place. */
void CleanString(char *s, const char *ss);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_ENUMINSTS_ENUMINST_H_ */
