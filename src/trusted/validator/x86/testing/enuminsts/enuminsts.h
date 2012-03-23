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
struct NaClEnumeratorDecoder;

/* Defines the maximum length of an instruction. */
#define NACL_ENUM_MAX_INSTRUCTION_BYTES 30

/* Defines the maximum number of enumeration decoders one can run. */
#define NACL_MAX_ENUM_DECODERS 2

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

  /* Defines the two enumerator decoders to apply. */
  struct NaClEnumeratorDecoder* _decoder[NACL_MAX_ENUM_DECODERS];

  /* Defines the number of decoders being applied. */
  size_t _num_decoders;

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
typedef void (*NaClDecoderParseInstFn)(NaClEnumerator* enmerator,
                                       int pc_address);

/* Defines the (virtual) function that returns the number of bytes in the
 * disassembled instruction.
 */
typedef size_t (*NaClDecoderInstLengthFn)(NaClEnumerator* enumerator);

/* Defines the (virtual) function that prints out the textual description
 * of the parsed instruction.
 */
typedef void (*NaClDecoderPrintInstFn)(NaClEnumerator* enumerator);

/* Defines the (virtual) function that returns the instruction mnemonic
 * for the disassembled instruction.
 */
typedef const char* (*NaClDecoderGetInstMnemonicFn)(NaClEnumerator* enumerator);

/* Defines the (virtual) function that returns the number of operands in
 * the disassembled instruction.
 */
typedef size_t (*NaClDecoderGetInstNumOperandsFn)(NaClEnumerator* enumerator);

/* Defines the (virtual) function that returns a text string describing the
 * operands of the instruciton (i.e. less the instruction mnemonic).
 */
typedef const char*
(*NaClDecoderGetInstOperandsTextFn)(NaClEnumerator* enumerator);

/* Defines the (virtual) function that returns true if operand n of the
 * disassembled instruction is a write to one of the registers RSP, RBP,
 * or R15, and the disassembler can prove it (If it can't prove it, it
 * should simply return FALSE).
 */
typedef Bool
(*NaClDecoderOperandWritesToReservedRegFn)(NaClEnumerator* enumerator,
                                           size_t n);

/* Defines the (virtual) function that tests that the instruction is legal.
 */
typedef Bool
(*NaClDecoderIsInstLegalFn)(NaClEnumerator *enumerator);

/* Defines the (virtual) function that tests that the instruction
 * validates to the level the tester can test validation.
 */
typedef Bool
(*NaClDecoderMaybeInstValidatesFn)(NaClEnumerator* enumerator);

/* Defines the (virtual) function that tests (to the limit it can) that
 * the given code segment validates. If the tester can't run the validator
 * on the segment, it should return true.
 */
typedef Bool
(*NaClDecoderMaybeSegmentValidatesFn)(NaClEnumerator* enumerator,
                                      uint8_t* segment,
                                      size_t size,
                                      int pc_address);

/* Defines the (virtual) function that processes the given global flag,
 * in terms of the corresponding tester.
 */
typedef void (*NaClDecoderInstallFlagFn)(NaClEnumerator* enumerator,
                                         const char* flag_name,
                                         void* flag_address);

/*
 * Defines the structure to hold a decoder/validation tool. Note that
 * the validation part is optional, and does not need to be supplied.
 *
 * Note: This struct acts like a C++ class with single inhertence. Derived
 * classes should define this as the first field in the struct, so that
 * they can be casted up to a NaClEnumeratorDecoder pointer.
 *
 * Note: Because not all decoders implement NaCl validation, some virtuals
 * are optional, and can be defined using NULL. In addition, some decoders
 * may not implement a full decoder, making it hard to define operands
 * of an instruction. Hence, the following virtual functions (i.e. fields)
 * are optional:
 *    _get_inst_mnemonic_fn
 *    _get_inst_num_operands_fn
 *    _get_inst_operands_text_fn;
 *    _writes_to_reserved_reg_fn;
 *    _maybe_inst_validates_fn;
 *    _segment_validates_fn;
 */
typedef struct NaClEnumeratorDecoder {
  /* The identifying name for the tester. */
  const char* _id_name;
  /* True if the legal filter should be applied to this tester. That is,
   * only report on instructions this tester finds to be a legal instruction.
   * When false, filter out instructions that are illegal.
   */
  Bool _legal_only;
  /* True if we should should not run comparison tests, but only print. */
  Bool _print_only;
  /*
   * Parses the first instruction in the itext array of the enumerator. Assumes
   * that the length of the first instruction must be <= the _num_bytes field
   * of the enumerator.
   */
  NaClDecoderParseInstFn _parse_inst_fn;
  /*
   * Returns the number of bytes in the disassembled instruction.
   */
  NaClDecoderInstLengthFn _inst_length_fn;
  /*
   * Prints out the disassembled instruction.
   */
  NaClDecoderPrintInstFn _print_inst_fn;
  /*
   * Returns the mnemonic name of the disassembled instruction.
   * If not implemented, use NULL;
   */
  NaClDecoderGetInstMnemonicFn _get_inst_mnemonic_fn;
  /*
   * Returns the number of operands in the disassembled instruction.
   * If not implemented, use NULL;
   */
  NaClDecoderGetInstNumOperandsFn _get_inst_num_operands_fn;
  /*
   * Returns a text string containing the operands of the disassembled
   * instruction. If not implemented, use NULL;
   */
  NaClDecoderGetInstOperandsTextFn _get_inst_operands_text_fn;
  /*
   * Returns true if operand n of the disassembled instruction can be
   * proven to be a write to registers RSP, RBP, or R15. If not implemented,
   * use NULL.
   */
  NaClDecoderOperandWritesToReservedRegFn _writes_to_reserved_reg_fn;
  /*
   * Returns true if the instruction is legal, according to the tester.
   */
  NaClDecoderIsInstLegalFn _is_inst_legal_fn;
  /*
   * Returns true if the instruction will validate. If not implemented,
   * use NULL. This function should only be implemented for decoders that
   * define a nacl validator.
   */
  NaClDecoderMaybeInstValidatesFn _maybe_inst_validates_fn;
  /*
   * Returns true if the segment should validate, to the best we can
   * check with this tester. If not implemented, use NULL;
   */
  NaClDecoderMaybeSegmentValidatesFn _segment_validates_fn;
  /* Processes the given command line flag. */
  NaClDecoderInstallFlagFn _install_flag_fn;
  /* Holds the usage message to printed for the decoder in the (command-line)
   * usage function.
   */
  const char* _usage_message;
} NaClEnumeratorDecoder;

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_ENUMINSTS_ENUMINST_H_ */
