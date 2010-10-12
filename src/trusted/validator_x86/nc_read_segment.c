/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include <stdio.h>
#include <stdlib.h>

#include "native_client/src/include/portability_string.h"
#include "native_client/src/trusted/validator_x86/nc_read_segment.h"

/* Defines the maximum number of characters allowed on an input line
 * of the input text defined by the commands command line option.
 */
#define NACL_MAX_INPUT_LINE 4096

static void NaClConvertHexToByte(char mini_buf[3], size_t mini_buf_index,
                                 uint8_t* mbase, size_t mbase_size,
                                 size_t* count) {
  mini_buf[mini_buf_index] = '\0';
  mbase[(*count)++] = (uint8_t)strtoul(mini_buf, NULL, 16);
  if (*count == mbase_size) {
    fprintf(stderr,
            "Error: Hex text file specifies more than %"NACL_PRIuS
            " bytes of data\n", mbase_size);
  }
}

/* Given that the first line of Hex text has been read from the given file,
 * and a byte array of the given size, this function read the text of hexidecial
 * values, puts them in the byte array, and returns how many bytes are read.
 * If any line begins with a pound sign (#), it is assumed to be a comment
 * and ignored. If the file contains more hex values that the size of the byte
 * array, an error message is printed and the read is truncated to the size of
 * the byte array. If the number of non-blank hex values aren't even, the single
 * hex value is used as the corresponding byte value.
 */
static size_t NaClReadHexData(FILE* file, uint8_t* mbase, size_t mbase_size,
                              char input_line[NACL_MAX_INPUT_LINE]) {
  size_t count = 0;
  char mini_buf[3];
  size_t mini_buf_index = 0;
  char *next;
  char ch;
  while (count < mbase_size) {
    if (input_line[0] != '#') {
      /* Not a comment line, process. */
      next = &input_line[0];
      mini_buf_index = 0;
      while (count < mbase_size && (ch = *(next++))) {
        switch (ch) {
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
          case 'a':
          case 'b':
          case 'c':
          case 'd':
          case 'e':
          case 'f':
          case 'A':
          case 'B':
          case 'C':
          case 'D':
          case 'E':
          case 'F':
            mini_buf[mini_buf_index++] = ch;
            if (2 == mini_buf_index) {
              NaClConvertHexToByte(mini_buf, mini_buf_index, mbase,
                               mbase_size, &count);
              if (count == mbase_size) {
                return mbase_size;
              }
              mini_buf_index = 0;
            }
            break;
          default:
            break;
        }
      }
    }
    if (fgets(input_line, NACL_MAX_INPUT_LINE, file) == NULL) break;
  }
  if (mini_buf_index > 0) {
    NaClConvertHexToByte(mini_buf, mini_buf_index, mbase, mbase_size, &count);
  }
  return count;
}

size_t NaClReadHexText(FILE* file, uint8_t* mbase, size_t mbase_size) {
  char input_line[NACL_MAX_INPUT_LINE];
  if (fgets(input_line, NACL_MAX_INPUT_LINE, file) == NULL) return 0;
  return NaClReadHexData(file, mbase, mbase_size, input_line);
}

size_t NaClReadHexTextWithPc(FILE* file, NaClPcAddress* pc,
                             uint8_t* mbase, size_t mbase_size) {
  char input_line[NACL_MAX_INPUT_LINE];
  *pc = 0;  /* Default if no input. */
  while (1) {
    if (fgets(input_line, NACL_MAX_INPUT_LINE, file) == NULL) return 0;
    if (input_line[0] == '#') {
      /* i.e. treat line as a comment. */
      continue;
    } else if (input_line[0] == '@') {
      *pc = (NaClPcAddress) STRTOULL(&input_line[1], NULL, 16);
    } else {
      return NaClReadHexData(file, mbase, mbase_size, input_line);
    }
  }
  /* NOT REACHED */
  return 0;
}
