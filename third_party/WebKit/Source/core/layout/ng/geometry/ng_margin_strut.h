// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGMarginStrut_h
#define NGMarginStrut_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"

namespace blink {

// This struct is used for the margin collapsing calculation.
struct CORE_EXPORT NGMarginStrut {
  LayoutUnit positive_margin;
  LayoutUnit negative_margin;

  // Store quirky margins separately, quirky containers need to ignore
  // quirky end margins.  Quirky margins are always default margins,
  // which are always positive.
  LayoutUnit quirky_positive_margin;

  // If this flag is set, we only Append non-quirky margins to this strut.
  // See comment inside NGBlockLayoutAlgorithm for when this occurs.
  bool is_quirky_container_start = false;

  // Appends negative or positive value to the current margin strut.
  void Append(const LayoutUnit& value, bool is_quirky);

  // Sum up negative and positive margins of this strut.
  LayoutUnit Sum() const;

  // Sum up non-quirky margins of this strut, used by quirky
  // containers to sum up the last margin.
  LayoutUnit QuirkyContainerSum() const;

  bool operator==(const NGMarginStrut& other) const;
  bool operator!=(const NGMarginStrut& other) const {
    return !(*this == other);
  }
};

}  // namespace blink

#endif  // NGMarginStrut_h
