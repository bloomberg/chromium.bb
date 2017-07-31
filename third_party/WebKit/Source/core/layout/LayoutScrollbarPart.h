/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LayoutScrollbarPart_h
#define LayoutScrollbarPart_h

#include "core/layout/LayoutBlock.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class LayoutScrollbar;
class ScrollableArea;

class LayoutScrollbarPart final : public LayoutBlock {
 public:
  static LayoutScrollbarPart* CreateAnonymous(Document*,
                                              ScrollableArea*,
                                              LayoutScrollbar* = nullptr,
                                              ScrollbarPart = kNoPart);

  const char* GetName() const override { return "LayoutScrollbarPart"; }

  PaintLayerType LayerTypeRequired() const override { return kNoPaintLayer; }

  void UpdateLayout() override;

  // Scrollbar parts needs to be rendered at device pixel boundaries.
  LayoutRectOutsets MarginBoxOutsets() const override {
    DCHECK(IsIntegerValue(LayoutBlock::MarginBoxOutsets().Top()));
    return LayoutBlock::MarginBoxOutsets();
  }
  LayoutUnit MarginTop() const override {
    DCHECK(IsIntegerValue(LayoutBlock::MarginTop()));
    return LayoutBlock::MarginTop();
  }
  LayoutUnit MarginBottom() const override {
    DCHECK(IsIntegerValue(LayoutBlock::MarginBottom()));
    return LayoutBlock::MarginBottom();
  }
  LayoutUnit MarginLeft() const override {
    DCHECK(IsIntegerValue(LayoutBlock::MarginLeft()));
    return LayoutBlock::MarginLeft();
  }
  LayoutUnit MarginRight() const override {
    DCHECK(IsIntegerValue(LayoutBlock::MarginRight()));
    return LayoutBlock::MarginRight();
  }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutScrollbarPart ||
           LayoutBlock::IsOfType(type);
  }
  LayoutObject* ScrollbarStyleSource() const;

  // Must call setStyleWithWritingModeOfParent() instead.
  void SetStyle(RefPtr<ComputedStyle>) = delete;

  // Expose for LayoutScrollbar and PaintInvalidationCapableScrollableArea for
  // paint invalidation.
  using LayoutObject::SetVisualRect;

 protected:
  void StyleWillChange(StyleDifference,
                       const ComputedStyle& new_style) override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void ImageChanged(WrappedImagePtr, const IntRect* = nullptr) override;

 private:
  LayoutScrollbarPart(ScrollableArea*, LayoutScrollbar*, ScrollbarPart);

  void ComputePreferredLogicalWidths() override;

  // Have all padding getters return 0. The important point here is to avoid
  // resolving percents against the containing block, since scroll bar corners
  // don't always have one (so it would crash). Scroll bar corners are not
  // actually laid out, and they don't have child content, so what we return
  // here doesn't really matter.
  LayoutUnit PaddingTop() const override { return LayoutUnit(); }
  LayoutUnit PaddingBottom() const override { return LayoutUnit(); }
  LayoutUnit PaddingLeft() const override { return LayoutUnit(); }
  LayoutUnit PaddingRight() const override { return LayoutUnit(); }
  LayoutUnit PaddingBefore() const override { return LayoutUnit(); }
  LayoutUnit PaddingAfter() const override { return LayoutUnit(); }
  LayoutUnit PaddingStart() const override { return LayoutUnit(); }
  LayoutUnit PaddingEnd() const override { return LayoutUnit(); }

  void LayoutHorizontalPart();
  void LayoutVerticalPart();

  void ComputeScrollbarWidth();
  void ComputeScrollbarHeight();

  void SetNeedsPaintInvalidation();

  bool AllowsOverflowClip() const override { return false; }

  UntracedMember<ScrollableArea> scrollable_area_;
  UntracedMember<LayoutScrollbar> scrollbar_;
  ScrollbarPart part_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutScrollbarPart, IsLayoutScrollbarPart());

}  // namespace blink

#endif  // LayoutScrollbarPart_h
