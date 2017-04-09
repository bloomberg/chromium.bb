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

#ifndef LayoutListItem_h
#define LayoutListItem_h

#include "core/layout/LayoutBlockFlow.h"

namespace blink {

class HTMLOListElement;
class LayoutListMarker;

class LayoutListItem final : public LayoutBlockFlow {
 public:
  explicit LayoutListItem(Element*);

  int Value() const {
    if (!is_value_up_to_date_)
      UpdateValueNow();
    return value_;
  }
  void UpdateValue();

  bool HasExplicitValue() const { return has_explicit_value_; }
  int ExplicitValue() const { return explicit_value_; }
  void SetExplicitValue(int);
  void ClearExplicitValue();

  void SetNotInList(bool);
  bool NotInList() const { return not_in_list_; }

  const String& MarkerText() const;

  void UpdateListMarkerNumbers();

  static void UpdateItemValuesForOrderedList(const HTMLOListElement*);
  static unsigned ItemCountForOrderedList(const HTMLOListElement*);

  bool IsEmpty() const;

  LayoutListMarker* Marker() const { return marker_; }

  const char* GetName() const override { return "LayoutListItem"; }

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectListItem || LayoutBlockFlow::IsOfType(type);
  }

  void WillBeDestroyed() override;

  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;

  void Paint(const PaintInfo&, const LayoutPoint&) const override;

  void SubtreeDidChange() final;

  // Returns true if we re-attached and updated the location of the marker.
  bool UpdateMarkerLocation();

  void PositionListMarker();

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void AddOverflowFromChildren() override;

  inline int CalcValue() const;
  void UpdateValueNow() const;
  void ExplicitValueChanged();

  int explicit_value_;
  LayoutListMarker* marker_;
  mutable int value_;

  bool has_explicit_value_ : 1;
  mutable bool is_value_up_to_date_ : 1;
  bool not_in_list_ : 1;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutListItem, IsListItem());

}  // namespace blink

#endif  // LayoutListItem_h
