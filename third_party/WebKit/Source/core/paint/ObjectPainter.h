// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPainter_h
#define ObjectPainter_h

#include "core/paint/ObjectPainterBase.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutPoint;
struct PaintInfo;
class LayoutObject;

class ObjectPainter : public ObjectPainterBase {
  STACK_ALLOCATED();

 public:
  ObjectPainter(const LayoutObject& layout_object)
      : layout_object_(layout_object) {}

  void PaintOutline(const PaintInfo&, const LayoutPoint& paint_offset);
  void PaintInlineChildrenOutlines(const PaintInfo&,
                                   const LayoutPoint& paint_offset);
  void AddPDFURLRectIfNeeded(const PaintInfo&, const LayoutPoint& paint_offset);

  // Paints the object atomically as if it created a new stacking context, for:
  // - inline blocks, inline tables, inline-level replaced elements (Section
  //   7.2.1.4 in http://www.w3.org/TR/CSS2/zindex.html#painting-order),
  // - non-positioned floating objects (Section 5 in
  //   http://www.w3.org/TR/CSS2/zindex.html#painting-order),
  // - flex items (http://www.w3.org/TR/css-flexbox-1/#painting),
  // - grid items (http://www.w3.org/TR/css-grid-1/#z-order),
  // - custom scrollbar parts.
  // Also see core/paint/README.md.
  //
  // It is expected that the caller will call this function independent of the
  // value of paintInfo.phase, and this function will do atomic paint (for
  // PaintPhaseForeground), normal paint (for PaintPhaseSelection and
  // PaintPhaseTextClip) or nothing (other paint phases) according to
  // paintInfo.phase.
  void PaintAllPhasesAtomically(const PaintInfo&,
                                const LayoutPoint& paint_offset);

  const LayoutObject& layout_object_;
};

}  // namespace blink

#endif
