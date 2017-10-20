/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef LayoutTextControlMultiLine_h
#define LayoutTextControlMultiLine_h

#include "core/layout/LayoutTextControl.h"

namespace blink {

class HTMLTextAreaElement;

class LayoutTextControlMultiLine final : public LayoutTextControl {
 public:
  LayoutTextControlMultiLine(HTMLTextAreaElement*);
  ~LayoutTextControlMultiLine() override;

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTextArea || LayoutTextControl::IsOfType(type);
  }

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& location_in_container,
                   const LayoutPoint& accumulated_offset,
                   HitTestAction) override;

  float GetAvgCharWidth(const AtomicString& family) const override;
  LayoutUnit PreferredContentLogicalWidth(float char_width) const override;
  LayoutUnit ComputeControlLogicalHeight(
      LayoutUnit line_height,
      LayoutUnit non_content_height) const override;
  // We override the two baseline functions because we want our baseline to be
  // the bottom of our margin box.
  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override {
    return LayoutUnit(-1);
  }

  scoped_refptr<ComputedStyle> CreateInnerEditorStyle(
      const ComputedStyle& start_style) const override;
  LayoutObject* LayoutSpecialExcludedChild(bool relayout_children,
                                           SubtreeLayoutScope&) override;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTextControlMultiLine, IsTextArea());

}  // namespace blink

#endif
