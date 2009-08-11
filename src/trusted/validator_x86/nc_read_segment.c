/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Reads in text file of hexidecimal values, and build a corresponding segment.
 *
 * Note: To see what the segment contains, run ncdis on the corresponding
 * segment to disassemble it.
 */

#include <stdlib.h>

#include "native_client/src/trusted/validator_x86/nc_read_segment.h"

/* Defines the maximum number of characters allowed on an input line
 * of the input text defined by the commands command line option.
 */
#define MAX_INPUT_LINE 4096

size_t NcReadHexText(FILE* file, uint8_t* mbase, size_t mbase_size) {
  size_t count = 0;
  char input_line[MAX_INPUT_LINE];
  char mini_buf[3];
  size_t mini_buf_index;
  char *next;
  char ch;
  while (count < mbase_size) {
    if (fgets(input_line, MAX_INPUT_LINE, file) == NULL) break;
    next = &input_line[0];
    mini_buf_index = 0;
    while ((ch = *(next++))) {
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
            mini_buf[2] = '\0';
            mbase[count++] = (uint8_t)strtoul(mini_buf, NULL, 16);
            if (count == mbase_size) return mbase_size;
            mini_buf_index = 0;
          }
          break;
        default:
          break;
      }
    }
  }
  return count;
}
