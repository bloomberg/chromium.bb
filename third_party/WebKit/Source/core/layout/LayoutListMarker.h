/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009 Apple Inc.
 *               All rights reserved.
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

#ifndef LayoutListMarker_h
#define LayoutListMarker_h

#include "core/layout/LayoutBox.h"

namespace blink {

class LayoutListItem;

// Used to layout the list item's marker.
// The LayoutListMarker always has to be a child of a LayoutListItem.
class LayoutListMarker final : public LayoutBox {
 public:
  static LayoutListMarker* CreateAnonymous(LayoutListItem*);
  ~LayoutListMarker() override;

  const String& GetText() const { return text_; }

  // A reduced set of list style categories allowing for more concise expression
  // of list style specific logic.
  enum class ListStyleCategory { kNone, kSymbol, kLanguage };

  // Returns the list's style as one of a reduced high level categorical set of
  // styles.
  ListStyleCategory GetListStyleCategory() const;

  bool IsInside() const;

  void UpdateMarginsAndContent();

  IntRect GetRelativeMarkerRect() const;
  LayoutRect LocalSelectionRect() const final;
  bool IsImage() const override;
  const StyleImage* GetImage() const { return image_.Get(); }
  const LayoutListItem* ListItem() const { return list_item_; }
  LayoutSize ImageBulletSize() const;

  void ListItemStyleDidChange();

  const char* GetName() const override { return "LayoutListMarker"; }

  LayoutUnit LineOffset() const { return line_offset_; }

 protected:
  void WillBeDestroyed() override;

 private:
  LayoutListMarker(LayoutListItem*);

  void ComputePreferredLogicalWidths() override;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectListMarker || LayoutBox::IsOfType(type);
  }

  void Paint(const PaintInfo&, const LayoutPoint&) const override;

  void UpdateLayout() override;

  void ImageChanged(WrappedImagePtr, const IntRect* = nullptr) override;

  InlineBox* CreateInlineBox() override;

  LayoutUnit LineHeight(
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;

  bool IsText() const { return !IsImage(); }

  bool CanBeSelectionLeaf() const override { return true; }

  LayoutUnit GetWidthOfTextWithSuffix() const;
  void UpdateMargins();
  void UpdateContent();

  void StyleWillChange(StyleDifference,
                       const ComputedStyle& new_style) override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  bool AnonymousHasStylePropagationOverride() override { return true; }

  bool PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const override {
    return false;
  }

  String text_;
  Persistent<StyleImage> image_;
  LayoutListItem* list_item_;
  LayoutUnit line_offset_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutListMarker, IsListMarker());

}  // namespace blink

#endif  // LayoutListMarker_h
