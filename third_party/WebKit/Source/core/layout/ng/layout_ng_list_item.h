// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef layout_ng_list_item_h
#define layout_ng_list_item_h

#include "core/CoreExport.h"
#include "core/html/ListItemOrdinal.h"
#include "core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// A LayoutObject subclass for 'display: list-item' in LayoutNG.
class CORE_EXPORT LayoutNGListItem final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGListItem(Element*);

  ListItemOrdinal& Ordinal() { return ordinal_; }

  int Value() const;
  String MarkerTextWithoutSuffix() const;

  void OrdinalValueChanged();

  // Returns whether the LayoutObject is a list marker or not.
  static bool IsListMarker(LayoutObject*);

  const char* GetName() const override { return "LayoutNGListItem"; }

 private:
  bool IsOfType(LayoutObjectType) const override;

  void WillBeDestroyed() override;
  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  bool IsInside() const;

  enum MarkerTextFormat { kWithSuffix, kWithoutSuffix };
  void MarkerText(StringBuilder*, MarkerTextFormat) const;
  void UpdateMarkerText(LayoutText*);
  void UpdateMarker();
  void DestroyMarker();

  ListItemOrdinal ordinal_;
  LayoutObject* marker_ = nullptr;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGListItem, IsLayoutNGListItem());

}  // namespace blink

#endif  // layout_ng_list_item_h
