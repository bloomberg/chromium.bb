// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/node_ordinal.h"

#include <algorithm>

namespace syncer {

NodeOrdinal Int64ToNodeOrdinal(int64 x) {
  uint64 y = static_cast<uint64>(x);
  y ^= 0x8000000000000000ULL;
  std::string bytes(NodeOrdinal::kMinLength, '\x00');
  if (y == 0) {
    // 0 is a special case since |bytes| must not be all zeros.
    bytes.push_back('\x80');
  } else {
    for (int i = 7; i >= 0; --i) {
      bytes[i] = static_cast<uint8>(y);
      y >>= 8;
    }
  }
  NodeOrdinal ordinal(bytes);
  DCHECK(ordinal.IsValid());
  return ordinal;
}

int64 NodeOrdinalToInt64(const NodeOrdinal& ordinal) {
  uint64 y = 0;
  const std::string& s = ordinal.ToInternalValue();
  size_t l = NodeOrdinal::kMinLength;
  if (s.length() < l) {
    NOTREACHED();
    l = s.length();
  }
  for (size_t i = 0; i < l; ++i) {
    const uint8 byte = s[l - i - 1];
    y |= static_cast<uint64>(byte) << (i * 8);
  }
  y ^= 0x8000000000000000ULL;
  // This is technically implementation-defined if y > INT64_MAX, so
  // we're assuming that we're on a twos-complement machine.
  return static_cast<int64>(y);
}

}  // namespace syncer
