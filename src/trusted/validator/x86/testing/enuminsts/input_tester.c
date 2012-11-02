/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * input_tester.c
 * Implements a decoder that matches the input enumeration fed into stdin.
 */
#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB.")
#endif

#include "native_client/src/trusted/validator/x86/testing/enuminsts/input_tester.h"

#include <stdio.h>
#include <string.h>

#include "native_client/src/trusted/validator/types_memory_model.h"
#include "native_client/src/trusted/validator/x86/ncinstbuffer.h"
#include "native_client/src/trusted/validator/x86/testing/enuminsts/str_utils.h"
#include "native_client/src/trusted/validator/x86/testing/enuminsts/text2hex.h"

#define kBufferSize 1024

/* Defines the virtual table for the input decoder. */
static struct {
  /* The virtual table that implements this decoder. */
  NaClEnumeratorDecoder base_;
  /* The iput text line. */
  char line_[kBufferSize];
  /* The number of bytes in the last instruction read from the
   * input stream.
   */
  int num_bytes_;
  /* The input line number associated with the last instruction
   * read from the input stream.
   */
  int line_number_;
  /* The specified pc address when parsing. */
  NaClPcAddress pc_address_;
  /* The instruction mnemonic, if defined. */
  char* mnemonic_;
  /* The instruction arguments, if defined. */
  char* operands_;
  /* Buffer used to hold mnemonic name and operands. */
  char buffer_[kBufferSize];
  /* Boolean flag defining if we have processed the first
   * line of input, which specifies how to configure the
   * the input decoder.
   */
  Bool configured_;
} input_decoder;

/* Defines the function to parse the first instruction in the enumerator
 * text. Since we are accepting the input that was set up the input
 * enumeration, there is nothing to do.
 */
static void ParseInst(const NaClEnumerator* enumerator,
                      const int pc_address) {
  UNREFERENCED_PARAMETER(enumerator);
  input_decoder.pc_address_ = pc_address;
  input_decoder.mnemonic_ = NULL;
  input_decoder.operands_ = NULL;
}

/* Finds the instruction mnemonic, by looking at the end of the
 * input line. Looks for a comment of the form '#mnemonic operands'
 */
static void AssembleDesc(const NaClEnumerator* enumerator) {
  char* desc;
  char* end;
  UNREFERENCED_PARAMETER(enumerator);

  /* Start by looking for description. */
  desc = (char*) strip(strskip(input_decoder.line_, "#"));
  if (desc == NULL) {
    /* Not found, fill in a default value. */
    input_decoder.mnemonic_ = "???";
    input_decoder.operands_ = "";
    return;
  }
  /* Copy the description into the buffer, and then extract the needed parts. */
  cstrncpy(input_decoder.buffer_, desc, kBufferSize);
  input_decoder.mnemonic_ = input_decoder.buffer_;
  end = (char*) strfind(input_decoder.buffer_, " ");
  if (end == NULL) {
    /* No operands, clean up mnemonic. */
    rstrip(input_decoder.buffer_);
    input_decoder.operands_ = "";
    return;
  }
  /* Has mnemonic and operands. Separate out parts. */
  *end = '\0';
  input_decoder.operands_ = (char*) strip(end + 1);
  rstrip(input_decoder.operands_);
}

/* Finds the instruction mnemonic, by looking at the end of the
 * input line. Looks for a comment of the form '#mnemonic operands'
 */
static const char* GetInstMnemonic(const NaClEnumerator* enumerator) {
  if (input_decoder.mnemonic_ != NULL) return input_decoder.mnemonic_;
  AssembleDesc(enumerator);
  return input_decoder.mnemonic_;
}

/* Finst the instruction operands, by looking at the end of the
 * input line. Looks for a comment of the form '#mnemonic operands'
 */
static const char* GetInstOperandsText(const NaClEnumerator* enumerator) {
  if (input_decoder.operands_ != NULL) return input_decoder.operands_;
  AssembleDesc(enumerator);
  return input_decoder.operands_;
}


