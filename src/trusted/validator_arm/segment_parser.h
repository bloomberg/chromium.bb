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
