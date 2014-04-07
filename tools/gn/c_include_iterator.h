// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_C_INCLUDE_ITERATOR_H_
#define TOOLS_GN_C_INCLUDE_ITERATOR_H_

#include "base/basictypes.h"
#include "base/strings/string_piece.h"

// Iterates through #includes in C source and header files.
//
// This only returns includes we want to check, which is user includes with
// double-quotes: #include "..."
class CIncludeIterator {
 public:
  // The buffer pointed to must outlive this class.
  CIncludeIterator(const base::StringPiece& file);
  ~CIncludeIterator();

  // Fills in the string with the contents of the next include and returns
  // true, or returns false if there are no more includes.
  bool GetNextIncludeString(base::StringPiece* out);

  // Maximum numbef of non-includes we'll tolerate before giving up. This does
  // not count comments or preprocessor.
  static const int kMaxNonIncludeLines;

 private:
  // Returns false on EOF, otherwise fills in the given line.
  bool GetNextLine(base::StringPiece* line);

  base::StringPiece file_;

  size_t offset_;

  // Number of lines we've processed since seeing the last include (or the
  // beginning of the file) with some exceptions.
  int lines_since_last_include_;

  DISALLOW_COPY_AND_ASSIGN(CIncludeIterator);
};

#endif  // TOOLS_GN_INCLUDE_ITERATOR_H_
