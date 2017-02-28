// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalSize_h
#define NGPhysicalSize_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/LayoutUnit.h"

namespace blink {

struct NGLogicalSize;

// NGPhysicalSize is the size of a rect (typically a fragment) in the physical
// coordinate system.
struct CORE_EXPORT NGPhysicalSize {
  NGPhysicalSize() {}
  NGPhysicalSize(LayoutUnit width, LayoutUnit height)
      : width(width), height(height) {}

  LayoutUnit width;
  LayoutUnit height;

  NGLogicalSize ConvertToLogical(NGWritingMode mode) const;

  bool operator==(const NGPhysicalSize& other) const;

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGPhysicalSize&);

}  // namespace blink

#endif  // NGPhysicalSize_h