/* Prints out the disassembled instruction. */
static void PrintInst(const NaClEnumerator* enumerator) {
  int i;

  printf("   IN: %"NACL_PRIxNaClPcAddressAll": ", input_decoder.pc_address_);
  for (i = 0; i < input_decoder.num_bytes_; ++i) {
    printf("%02x ", enumerator->_itext[i]);
  }
  for (i = input_decoder.num_bytes_; i < MAX_INST_LENGTH; ++i) {
    printf("   ");
  }

  /* Print out decoding if included on the input line. */
  if (NULL == input_decoder.base_._get_inst_mnemonic_fn) {
    printf("\n");
  } else {
    printf("%s %s\n", GetInstMnemonic(enumerator),
           GetInstOperandsText(enumerator));
  }
}

/* Returns true if the instruction parsed a legal instruction. */
static Bool IsInstLegal(const NaClEnumerator* enumerator) {
  UNREFERENCED_PARAMETER(enumerator);
  return TRUE;
}

static size_t InstLength(const NaClEnumerator* enumerator) {
  UNREFERENCED_PARAMETER(enumerator);
  return (size_t) input_decoder.num_bytes_;
}

static void InstallFlag(const NaClEnumerator* enumerator,
                        const char* flag_name,
                        const void* flag_address) {
  UNREFERENCED_PARAMETER(enumerator);
  UNREFERENCED_PARAMETER(flag_name);
  UNREFERENCED_PARAMETER(flag_address);
}


/* Defines the registry function that creates a input decoder, and returns
 * the decoder to be registered.
 */
NaClEnumeratorDecoder* RegisterInputDecoder(void) {
  input_decoder.base_._id_name = "in";
  input_decoder.base_._legal_only = TRUE;
  input_decoder.base_._parse_inst_fn = ParseInst;
  input_decoder.base_._inst_length_fn = InstLength;
  input_decoder.base_._print_inst_fn = PrintInst;
  /* Initially assume that the input doesn't get information on
   * mnemonic and operand text. Change (in InstallFlag) above if
   * specified on command line.
   */
  input_decoder.base_._get_inst_mnemonic_fn = NULL;
  input_decoder.base_._get_inst_num_operands_fn = NULL;
  input_decoder.base_._get_inst_operands_text_fn = NULL;
  input_decoder.base_._writes_to_reserved_reg_fn = NULL;
  input_decoder.base_._is_inst_legal_fn = IsInstLegal;
  input_decoder.base_._maybe_inst_validates_fn = NULL;
  input_decoder.base_._segment_validates_fn = NULL;
  input_decoder.base_._install_flag_fn = InstallFlag;
  input_decoder.base_._usage_message = "Defines legal instructions from stdin";
  input_decoder.num_bytes_ = 0;
  input_decoder.line_number_ = 0;
  input_decoder.pc_address_ = 0;
  input_decoder.mnemonic_ = NULL;
  input_decoder.operands_ = NULL;
  input_decoder.configured_ = FALSE;
  return &input_decoder.base_;
}

int ReadAnInstruction(InstByteArray ibytes) {
  input_decoder.num_bytes_ = 0;
  while (input_decoder.num_bytes_ == 0) {
    ++input_decoder.line_number_;
    if (fgets(input_decoder.line_, kBufferSize, stdin) == NULL) return 0;

    /* If the line specifies that the input has opcode sequences plus
     * descriptions, then install the virtuals to handle the input.
     */
    if (!input_decoder.configured_) {
      if (input_decoder.line_ ==
          strstr(input_decoder.line_, "#OPCODEPLUSDESC#")) {
        input_decoder.base_._get_inst_mnemonic_fn = GetInstMnemonic;
        input_decoder.base_._get_inst_operands_text_fn = GetInstOperandsText;
      }
      input_decoder.configured_ = TRUE;
    }

    /* If the line is a progress line, print out the corresponding progress
     * message.
     */
    if (input_decoder.line_ == strstr(input_decoder.line_, "#PROGRESS#")) {
      printf("%s", &input_decoder.line_[strlen("#PROGRESS#")]);
    }

    /* Finally, convert the input into the corresponding sequence of bytes
     * that defines the instruction.
     */
    input_decoder.num_bytes_ =
        Text2Bytes(ibytes, input_decoder.line_, "stdin",
                   input_decoder.line_number_);
  }
  return input_decoder.num_bytes_;
}
