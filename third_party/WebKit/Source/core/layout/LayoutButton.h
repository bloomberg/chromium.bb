/*
 * Copyright (C) 2005 Apple Computer
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

#ifndef LayoutButton_h
#define LayoutButton_h

#include "core/editing/EditingUtilities.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/layout/LayoutFlexibleBox.h"

namespace blink {

// LayoutButtons are just like normal flexboxes except that they will generate
// an anonymous block child.
// For inputs, they will also generate an anonymous LayoutText and keep its
// style and content up to date as the button changes.
class LayoutButton final : public LayoutFlexibleBox {
 public:
  explicit LayoutButton(Element*);
  ~LayoutButton() override;

  const char* GetName() const override { return "LayoutButton"; }
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectLayoutButton ||
           LayoutFlexibleBox::IsOfType(type);
  }

  bool CanBeSelectionLeaf() const override {
    return GetNode() && HasEditableStyle(*GetNode());
  }

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;
  void RemoveLeftoverAnonymousBlock(LayoutBlock*) override {}
  bool CreatesAnonymousWrapper() const override { return true; }

  bool HasControlClip() const override;
  LayoutRect ControlClipRect(const LayoutPoint&) const override;

  LayoutUnit BaselinePosition(FontBaseline,
                              bool first_line,
                              LineDirectionMode,
                              LinePositionMode) const override;

 private:
  void UpdateAnonymousChildStyle(const LayoutObject& child,
                                 ComputedStyle& child_style) const override;

  bool HasLineIfEmpty() const override { return IsHTMLInputElement(GetNode()); }

  LayoutBlock* inner_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutButton, IsLayoutButton());

}  // namespace blink

#endif  // LayoutButton_h
