// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/c_include_iterator.h"

#include "base/logging.h"

namespace {

enum IncludeType {
  INCLUDE_NONE,
  INCLUDE_SYSTEM,  // #include <...>
  INCLUDE_USER     // #include "..."
};

// Returns true if str starts with the prefix.
bool StartsWith(const base::StringPiece& str, const base::StringPiece& prefix) {
  base::StringPiece extracted = str.substr(0, prefix.size());
  return extracted == prefix;
}

// Returns a new string piece referencing the same buffer as the argument, but
// with leading space trimmed. This only checks for space and tab characters
// since we're dealing with lines in C source files.
base::StringPiece TrimLeadingWhitespace(const base::StringPiece& str) {
  size_t new_begin = 0;
  while (new_begin < str.size() &&
         (str[new_begin] == ' ' || str[new_begin] == '\t'))
    new_begin++;
  return str.substr(new_begin);
}

// We don't want to count comment lines and preprocessor lines toward our
// "max lines to look at before giving up" since the beginnings of some files
// may have a lot of comments.
//
// We only handle C-style "//" comments since this is the normal commenting
// style used in Chrome, and do so pretty stupidly. We don't want to write a
// full C++ parser here, we're just trying to get a good heuristic for checking
// the file.
//
// We assume the line has leading whitespace trimmed. We also assume that empty
// lines have already been filtered out.
bool ShouldCountTowardNonIncludeLines(const base::StringPiece& line) {
  if (StartsWith(line, "//"))
    return false;  // Don't count comments.
  if (StartsWith(line, "#"))
    return false;  // Don't count preprocessor.
  return true;  // Count everything else.
}

// Given a line, checks to see if it looks like an include or import and
// extract the path. The type of include is returned. Returns INCLUDE_NONE on
// error or if this is not an include line.
IncludeType ExtractInclude(const base::StringPiece& line,
                           base::StringPiece* path) {
  static const char kInclude[] = "#include";
  static const size_t kIncludeLen = arraysize(kInclude) - 1;  // No null.
  static const char kImport[] = "#import";
  static const size_t kImportLen = arraysize(kImport) - 1;  // No null.

  base::StringPiece contents;
  if (StartsWith(line, base::StringPiece(kInclude, kIncludeLen)))
    contents = TrimLeadingWhitespace(line.substr(kIncludeLen));
  else if (StartsWith(line, base::StringPiece(kImport, kImportLen)))
    contents = TrimLeadingWhitespace(line.substr(kImportLen));

  if (contents.empty())
    return INCLUDE_NONE;

  IncludeType type = INCLUDE_NONE;
  char terminating_char = 0;
  if (contents[0] == '"') {
    type = INCLUDE_USER;
    terminating_char = '"';
  } else if (contents[0] == '<') {
    type = INCLUDE_SYSTEM;
    terminating_char = '>';
  } else {
    return INCLUDE_NONE;
  }

  // Count everything to next "/> as the contents.
  size_t terminator_index = contents.find(terminating_char, 1);
  if (terminator_index == base::StringPiece::npos)
    return INCLUDE_NONE;

  *path = contents.substr(1, terminator_index - 1);
  return type;
}

}  // namespace

const int CIncludeIterator::kMaxNonIncludeLines = 10;

CIncludeIterator::CIncludeIterator(const base::StringPiece& file)
    : file_(file),
      offset_(0),
      lines_since_last_include_(0) {
}

CIncludeIterator::~CIncludeIterator() {
}

bool CIncludeIterator::GetNextIncludeString(base::StringPiece* out) {
  base::StringPiece line;
  while (lines_since_last_include_ <= kMaxNonIncludeLines &&
         GetNextLine(&line)) {
    base::StringPiece trimmed = TrimLeadingWhitespace(line);
    if (trimmed.empty())
      continue;  // Just ignore all empty lines.

    base::StringPiece include_contents;
    IncludeType type = ExtractInclude(trimmed, &include_contents);
    if (type == INCLUDE_USER) {
      // Only count user includes for now.
      *out = include_contents;
      lines_since_last_include_ = 0;
      return true;
    }

    if (ShouldCountTowardNonIncludeLines(trimmed))
      lines_since_last_include_++;
  }
  return false;
}

bool CIncludeIterator::GetNextLine(base::StringPiece* line) {
  if (offset_ == file_.size())
    return false;

  size_t begin = offset_;
  while (offset_ < file_.size() && file_[offset_] != '\n')
    offset_++;

  *line = file_.substr(begin, offset_ - begin);

  // If we didn't hit EOF, skip past the newline for the next one.
  if (offset_ < file_.size())
    offset_++;
  return true;
}
