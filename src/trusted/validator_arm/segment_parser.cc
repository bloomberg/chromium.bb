/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Parse test code segments for testing.

#include <stdlib.h>
#include <string.h>

#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/validator_arm/segment_parser.h"
#include "native_client/src/trusted/validator_arm/string_split.h"

SegmentParser::SegmentParser(std::istream* stream)
    : code_segment_() {
  static nacl::string whitespace(" \t");
  // Start by processing the input file and collecting data.
  nacl::string input_line;
  bool is_first = true;
  uint32_t vbase = 0;  // Default assumption.
  std::vector<uint32_t> values;
  while (std::getline(*stream, input_line)) {
    // Tokenize the command line and process
    nacl::string line(input_line);
    // Start by removing trailing comments.
    nacl::string::size_type idx = line.find("#");
    if (idx != nacl::string::npos) {
      line = line.substr(0, idx);
    }

    // Pull out data
    std::vector<nacl::string> datum;
    SplitStringUsing(line, whitespace.c_str(), &datum);

    for (std::vector<nacl::string>::const_iterator iter = datum.begin();
         iter != datum.end();
         ++iter) {
      nacl::string data = *iter;
      char* end_ptr;
      uint32_t value = strtoul(data.c_str(), &end_ptr, 16);
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
