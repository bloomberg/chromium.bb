/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
void CDOMTable_CreateTable(NaClSrpcRpc *rpc,
                           NaClSrpcArg **in_args,
                           NaClSrpcArg **out_args,
                           NaClSrpcClosure* done) {
  int rows, cols;
  rows = in_args[0]->u.ival;
  cols = in_args[1]->u.ival;

  g_BigBuffer.index = 0;                            /* reset BigBuffer_index */
  g_DOMTable_element_count = 0;                     /* reset element count */
  DOMTable_CreateTableIH(rows, cols, &g_BigBuffer); /* create the table */
  out_args[0]->u.ival = g_BigBuffer.index;          /* return the length */
  if (0 == g_BigBuffer.failed) {
    rpc->result = NACL_SRPC_RESULT_OK;
  } else {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  }
  done->Run(done);
}
NACL_SRPC_METHOD("CDOMTable_CreateTable:ii:i", CDOMTable_CreateTable);
