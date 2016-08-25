// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGMarginStrut_h
#define NGMarginStrut_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"

namespace blink {

// Stores the four margins of a box
struct NGBoxMargins {
  LayoutUnit inline_start;
  LayoutUnit inline_end;
  LayoutUnit block_start;
  LayoutUnit block_end;

  LayoutUnit InlineSum() const { return inline_start + inline_end; }
  LayoutUnit BlockSum() const { return block_start + block_end; }
};

// This struct is used for the margin collapsing calculation.
struct NGMarginStrut {
  LayoutUnit margin_block_start;
  LayoutUnit margin_block_end;

  LayoutUnit negative_margin_block_start;
  LayoutUnit negative_margin_block_end;
};

}  // namespace blink

#endif  // NGMarginStrut_h
