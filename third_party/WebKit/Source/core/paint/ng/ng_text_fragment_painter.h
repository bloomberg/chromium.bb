// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ng_text_fragment_painter_h
#define ng_text_fragment_painter_h

#include "core/style/ComputedStyleConstants.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutPoint;
class Document;
class NGPaintFragment;
struct PaintInfo;

// Text fragment painter for LayoutNG. Operates on NGPhysicalTextFragments and
// handles clipping, selection, etc. Delegates to NGTextPainter to paint the
// text itself.
class NGTextFragmentPainter {
  STACK_ALLOCATED();

 public:
  explicit NGTextFragmentPainter(const NGPaintFragment&);

  void Paint(const Document&, const PaintInfo&, const LayoutPoint&);

 private:
  const NGPaintFragment& fragment_;
};

}  // namespace blink

#endif  // ng_text_fragment_painter_h
