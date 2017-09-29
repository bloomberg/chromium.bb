// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_system_caret_win.h"

#include <windows.h>

#include "base/logging.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/ax_platform_unique_id.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {

AXSystemCaretWin::AXSystemCaretWin(gfx::AcceleratedWidget event_target)
    : event_target_(event_target) {
  caret_ = static_cast<AXPlatformNodeWin*>(AXPlatformNodeWin::Create(this));
  data_.id = GetNextAXPlatformNodeUniqueId();
  data_.role = AX_ROLE_CARET;
  // |get_accState| should return 0 which means that the caret is visible.
  data_.state = 0;
  // According to MSDN, "Edit" should be the name of the caret object.
  data_.SetName(L"Edit");
  data_.offset_container_id = -1;
}

AXSystemCaretWin::~AXSystemCaretWin() {
  caret_->Destroy();
  caret_ = nullptr;
}

base::win::ScopedComPtr<IAccessible> AXSystemCaretWin::GetCaret() const {
  base::win::ScopedComPtr<IAccessible> caret_accessible;
  HRESULT hr = caret_->QueryInterface(
      IID_IAccessible,
      reinterpret_cast<void**>(caret_accessible.GetAddressOf()));
  DCHECK(SUCCEEDED(hr));
  return caret_accessible;
}

void AXSystemCaretWin::MoveCaretTo(const gfx::Rect& bounds) {
  if (bounds.IsEmpty())
    return;
  data_.location = gfx::RectF(bounds);
  if (event_target_) {
    ::NotifyWinEvent(EVENT_OBJECT_LOCATIONCHANGE, event_target_, OBJID_CARET,
                     -data_.id);
  }
}

const AXNodeData& AXSystemCaretWin::GetData() const {
  return data_;
}

const AXTreeData& AXSystemCaretWin::GetTreeData() const {
  CR_DEFINE_STATIC_LOCAL(AXTreeData, empty_data, ());
  return empty_data;
}

gfx::NativeWindow AXSystemCaretWin::GetTopLevelWidget() {
  return nullptr;
}

gfx::NativeViewAccessible AXSystemCaretWin::GetParent() {
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

int AXSystemCaretWin::GetChildCount() {
  return 0;
}

gfx::NativeViewAccessible AXSystemCaretWin::ChildAtIndex(int index) {
  return nullptr;
}

gfx::Rect AXSystemCaretWin::GetScreenBoundsRect() const {
  gfx::Rect bounds = ToEnclosingRect(data_.location);
  return bounds;
}

gfx::NativeViewAccessible AXSystemCaretWin::HitTestSync(int x, int y) {
  return nullptr;
}

gfx::NativeViewAccessible AXSystemCaretWin::GetFocus() {
  return nullptr;
}

gfx::AcceleratedWidget
AXSystemCaretWin::GetTargetForNativeAccessibilityEvent() {
  return event_target_;
}

bool AXSystemCaretWin::AccessibilityPerformAction(const AXActionData& data) {
  return false;
}

int AXSystemCaretWin::GetIndexInParent() const {
  return -1;
}

AXPlatformNode* AXSystemCaretWin::GetFromNodeID(int32_t id) {
  return nullptr;
}

bool AXSystemCaretWin::ShouldIgnoreHoveredStateForTesting() {
  return false;
}

bool AXSystemCaretWin::IsOffscreen() const {
  return false;
}

}  // namespace ui
