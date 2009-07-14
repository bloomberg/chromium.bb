/*
 * Copyright 2008, Google Inc.
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
/* cdomtable.c: Generate HTML for an HTML table.
 * These C functions are modeled after JavaScript versions provided
 * as comments.
 */

#include <nacl/nacl_srpc.h>
#include "native_client/tests/cdom_perf/cdomshm.h"

/* Helper variable to create consistent values for the table elements. */
static int g_DOMTable_element_count = 0;

/* function DOMTable_CreateCellIH(row_id, col_id) {
 *  return '<td id="$' + row_id + '$' + col_id + '">'+
 *         DOMTable_element_count++ +
 *         '</td>';
 * }
 */
static inline void DOMTable_CreateCellIH(const int row_id, const int col_id,
                                         BigBuffer *bb) {
  /* This format string can produce at most 18 + 8 + 8 + 8 == 42 bytes */
  const char *kFmtString = "<td id=\"$%d$%d\">%d</td>";
  const int kMaxStringLength = 64;
  char s[kMaxStringLength];

  snprintf(&s[0], kMaxStringLength, kFmtString,
           row_id, col_id, g_DOMTable_element_count++);
  BBWrite(s, bb);
}

/* function DOMTable_CreateRowIH(row_id, cols) {
 *   var html_string = '<tr>';
 *   for (var i = 0; i < cols; i++) {
 *     html_string += DOMTable_CreateCellIH(row_id, i);
 *   }
 *   return html_string + '</tr>';
 * }
 */
static inline void DOMTable_CreateRowIH(const int row_id, const int cols,
                                        BigBuffer *bb) {
  int i;

  BBWrite("<tr>", bb);
  for (i = 0; i < cols; i++) {
    DOMTable_CreateCellIH(row_id, i, bb);
  }
  BBWrite("</tr>", bb);
}

/* function DOMTable_CreateTableIH(rows, cols) {
 *   var html_string = '<table>';
 *   for (var i = 0; i < rows; i++) {
 *     html_string += DOMTable_CreateRowIH(i, cols);
 *   }
 *   return html_string + '</table>';
 * }
 */
static void DOMTable_CreateTableIH(int rows, int cols, BigBuffer *bb) {
  int i;

  BBWrite("<table>", bb);
  for (i = 0; i < rows; i++) {
    DOMTable_CreateRowIH(i, cols, bb);
  }
  BBWrite("</table>", bb);
}

/* DOMTable_CreateTable provides the C to JavaScript interface.
 */
int CDOMTable_CreateTable(NaClSrpcChannel *channel,
                          NaClSrpcArg **in_args,
                          NaClSrpcArg **out_args) {
  int rows, cols;
  rows = in_args[0]->u.ival;
  cols = in_args[1]->u.ival;

  g_BigBuffer.index = 0;                            /* reset BigBuffer_index */
  g_DOMTable_element_count = 0;                     /* reset element count */
  DOMTable_CreateTableIH(rows, cols, &g_BigBuffer); /* create the table */
  out_args[0]->u.ival = g_BigBuffer.index;          /* return the length */
  if (0 == g_BigBuffer.failed) {
    return NACL_SRPC_RESULT_OK;
  } else {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
}
NACL_SRPC_METHOD("CDOMTable_CreateTable:ii:i", CDOMTable_CreateTable);
