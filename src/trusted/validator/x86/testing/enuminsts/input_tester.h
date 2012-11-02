/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * input_tester.h
 * Implements a decoder that matches the instructions read from stdin.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_ENUMINSTS_INPUT_TESTER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_ENUMINSTS_INPUT_TESTER_H_

#include "native_client/src/trusted/validator/x86/testing/enuminsts/enuminsts.h"

/* Defines a reader that reads instruction byte sequences from stdin
 * (one per line). '#' denotes the beginning of a comment, and spaces
 * are allowed between (hexidecimal) bytes. If an input line is just whitespace,
 * it is skipped and the next line is read. Communicates with the input decoder
 * (see below) so that they process the same sequence of instructions.
 * Argument:
 *    ibytes - An array to put the sequence of read bytes into.
 * Returns:
 *    The number of bytes put into ibytes. If zero, there is no more input.
 */
extern int ReadAnInstruction(InstByteArray ibytes);

/* Defines an input decoder for instructions read from stdin. */
extern NaClEnumeratorDecoder* RegisterInputDecoder(void);

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_ENUMINSTS_INPUT_TESTER_H_
