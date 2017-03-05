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
  LayoutUnit margin;
  LayoutUnit negative_margin;

  // Appends negative or positive value to the current margin strut.
  void Append(const LayoutUnit& value);

  // Sum up negative and positive margins of this strut.
  LayoutUnit Sum() const;

  bool operator==(const NGMarginStrut& other) const;

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGMarginStrut&);

}  // namespace blink

#endif  // NGMarginStrut_h
