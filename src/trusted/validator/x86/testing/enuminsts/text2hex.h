/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Hexidecimal text to bytes conversion tools.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_ENUMINSTS_TEXT2BYTES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_ENUMINSTS_TEXT2BYTES_H_

#include "native_client/src/trusted/validator/x86/testing/enuminsts/enuminsts.h"

/* Reads a line of text defining the sequence of bytes that defines
 * an instruction, and converts that to the corresponding sequence of
 * opcode bytes. Returns the number of bytes found. Returning zero implies
 * that no instruction opcode was found in the given text.
 * Arguments are:
 *   ibytes - The found sequence of opcode bytes.
 *   itext - The sequence of bytes to convert.
 *   context - String describing the context (i.e. filename or
 *          command line argument description).
 *   line - The line number associated with the context (if negative,
 *          it assumes that the line number shouldn't be reported).
 */
extern int Text2Bytes(InstByteArray ibytes,
                      const char* itext,
                      const char* context,
                      const int line);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_ENUMINSTS_INPUT_TESTER_H_ */
