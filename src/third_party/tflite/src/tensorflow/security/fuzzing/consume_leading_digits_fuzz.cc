/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <cstdlib>

#include "tensorflow/core/platform/str_util.h"
#include "tensorflow/core/platform/stringpiece.h"

// This is a fuzzer for tensorflow::str_util::ConsumeLeadingDigits

namespace {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider fuzzed_data(data, size);

  while (fuzzed_data.remaining_bytes() > 0) {
    std::string s = fuzzed_data.ConsumeRandomLengthString(25);
    tensorflow::StringPiece sp(s);
    tensorflow::uint64 val;

    const bool leading_digits =
        tensorflow::str_util::ConsumeLeadingDigits(&sp, &val);
    const char lead_char_consume_digits = *(sp.data());
    if (leading_digits) {
      if (lead_char_consume_digits >= '0') {
        assert(lead_char_consume_digits > '9');
      }
      assert(val >= 0);
    }
  }

  return 0;
}

}  // namespace
