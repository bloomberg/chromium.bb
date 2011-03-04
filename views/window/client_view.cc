// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "views/window/client_view.h"
#if defined(OS_LINUX)
#include "views/window/hit_test.h"
#endif
#include "views/window/window.h"
#include "views/window/window_delegate.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
// ClientView, public:

ClientView::ClientView(Window* window, View* contents_view)
    : window_(window),
      contents_view_(contents_view) {
}

int ClientView::NonClientHitTest(const gfx::Point& point) {
  return bounds().Contains(point) ? HTCLIENT : HTNOWHERE;
}

void ClientView::WindowClosing() {
  window_->GetDelegate()->WindowClosing();
}

///////////////////////////////////////////////////////////////////////////////
// ClientView, View overrides:

gfx::Size ClientView::GetPreferredSize() {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a Container.
  if (contents_view_)
    return contents_view_->GetPreferredSize();
  return gfx::Size();
}

void ClientView::Layout() {
  // |contents_view_| is allowed to be NULL up until the point where this view
  // is attached to a Container.
  if (contents_view_)
    contents_view_->SetBounds(0, 0, width(), height());
}

void ClientView::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && child == this) {
    DCHECK(GetWidget());
    DCHECK(contents_view_); // |contents_view_| must be valid now!
    // Insert |contents_view_| at index 0 so it is first in the focus chain.
    // (the OK/Cancel buttons are inserted before contents_view_)
    AddChildViewAt(contents_view_, 0);
  }
}

void ClientView::OnBoundsChanged() {
  // Overridden to do nothing. The NonClientView manually calls Layout on the
  // ClientView when it is itself laid out, see comment in
  // NonClientView::Layout.
}

AccessibilityTypes::Role ClientView::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_CLIENT;
}

}  // namespace views
