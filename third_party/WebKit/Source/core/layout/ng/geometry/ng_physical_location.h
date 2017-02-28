// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalLocation_h
#define NGPhysicalLocation_h

#include "core/CoreExport.h"

#include "platform/LayoutUnit.h"

namespace blink {

// NGPhysicalLocation is the position of a rect (typically a fragment) relative
// to the root document.
struct CORE_EXPORT NGPhysicalLocation {
  NGPhysicalLocation() {}
  NGPhysicalLocation(LayoutUnit left, LayoutUnit top) : left(left), top(top) {}
  LayoutUnit left;
  LayoutUnit top;

  bool operator==(const NGPhysicalLocation& other) const;

  String ToString() const;
};

CORE_EXPORT inline std::ostream& operator<<(std::ostream& os,
                                            const NGPhysicalLocation& value) {
  return os << value.ToString();
}

}  // namespace blink

#endif  // NGPhysicalLocation_h
