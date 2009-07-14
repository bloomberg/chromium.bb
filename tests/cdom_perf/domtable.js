// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DOMTable - a benchmark creating tables and accessing table elements
//
// This benchmark tests different mechanisms for creating an HTML table. By
// either creating the DOM elements individually or by creating an inner HTML
// as a string. The effect of forcing a layout is also measured.
// A second part of the benchmark sums the contents of all elements. Again
// in one set the benchmark is created using DOM functions from JavaScript,
// causing all nodes to be prewrapped, while in a second set the table is
// created using inner HTML which will wrap the elements at access.
//////////////////////////////////////////////////////////////////////
//
// This is an excerpt of the original code, for Native Client
// performance comparison.
//

// Size of the created tables.
var DOMTable_maxRows = 100;
var DOMTable_maxCols = 40;

// Helper variable to create consistent values for the table elements.
var DOMTable_element_count = 0;

// Functions needed to create a table by creating a big piece of HTML in a
// single string.
function DOMTable_CreateCellIH(row_id, col_id) {
  return '<td id="$' + row_id + '$' + col_id + '">'+
         DOMTable_element_count++ +
         '</td>';
}

function DOMTable_CreateRowIH(row_id, cols) {
  var html_string = '<tr>';
  for (var i = 0; i < cols; i++) {
    html_string += DOMTable_CreateCellIH(row_id, i);
  }
  return html_string + '</tr>';
}

function DOMTable_CreateTableIH(rows, cols) {
  var html_string = '<table>';
  for (var i = 0; i < rows; i++) {
    html_string += DOMTable_CreateRowIH(i, cols);
  }
  return html_string + '</table>';
}
