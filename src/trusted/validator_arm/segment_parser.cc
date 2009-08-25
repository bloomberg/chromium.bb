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

// Parse test code segments for testing.

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "native_client/src/trusted/validator_arm/segment_parser.h"
#include "native_client/src/trusted/validator_arm/string_split.h"

SegmentParser::SegmentParser(std::istream* stream) {
  static std::string whitespace(" \t");
  // Start by processing the input file and collecting data.
  std::string input_line;
  bool is_first = true;
  uint32_t vbase = 0;  // Default assumption.
  std::vector<uint32_t> values;
  while (std::getline(*stream, input_line)) {
    // Tokenize the command line and process
    std::string line(input_line);
    // Start by removing trailing comments.
    std::string::size_type idx = line.find("#");
    if (idx != std::string::npos) {
      line = line.substr(0, idx);
    }

    // Pull out data
    std::vector<std::string> datum;
    SplitStringUsing(line, whitespace.c_str(), &datum);

    for (std::vector<std::string>::const_iterator iter = datum.begin();
         iter != datum.end();
         ++iter) {
      std::string data = *iter;
      char* end_ptr;
      unsigned long value = strtoul(data.c_str(), &end_ptr, 16);
      if (is_first) {
        is_first = false;
        if (':' == *end_ptr) {
          vbase = value;
          continue;
        }
      }
      // if reached, not start address.
      if (*end_ptr) {
        fprintf(stderr, "Error: Malformed value '%s' in '%s'\n",
                data.c_str(), input_line.c_str());
      } else {
        values.push_back(value);
      }
    }
  }
  // Now convert to internal representation
  const size_t number_bytes = values.size() * sizeof(uint32_t);
  uint8_t* mbase = new uint8_t[number_bytes];
  uint8_t* next = mbase;
  for (size_t i = 0; i < values.size(); ++i) {
    uint32_t value = values[i];
    memcpy(next, &value, sizeof(uint32_t));
    next += sizeof(uint32_t);
  }
  CodeSegmentInitialize(&code_segment_, mbase, vbase, number_bytes);
}

SegmentParser::~SegmentParser() {
  delete[] code_segment_.mbase;
}
