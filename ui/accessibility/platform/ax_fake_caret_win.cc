// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_fake_caret_win.h"

#include <windows.h>

#include "base/logging.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/ax_platform_unique_id.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {

AXFakeCaretWin::AXFakeCaretWin(gfx::AcceleratedWidget event_target)
    : event_target_(event_target) {
  caret_ = static_cast<AXPlatformNodeWin*>(AXPlatformNodeWin::Create(this));
  data_.id = GetNextAXPlatformNodeUniqueId();
  data_.role = AX_ROLE_CARET;
  data_.state = 0;
  data_.SetName(L"caret");
  data_.offset_container_id = -1;
}

AXFakeCaretWin::~AXFakeCaretWin() {
  caret_->Destroy();
  caret_ = nullptr;
}

base::win::ScopedComPtr<IAccessible> AXFakeCaretWin::GetCaret() const {
  base::win::ScopedComPtr<IAccessible> caret_accessible;
  HRESULT hr = caret_->QueryInterface(
      IID_IAccessible,
      reinterpret_cast<void**>(caret_accessible.GetAddressOf()));
  DCHECK(SUCCEEDED(hr));
  return caret_accessible;
}

void AXFakeCaretWin::MoveCaretTo(const gfx::Rect& bounds) {
  if (bounds.IsEmpty())
    return;
  data_.location = gfx::RectF(bounds);
  if (event_target_) {
    ::NotifyWinEvent(EVENT_OBJECT_LOCATIONCHANGE, event_target_, OBJID_CARET,
                     -data_.id);
  }
}

const AXNodeData& AXFakeCaretWin::GetData() const {
  return data_;
}

const ui::AXTreeData& AXFakeCaretWin::GetTreeData() const {
  CR_DEFINE_STATIC_LOCAL(ui::AXTreeData, empty_data, ());
  return empty_data;
}

gfx::NativeWindow AXFakeCaretWin::GetTopLevelWidget() {
  return nullptr;
}

gfx::NativeViewAccessible AXFakeCaretWin::GetParent() {
  if (!event_target_)
    return nullptr;

  gfx::NativeViewAccessible parent;
  HRESULT hr =
      ::AccessibleObjectFromWindow(event_target_, OBJID_WINDOW, IID_IAccessible,
                                   reinterpret_cast<void**>(&parent));
  if (SUCCEEDED(hr))
    return parent;
  return nullptr;
}

int AXFakeCaretWin::GetChildCount() {
  return 0;
}

gfx::NativeViewAccessible AXFakeCaretWin::ChildAtIndex(int index) {
  return nullptr;
}

gfx::Rect AXFakeCaretWin::GetScreenBoundsRect() const {
  gfx::Rect bounds = ToEnclosingRect(data_.location);
  return bounds;
}

gfx::NativeViewAccessible AXFakeCaretWin::HitTestSync(int x, int y) {
  return nullptr;
}

gfx::NativeViewAccessible AXFakeCaretWin::GetFocus() {
  return nullptr;
}

gfx::AcceleratedWidget AXFakeCaretWin::GetTargetForNativeAccessibilityEvent() {
  return event_target_;
}

bool AXFakeCaretWin::AccessibilityPerformAction(const ui::AXActionData& data) {
  return false;
}

}  // namespace ui
