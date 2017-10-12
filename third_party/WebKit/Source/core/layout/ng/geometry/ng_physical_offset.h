// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalOffset_h
#define NGPhysicalOffset_h

#include "core/CoreExport.h"

#include "platform/LayoutUnit.h"

namespace blink {

class LayoutPoint;

// NGPhysicalOffset is the position of a rect (typically a fragment) relative to
// its parent rect in the physical coordinate system.
struct CORE_EXPORT NGPhysicalOffset {
  NGPhysicalOffset() {}
  NGPhysicalOffset(LayoutUnit left, LayoutUnit top) : left(left), top(top) {}

  LayoutUnit left;
  LayoutUnit top;

  NGPhysicalOffset operator+(const NGPhysicalOffset& other) const;
  NGPhysicalOffset& operator+=(const NGPhysicalOffset& other);

  NGPhysicalOffset operator-(const NGPhysicalOffset& other) const;
  NGPhysicalOffset& operator-=(const NGPhysicalOffset& other);

  bool operator==(const NGPhysicalOffset& other) const;

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  explicit NGPhysicalOffset(const LayoutPoint&);

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGPhysicalOffset&);

}  // namespace blink

#endif  // NGPhysicalOffset_h
