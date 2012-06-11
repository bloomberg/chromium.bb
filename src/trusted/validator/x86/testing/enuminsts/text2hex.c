/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Hexidecimal text to bytes conversion tools.
 */
#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB.")
#endif

#include "native_client/src/trusted/validator/x86/testing/enuminsts/text2hex.h"

#include <stdio.h>

#include "native_client/src/include/portability_io.h"

#define kBufferSize 1024

/* Returns the integer corresponding to a hex value in ASCII. This would
 * be faster as an array lookup, however since it's only used for command
 * line input it doesn't matter.
 */
static unsigned int A2INibble(const char nibble) {
  switch (nibble) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a':
    case 'A': return 0xA;
    case 'b':
    case 'B': return 0xB;
    case 'c':
    case 'C': return 0xC;
    case 'd':
    case 'D': return 0xD;
    case 'e':
    case 'E': return 0xE;
    case 'f':
    case 'F': return 0xF;
    default: break;
  }
  fprintf(stderr, "bad hex value %c", nibble);
  exit(1);
}

/* Convert a two-character string representing an byte value in hex
 * into the corresponding byte value.
 */
static unsigned int A2IByte(const char nibble1, const char nibble2) {
  return A2INibble(nibble2) + A2INibble(nibble1) * 0x10;
}

/* Generates a buffer containing the context message to print.
 * Arguments are:
 *   context - String describing the context (i.e. filename or
 *          command line argument description).
 *   line - The line number associated with the context (if negative,
 *          it assumes that the line number shouldn't be reported).
 */
static const char* TextContext(const char* context,
                               const int line) {
  if (line < 0) {
    return context;
  } else {
    static char buffer[kBufferSize];
    SNPRINTF(buffer, kBufferSize, "%s line %d", context, line);
    return buffer;
  }
}

/* Installs byte into the byte buffer. Returns the new value for num_bytes.
 * Arguments are:
 *   ibytes - The found sequence of opcode bytes.
 *   num_bytes - The number of bytes currently in ibytes.
 *   mini_buf - The buffer containing the two hexidecimal characters to convert.
 *   context - String describing the context (i.e. filename or
 *          command line argument description).
 *   line - The line number associated with the context (if negative,
 *          it assumes that the line number shouldn't be reported).
 */
static int InstallTextByte(InstByteArray ibytes,
                           int num_bytes,
                           const char mini_buf[2],
                           const char* itext,
                           const char* context,
                           const int line) {
  if (num_bytes == NACL_ENUM_MAX_INSTRUCTION_BYTES) {
    char buffer[kBufferSize];
    SNPRINTF(buffer, kBufferSize,
             "%s: opcode sequence too long in '%s'",
             TextContext(context, line), itext);
    ReportFatalError(buffer);
  }
  ibytes[num_bytes] = A2IByte(mini_buf[0], mini_buf[1]);
  return num_bytes + 1;
}

/* Reads a line of text defining the sequence of bytes that defines
 * an instruction, and converts that to the corresponding sequence of
 * opcode bytes. Returns the number of bytes found. Arguments are:
 *   ibytes - The found sequence of opcode bytes.
 *   itext - The sequence of bytes to convert.
 *   context - String describing the context (i.e. filename or
 *          command line argument description).
 *   line - The line number associated with the context (if negative,
 *          it assumes that the line number shouldn't be reported).
 */
int Text2Bytes(InstByteArray ibytes,
               const char* itext,
               const char* context,
               const int line) {
  char mini_buf[2];
  size_t mini_buf_index;
  char *next;
  char ch;
  int num_bytes = 0;
  Bool continue_translation = TRUE;

  /* Now process text of itext. */
  next = (char*) &itext[0];
  mini_buf_index = 0;
  while (continue_translation && (ch = *(next++))) {
    switch (ch) {
      case '#':
        /* Comment, skip reading any more characters. */
        continue_translation = FALSE;
        break;
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
        /* Hexidecimal character. Add to mini buffer, and install
         * if two bytes.
         */
        mini_buf[mini_buf_index++] = ch;
        if (2 == mini_buf_index) {
          num_bytes = InstallTextByte(ibytes, num_bytes, mini_buf, itext,
                                      context, line);
          mini_buf_index = 0;
        }
        break;
      case ' ':
      case '\t':
      case '\n':
        /* Space - assume it is a separator between bytes. */
        switch(mini_buf_index) {
          case 0:
            break;
          case 2:
            num_bytes = InstallTextByte(ibytes, num_bytes, mini_buf, itext,
                                        context, line);
            mini_buf_index = 0;
            break;
          default:
            continue_translation = FALSE;
        }
        break;
      default: {
        char buffer[kBufferSize];
        SNPRINTF(buffer, kBufferSize,
                 "%s: contains bad text:\n   '%s'\n",
                 TextContext(context, line), itext);
        ReportFatalError(buffer);
        break;
      }
    }
  }

  /* If only a single byte was used to define hex value, convert it. */
  if (mini_buf_index > 0) {
    char buffer[kBufferSize];
    SNPRINTF(buffer, kBufferSize,
             "%s: Opcode sequence must be an even number of chars '%s'",
             TextContext(context, line), itext);
    ReportFatalError(buffer);
  }

  return num_bytes;
}
