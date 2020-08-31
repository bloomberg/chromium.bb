// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/accessibility_info_data_wrapper.h"

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"
#include "chrome/browser/chromeos/arc/accessibility/geometry_util.h"
#include "components/exo/wm_helper.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {

AccessibilityInfoDataWrapper::AccessibilityInfoDataWrapper(
    AXTreeSourceArc* tree_source)
    : tree_source_(tree_source) {}

void AccessibilityInfoDataWrapper::Serialize(ui::AXNodeData* out_data) const {
  out_data->id = GetId();
  PopulateAXRole(out_data);
  PopulateBounds(out_data);
}

void AccessibilityInfoDataWrapper::PopulateBounds(
    ui::AXNodeData* out_data) const {
  AccessibilityInfoDataWrapper* root = tree_source_->GetRoot();
  gfx::Rect info_data_bounds = GetBounds();
  gfx::RectF& out_bounds = out_data->relative_bounds.bounds;

  if (root && exo::WMHelper::HasInstance()) {
    if (tree_source_->is_notification() ||
        tree_source_->is_input_method_window() || root->GetId() != GetId()) {
      // By default, populate the bounds relative to the tree root.
      const gfx::Rect& root_bounds = root->GetBounds();

      info_data_bounds.Offset(-1 * root_bounds.x(), -1 * root_bounds.y());
      out_bounds = ToChromeScale(info_data_bounds);
      out_data->relative_bounds.offset_container_id = root->GetId();
    } else {
      // For the root node of application tree, populate the bounds to be
      // relative to its container View.
      views::Widget* widget = views::Widget::GetWidgetForNativeView(
          exo::WMHelper::GetInstance()->GetActiveWindow());
      DCHECK(widget);
      DCHECK(widget->widget_delegate());
      DCHECK(widget->widget_delegate()->GetContentsView());
      const gfx::Rect& root_bounds =
          widget->widget_delegate()->GetContentsView()->GetBoundsInScreen();

      out_bounds = ToChromeBounds(info_data_bounds, widget);
      out_bounds.Offset(-1 * root_bounds.x(), -1 * root_bounds.y());
    }

    // |out_bounds| is in Chrome DPI here. As ARC is considered the same as web
    // in Chrome automation, scale the bounds by device scale factor.
    out_bounds.Scale(tree_source_->device_scale_factor());
  } else {
    // We cannot compute global bounds, so use the raw bounds.
    out_bounds.SetRect(info_data_bounds.x(), info_data_bounds.y(),
                       info_data_bounds.width(), info_data_bounds.height());
  }
}

}  // namespace arc
