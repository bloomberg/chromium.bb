/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Parses ascii text file describing test code segments, and
// generates the corresponding code segment.

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_SEGMENT_PARSER_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_SEGMENT_PARSER_H__

#include <istream>

#include "native_client/src/trusted/validator_arm/ncdecode.h"

// This class takes an input stream (containing ascii text
// describing a code segment), and generates the corresponding
// code segment.
//
// The code segment is a sequence of (8 digit) hexidecimal
// numbers, describing the contents of each (4 byte) word.
// Optionally, the first hexidecimal number can be suffixed
// with a colin (':'), and is used to denote the starting
// offset (instead of code segment contents). If the special
// (optional) starting offset is omitted, zero is assumed.
//
// For readability, whitespace can be added everywhere, except
// for the start address which must be immediately suffixed with
// a colin. In addition, line comments can be added, and are
// defined by the appearance of the '#" character. All text
// after the initial '#' character (on a line) is treated as
// a comment.
class SegmentParser {
 public:
  // Create a code segment from the given stream.
  explicit SegmentParser(std::istream* stream);

  ~SegmentParser();

  // Returns a copy of the code segment parsed from the stream,
  // into the given segment.
  // Note: The lifetime of data within the code segment is
  // the same as this.
  void Initialize(CodeSegment* segment) {
    CodeSegmentCopy(&code_segment_, segment);
  }
 private:
  CodeSegment code_segment_;
};

#endif  // NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_SEGMENT_PARSER_H__
