// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/client_view.h"

#include <memory>

#include "base/check.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/hit_test.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
// ClientView, public:

ClientView::ClientView(Widget* widget, View* contents_view)
    : contents_view_(contents_view) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

int ClientView::NonClientHitTest(const gfx::Point& point) {
  return bounds().Contains(point) ? HTCLIENT : HTNOWHERE;
}

bool ClientView::CanClose() {
  return true;
}

void ClientView::WidgetClosing() {}

///////////////////////////////////////////////////////////////////////////////
// ClientView, View overrides:

gfx::Size ClientView::CalculatePreferredSize() const {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a Container.
  return contents_view_ ? contents_view_->GetPreferredSize() : gfx::Size();
}

gfx::Size ClientView::GetMaximumSize() const {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a Container.
  return contents_view_ ? contents_view_->GetMaximumSize() : gfx::Size();
}

gfx::Size ClientView::GetMinimumSize() const {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a Container.
  return contents_view_ ? contents_view_->GetMinimumSize() : gfx::Size();
}

void ClientView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kClient;
}

void ClientView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Overridden to do nothing. The NonClientView manually calls Layout on the
  // ClientView when it is itself laid out, see comment in
  // NonClientView::Layout.
}

void ClientView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    DCHECK(GetWidget());
    DCHECK(contents_view_);  // |contents_view_| must be valid now!
    // Insert |contents_view_| at index 0 so it is first in the focus chain.
    // (the OK/Cancel buttons are inserted before contents_view_)
    // TODO(weili): This seems fragile and can be refactored.
    // Tracked at https://crbug.com/1012466.
    AddChildViewAt(contents_view_, 0);
  } else if (!details.is_add && details.child == contents_view_) {
    contents_view_ = nullptr;
  }
}

BEGIN_METADATA(ClientView)
METADATA_PARENT_CLASS(View)
END_METADATA()

}  // namespace views
