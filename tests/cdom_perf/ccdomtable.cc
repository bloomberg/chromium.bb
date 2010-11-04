/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
/* ccdomtable.cc: Generate HTML for an HTML table.
 * These CC functions are modeled after JavaScript versions provided
 * as comments.
 */

#include <string>

#include <nacl/nacl_srpc.h>
#include "native_client/tests/cdom_perf/cdomshm.h"

// Helper variable to create consistent values for the table elements.
static int g_DOMTable_element_count = 0;

// Functions needed to create a table by creating a big piece of HTML in a
// single string.
// function DOMTable_CreateCellIH(row_id, col_id) {
//  return '<td id="$' + row_id + '$' + col_id + '">'+
//         DOMTable_element_count++ +
//         '</td>';
// }
static inline std::string DOMTable_CreateCellIH(const int row_id,
                                                const int col_id) {
  /* This format string can produce at most 18 + 8 + 8 + 8 == 42 bytes */
  const char *kFmtString = "<td id=\"$%d$%d\">%d</td>";
  const int kMaxStringLength = 64;
  char s[kMaxStringLength];

  snprintf(&s[0], kMaxStringLength, kFmtString,
           row_id, col_id, g_DOMTable_element_count++);
  return std::string(s);
}

// function DOMTable_CreateRowIH(row_id, cols) {
//   var html_string = '<tr>';
//   for (var i = 0; i < cols; i++) {
//     html_string += DOMTable_CreateCellIH(row_id, i);
//   }
//   return html_string + '</tr>';
// }
static std::string DOMTable_CreateRowIH(const int row_id, const int cols) {
  std::string html("<tr>");

  for (int i = 0; i < cols; i++) {
    html += DOMTable_CreateCellIH(row_id, i);
  }
  html += "</tr>";
  return html;
}

// function DOMTable_CreateTableIH(rows, cols) {
//   var html_string = '<table>';
//   for (var i = 0; i < rows; i++) {
//     html_string += DOMTable_CreateRowIH(i, cols);
//   }
//   return html_string + '</table>';
// }
static std::string DOMTable_CreateTableIH(int rows, int cols) {
  std::string html_string("<table>");

  for (int i = 0; i < rows; i++) {
    html_string += DOMTable_CreateRowIH(i, cols);
  }
  html_string += "</table>";
  return html_string;
}

int CCDOMTable_CreateTable(NaClSrpcChannel *channel,
                           NaClSrpcArg **in_args,
                           NaClSrpcArg **out_args) {
  /*
   * Strdup must be used because the SRPC layer frees the string passed to it.
   */
  int rows = in_args[0]->u.ival;
  int cols = in_args[1]->u.ival;

  g_BigBuffer.index = 0;                      /* reset BigBuffer_index */
  g_DOMTable_element_count = 0;               /* reset element count */
  std::string html = DOMTable_CreateTableIH(rows, cols);
  BBWrite(html.c_str(), &g_BigBuffer);
  out_args[0]->u.ival = g_BigBuffer.index;
  if (0 == g_BigBuffer.failed) {
    return NACL_SRPC_RESULT_OK;
  } else {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
}
NACL_SRPC_METHOD("CCDOMTable_CreateTable:ii:i", CCDOMTable_CreateTable);
