/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayoutTextControlSingleLine_h
#define LayoutTextControlSingleLine_h

#include "core/html/forms/HTMLInputElement.h"
#include "core/layout/LayoutTextControl.h"

namespace blink {

class HTMLInputElement;

class LayoutTextControlSingleLine : public LayoutTextControl {
 public:
  LayoutTextControlSingleLine(HTMLInputElement*);
  ~LayoutTextControlSingleLine() override;
  // FIXME: Move createInnerEditorStyle() to TextControlInnerEditorElement.
  scoped_refptr<ComputedStyle> CreateInnerEditorStyle(
      const ComputedStyle& start_style) const final;

  void CapsLockStateMayHaveChanged();

 protected:
  Element* ContainerElement() const;
  Element* EditingViewPortElement() const;
  HTMLInputElement* InputElement() const;

 private:
  bool HasControlClip() const final;
  LayoutRect ControlClipRect(const LayoutPoint&) const final;
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTextField || LayoutTextControl::IsOfType(type);
  }

  void Paint(const PaintInfo&, const LayoutPoint&) const override;
  void UpdateLayout() override;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& location_in_container,
                   const LayoutPoint& accumulated_offset,
                   HitTestAction) final;

  void Autoscroll(const IntPoint&) final;

  // Subclassed to forward to our inner div.
  LayoutUnit ScrollLeft() const final;
  LayoutUnit ScrollTop() const final;
  LayoutUnit ScrollWidth() const final;
  LayoutUnit ScrollHeight() const final;
  void SetScrollLeft(LayoutUnit) final;
  void SetScrollTop(LayoutUnit) final;

  int TextBlockWidth() const;
  float GetAvgCharWidth(const AtomicString& family) const final;
  LayoutUnit PreferredContentLogicalWidth(float char_width) const final;
  LayoutUnit ComputeControlLogicalHeight(
      LayoutUnit line_height,
      LayoutUnit non_content_height) const override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) final;
  void AddOverflowFromChildren() final;

  bool AllowsOverflowClip() const override { return false; }

  bool TextShouldBeTruncated() const;
  HTMLElement* InnerSpinButtonElement() const;

  bool should_draw_caps_lock_indicator_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTextControlSingleLine, IsTextField());

// ----------------------------

class LayoutTextControlInnerEditor : public LayoutBlockFlow {
 public:
  LayoutTextControlInnerEditor(Element* element) : LayoutBlockFlow(element) {}
  bool ShouldIgnoreOverflowPropertyForInlineBlockBaseline() const override {
    return true;
  }

 private:
  bool IsIntrinsicallyScrollable(
      ScrollbarOrientation orientation) const override {
    return orientation == kHorizontalScrollbar;
  }
  bool ScrollsOverflowX() const override { return HasOverflowClip(); }
  bool ScrollsOverflowY() const override { return false; }
  bool HasLineIfEmpty() const override { return true; }
};

}  // namespace blink

#endif
