/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Some useful formatting tools that allow us to build our own directive
 * processing simply. The code assumes that directives of the form
 * %X (for some character x). Further, in a format string '\\' is treated
 * the backslash character ('\') while '\%' escapes out the interpretation
 * of '%' as the beginning of a directive.
 *
 * If the specified %X directive is not understood by the directive processing
 * function, the directive will not be translated while processing the format
 * string.
 *
 * Note: Buffer insertions are based on the routines FormatDataAppend and
 * FormatAppend. Each of these routines append text to the buffer, based
 * on the current cursor position. The cursor is the index to add the next
 * character to the buffer, if the buffer was sufficiently large enough to
 * hold the appended text. Therefore, after all text has been appended, if
 * the cursor is smaller than the buffer size, no truncation occurs.
 *
 * Note: Buffer insertions will automatically add the null terminator after
 * the appended string, but the cursor is not updated to reflect this change.
 * If a buffer overflows, the last character in the buffer will be the null
 * terminator, unless the buffer size is empty. If the buffer size is empty,
 * there is no room for the null terminator and it is not inserted.
 *
 * Note: To dynamically compute the amount of space needed to format a string,
 * call on an empty buffer with size zero, and the cursor initially set to zero.
 * After the call, the value of the cursor, plus one, is the minimum sized
 * buffer that will be needed to format that string.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_UTILS_FORMMATTING_H__
#define NATIVE_CLIENT_SRC_SHARED_UTILS_FORMMATTING_H__

#include "native_client/src/shared/utils/types.h"

/*
 * Defines the generic format for a fucntion that processses directives
 * when they are found. Returns true if the directive was processed and
 * the resulting string was added at the given cursor position. When the buffer
 * fills, no additional text is added to the buffer, even though the cursor
 * is incremented accordingly. Otherwise, the buffer is left unchanged,
 * and the function must return false.
 *
 * arguments:
 *   directive - The character X of the found %X directive.
 *   buffer - The buffer to add the resulting text of the directive
 *   buffer_size - The size of the buffer.
 *   data - (Generic) pointer to the data that should be used to process the
 *          directive.
 *   cursor - The index into the buffer, where the next character
 *         should be added, if there is sufficient room in the buffer.
 */
typedef Bool (*FormatDataUsingDirectiveFcn)(
    char directive,
    char* buffer,
    size_t buffer_size,
    void* data,
    size_t* cursor);

/*
 * Defines a driver routine to process a format string and put the rsulting
 * generated text in at the cursor position. When the buffer fills, no
 * additional text is added to the buffer, even though the cursor is
 * incremented accordingly.
 *
 * Note: Buffer overflow occurs iff the cursor is greater than or
 * equal to the buffer size.
 *
 * arguments:
 *   buffer - The buffer to fill using the format.
 *   buffer_size - The size of the buffer.
 *   format - The format string to use.
 *   data - (Generic) pointer to the data that should be used to process the
 *          directives in the format string.
 *   directive_fcn - The function to process directives as they are found.
 *   cursor - The index into the buffer, where the next character should
 *          be added, if there is sufficient room in the buffer.
 */
void FormatDataAppend(char* buffer,
                      size_t buffer_size,
                      const char* format,
                      void* data,
                      FormatDataUsingDirectiveFcn directive_fcn,
                      size_t* cursor);

/*
 * Defines a driver routine to process a format string and put the resulting
 * generated text in the given buffer. Returns true iff buffer overflow doesn't
 * occur.
 *
 * arguments:
 *   buffer - The buffer to fill using the format.
 *   buffer_size - The size of the buffer.
 *   format - The format string to use.
 *   data - (Generic) pointer to the data that should be used to process the
 *          directives in the format string.
 *   directive_fcn - The function to process directives as they are found.
 */
Bool FormatData(char* buffer,
                size_t buffer_size,
                const char* format,
                void* data,
                FormatDataUsingDirectiveFcn directive_fcn);

/*
 * Append the given text at the cursor position. When the buffer fills, no
 * additional text is added to the buffer, even though the cursor is incremented
 * accordingly.
 *
 * Note: Buffer overflow occurs iff the cursor is greater than or
 * equal to the buffer size.
 *
 * arguments:
 *    buffer - The buffer to fill with the text.
 *    buffer_size - The size of the  buffer.
 *    text - The text to append.
 *    cursor - The index into the buffer, where the next character
 *         should be added, if there is sufficient room in the buffer.
 */
void FormatAppend(char* buffer,
                  size_t buffer_size,
                  const char* text,
                  size_t* index);

#endif  /* NATIVE_CLIENT_SRC_SHARED_UTILS_FORMMATTING_H__ */
