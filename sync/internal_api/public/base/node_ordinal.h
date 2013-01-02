// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_BASE_NODE_ORDINAL_H_
#define SYNC_INTERNAL_API_PUBLIC_BASE_NODE_ORDINAL_H_

#include "base/basictypes.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/ordinal.h"

namespace syncer {

// A NodeOrdinal is an Ordinal whose internal value comes from the
// ordinal_in_parent field of SyncEntity (see sync.proto).  It uses
// the entire uint8 range for backwards compatibility with the old
// int64-based positioning.

struct NodeOrdinalTraits {
  static const uint8 kZeroDigit = 0;
  static const uint8 kMaxDigit = kuint8max;
  static const size_t kMinLength = 8;
};

typedef Ordinal<NodeOrdinalTraits> NodeOrdinal;

COMPILE_ASSERT(static_cast<char>(NodeOrdinal::kZeroDigit) == '\x00',
               NodeOrdinalHasCorrectZeroDigit);
COMPILE_ASSERT(static_cast<char>(NodeOrdinal::kOneDigit) == '\x01',
               NodeOrdinalHasCorrectOneDigit);
COMPILE_ASSERT(static_cast<char>(NodeOrdinal::kMidDigit) == '\x80',
               NodeOrdinalHasCorrectMidDigit);
COMPILE_ASSERT(static_cast<char>(NodeOrdinal::kMaxDigit) == '\xff',
               NodeOrdinalHasCorrectMaxDigit);
COMPILE_ASSERT(NodeOrdinal::kMidDigitValue == 128,
               NodeOrdinalHasCorrectMidDigitValue);
COMPILE_ASSERT(NodeOrdinal::kMaxDigitValue == 255,
               NodeOrdinalHasCorrectMaxDigitValue);
COMPILE_ASSERT(NodeOrdinal::kRadix == 256,
               NodeOrdinalHasCorrectRadix);

// Converts an int64 position (usually from the position_in_parent
// field of SyncEntity) to a NodeOrdinal.  This transformation
// preserves the ordering relation: a < b under integer ordering if
// and only if Int64ToNodeOrdinal(a) < Int64ToNodeOrdinal(b).
SYNC_EXPORT_PRIVATE NodeOrdinal Int64ToNodeOrdinal(int64 x);

// The inverse of Int64ToNodeOrdinal.  This conversion is, in general,
// lossy: NodeOrdinals can have arbitrary fidelity, while numeric
// positions contain only 64 bits of information (in fact, this is the
// reason we've moved away from them).
SYNC_EXPORT_PRIVATE int64 NodeOrdinalToInt64(const NodeOrdinal& ordinal);

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_BASE_NODE_ORDINAL_H_
