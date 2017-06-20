// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutViewItem_h
#define LayoutViewItem_h

#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class PaintLayerCompositor;

class LayoutViewItem : public LayoutBlockItem {
 public:
  explicit LayoutViewItem(LayoutView* layout_view)
      : LayoutBlockItem(layout_view) {}

  explicit LayoutViewItem(const LayoutBlockItem& item) : LayoutBlockItem(item) {
    SECURITY_DCHECK(!item || item.IsLayoutView());
  }

  explicit LayoutViewItem(std::nullptr_t) : LayoutBlockItem(nullptr) {}

  LayoutViewItem() {}

  bool UsesCompositing() const { return ToView()->UsesCompositing(); }

  PaintLayerCompositor* Compositor() { return ToView()->Compositor(); }

  IntRect DocumentRect() const { return ToView()->DocumentRect(); }

  LayoutRect ViewRect() const { return ToView()->ViewRect(); }

  IntSize GetLayoutSize(
      IncludeScrollbarsInRect scrollbars = kExcludeScrollbars) const {
    return ToView()->GetLayoutSize(scrollbars);
  }

  LayoutRect OverflowClipRect(const LayoutPoint& location) const {
    return ToView()->OverflowClipRect(location);
  }

  bool HitTest(HitTestResult& result) { return ToView()->HitTest(result); }

  bool HitTestNoLifecycleUpdate(HitTestResult& result) {
    return ToView()->HitTestNoLifecycleUpdate(result);
  }

  //    bool hitTest(HitTestResult&);
  //    bool hitTestNoLifecycleUpdate(HitTestResult&);

  unsigned HitTestCount() const { return ToView()->HitTestCount(); }

  unsigned HitTestCacheHits() const { return ToView()->HitTestCacheHits(); }

  void ClearHitTestCache() { ToView()->ClearHitTestCache(); }

  void InvalidatePaintForViewAndCompositedLayers() {
    ToView()->InvalidatePaintForViewAndCompositedLayers();
  }

  int ViewHeight(
      IncludeScrollbarsInRect scrollbar_inclusion = kExcludeScrollbars) const {
    return ToView()->ViewHeight(scrollbar_inclusion);
  }

  int ViewWidth(
      IncludeScrollbarsInRect scrollbar_inclusion = kExcludeScrollbars) const {
    return ToView()->ViewWidth(scrollbar_inclusion);
  }

  FloatSize ViewportSizeForViewportUnits() const {
    return ToView()->ViewportSizeForViewportUnits();
  }

 private:
  LayoutView* ToView() { return ToLayoutView(GetLayoutObject()); }
  const LayoutView* ToView() const { return ToLayoutView(GetLayoutObject()); }
};

inline LayoutViewItem LayoutItem::View() const {
  return LayoutViewItem(layout_object_->View());
}

}  // namespace blink

#endif  // LayoutViewItem_h
