/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Reads in text file of hexidecimal values, and build a corresponding segment.
 *
 * Note: To see what the segment contains, run ncdis on the corresponding
 * segment to disassemble it.
 *
 * Note: The purpose of this code is to make it easy to specify the contents
 * of code segments using textual values, so that tests are easier to write.
 * The code is NOT industrial strength and shouldn't be used except for simple
 * test cases.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_READ_SEGMENT_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_READ_SEGMENT_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include <stdio.h>
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator/types_memory_model.h"

/* Given a file, and a byte array of the given size, this function
 * opens the corresponding file, reads the text of hexidecimal values, puts
 * them in the byte array, and returns how many bytes were read. If any
 * line begins with a pound sign (#), it is assumed to be a comment and
 * ignored. If the file contains more hex values than the size of the byte
 * array, the read is truncated to the size of the byte array. If the number
 * of non-blank hex values aren't even, the single hex value is used as the
 * the corresponding byte value.
 */
size_t NaClReadHexText(FILE* input, uint8_t* mbase, size_t mbase_size);

/* Same as NcReadHexText, except if the first (non-comment) line has
 * an at sign (@) in column 1, it assumes that the first line is specify
 * a value for the initial program counter (pc). If the first line doesn't
 * specify a value for the pc, zero is assumed. All other lines are
 * assumed to be hex values to be converted to byte values and placed
 * into the byte array.
 */
size_t NaClReadHexTextWithPc(FILE* input, NaClPcAddress* pc,
                             uint8_t* mbase, size_t mbase_size);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_READ_SEGMENT_H_ */
