// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/stringprintf.h"

#include <iomanip>
#include <sstream>

namespace openscreen {

std::string HexEncode(absl::Span<const uint8_t> bytes) {
  std::ostringstream hex_dump;
  hex_dump << std::setfill('0') << std::hex;
  for (const uint8_t byte : bytes) {
    hex_dump << std::setw(2) << static_cast<int>(byte);
  }
  return hex_dump.str();
}

}  // namespace openscreen
