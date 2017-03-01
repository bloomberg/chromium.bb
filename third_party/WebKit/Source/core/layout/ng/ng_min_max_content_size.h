// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MinMaxContentSize_h
#define MinMaxContentSize_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/LayoutUnit.h"

namespace blink {

struct CORE_EXPORT MinMaxContentSize {
  LayoutUnit min_content;
  LayoutUnit max_content;
  LayoutUnit ShrinkToFit(LayoutUnit available_size) const;
  bool operator==(const MinMaxContentSize& other) const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const MinMaxContentSize&);

}  // namespace blink

#endif  // MinMaxContentSize_h
