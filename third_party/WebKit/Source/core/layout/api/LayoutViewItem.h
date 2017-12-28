// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutViewItem_h
#define LayoutViewItem_h

#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutViewItem : public LayoutBlockItem {
 public:
  explicit LayoutViewItem(LayoutView* layout_view)
      : LayoutBlockItem(layout_view) {}

  explicit LayoutViewItem(const LayoutBlockItem& item) : LayoutBlockItem(item) {
    SECURITY_DCHECK(!item || item.IsLayoutView());
  }

  explicit LayoutViewItem(std::nullptr_t) : LayoutBlockItem(nullptr) {}

  LayoutViewItem() {}

  bool HitTest(HitTestResult& result) { return ToView()->HitTest(result); }

  bool HitTestNoLifecycleUpdate(HitTestResult& result) {
    return ToView()->HitTestNoLifecycleUpdate(result);
  }

  unsigned HitTestCount() const { return ToView()->HitTestCount(); }

  unsigned HitTestCacheHits() const { return ToView()->HitTestCacheHits(); }

  void ClearHitTestCache() { ToView()->ClearHitTestCache(); }

  void UpdateCounters() { ToView()->UpdateCounters(); }

 private:
  LayoutView* ToView() { return ToLayoutView(GetLayoutObject()); }
  const LayoutView* ToView() const { return ToLayoutView(GetLayoutObject()); }
};

inline LayoutViewItem LayoutItem::View() const {
  return LayoutViewItem(layout_object_->View());
}

}  // namespace blink

#endif  // LayoutViewItem_h
