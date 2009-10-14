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
 * Some useful formatting tools that allow us to build our own directive
 * processing simply.
 */

#include "native_client/src/shared/utils/formatting.h"

static INLINE int IntMin(int x, int y) {
  return x < y ? x : y;
}

/*
 * The set of possible states one can get in while processing directives
 * int he FormatDataDriver.
 */
typedef enum {
  ORDINARY,           /* ordinary text. */
  AFTER_BACKSLASH,    /* previous character was a (possible)
                       * escaping backslash
                       */
  AFTER_PERCENT,      /* previous character was a (possible) %
                       * for a directive
                       */
  RESET               /* change state back to ordinary text. */
} FormatDataState;

/* Adds a character to the buffer (unless buffer overflow would
 * occur). Updates the cursor to point to the next character in the buffer.
 */
static INLINE void AddToBuffer(char ch,
                               char* buffer,
                               size_t buffer_size,
                               size_t* cursor) {
  if (*cursor < buffer_size) {
    /* Don't worry about adding null terminator, let caller handle
     * case.
     */
    buffer[*cursor] = ch;
  }
  (*cursor)++;
}

void FormatDataAppend(char* buffer,
                      size_t buffer_size,
                      const char* format,
                      void* data,
                      FormatDataUsingDirectiveFcn directive_fcn,
                      size_t* cursor) {
  FormatDataState state = ORDINARY;
  char ch = *format;

  /* Stop processing if we know buffer overflow has occurred. */
  while (ch != '\0') {
    format++;
    switch (state) {
      case RESET:
      default:
        /* This shouldn't happen!! process like ordinary text.*/
        state = ORDINARY;
        /* intentionally drop to next case. */
      case ORDINARY:
        switch (ch) {
          case '%':
            state = AFTER_PERCENT;
            break;
          case '\\':
            state = AFTER_BACKSLASH;
            break;
          default:
            break;
        }
        break;
      case AFTER_BACKSLASH:
        switch (ch) {
          case '%':
          case '\\':
            /* Escaped symbol, drop initial backslash, and
             * then treat as ordinary text.
             */
            break;
          default:
            /* Not special character for escaping, ignore backslash
             * as special directive.
             */
            AddToBuffer('\\', buffer, buffer_size, cursor);
            break;
        }
        state = ORDINARY;
        break;
      case AFTER_PERCENT:
        if (directive_fcn(ch, buffer, buffer_size, data, cursor)) {
          /* Code added by directive function, so go back to ordinary state. */
          state = RESET;
        } else {
          /* Directive not understood, leave alone. */
          AddToBuffer('%', buffer, buffer_size, cursor);
          state = ORDINARY;
        }
        break;
    }
    switch (state) {
      case ORDINARY:
        AddToBuffer(ch, buffer, buffer_size, cursor);
        break;
      case RESET:
        state = ORDINARY;
        break;
      case AFTER_BACKSLASH:
      case AFTER_PERCENT:
      default:
        break;
    }
    ch = *format;
  }
  /* Be sure to null terminate the generated string. */
  if (buffer_size > 0) {
    buffer[IntMin(*cursor, buffer_size - 1)] = '\0';
  }
}

Bool FormatData(char* buffer,
                size_t buffer_size,
                const char* format,
                void* data,
                FormatDataUsingDirectiveFcn directive_fcn) {
  size_t cursor = 0;
  FormatDataAppend(buffer, buffer_size, format,
                   data, directive_fcn, &cursor);
  return cursor < buffer_size;
}

void FormatAppend(char* buffer,
                  size_t buffer_size,
                  const char* text,
                  size_t* cursor) {
  char const* next = text;
  char ch = *(next++);
  while (ch != '\0') {
    AddToBuffer(ch, buffer, buffer_size, cursor);
    ch = *(next++);
  }
  /* Be sure to null terminate the generated string. */
  if (buffer_size > 0) {
    buffer[IntMin(*cursor, buffer_size - 1)] = '\0';
  }
}
