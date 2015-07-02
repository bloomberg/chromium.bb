// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/widget.h"

#if defined(USE_X11)
#include <X11/Xutil.h>
#include "ui/gfx/x/x11_types.h"
#endif

namespace views {
namespace test {

namespace {

// Perform a pre-order traversal of |children| and all descendants, looking for
// |first| and |second|. If |first| is found before |second|, return true.
// When a layer is found, it is set to null. Returns once |second| is found, or
// when there are no children left.
// Note that ui::Layer children are bottom-to-top stacking order.
bool FindLayersInOrder(const std::vector<ui::Layer*>& children,
                       const ui::Layer** first,
                       const ui::Layer** second) {
  for (const ui::Layer* child : children) {
    if (child == *second) {
      *second = nullptr;
      return *first == nullptr;
    }

    if (child == *first)
      *first = nullptr;

    if (FindLayersInOrder(child->children(), first, second))
      return true;

    // If second is cleared without success, exit early with failure.
    if (!*second)
      return false;
  }
  return false;
}

}  // namespace

// static
void WidgetTest::SimulateNativeDestroy(Widget* widget) {
  delete widget->GetNativeView();
}

// static
void WidgetTest::SimulateNativeActivate(Widget* widget) {
  gfx::NativeView native_view = widget->GetNativeView();
  aura::client::GetFocusClient(native_view)->FocusWindow(native_view);
}

// static
bool WidgetTest::IsNativeWindowVisible(gfx::NativeWindow window) {
  return window->IsVisible();
}

// static
bool WidgetTest::IsWindowStackedAbove(Widget* above, Widget* below) {
  EXPECT_TRUE(above->IsVisible());
  EXPECT_TRUE(below->IsVisible());

  ui::Layer* root_layer = above->GetNativeWindow()->GetRootWindow()->layer();

  // Traversal is bottom-to-top, so |below| should be found first.
  const ui::Layer* first = below->GetLayer();
  const ui::Layer* second = above->GetLayer();
  return FindLayersInOrder(root_layer->children(), &first, &second);
}

// static
gfx::Size WidgetTest::GetNativeWidgetMinimumContentSize(Widget* widget) {
  // On Windows, HWNDMessageHandler receives a WM_GETMINMAXINFO message whenever
  // the window manager is interested in knowing the size constraints. On
  // ChromeOS, it's handled internally. Elsewhere, the size constraints need to
  // be pushed to the window server when they change.
#if defined(OS_CHROMEOS) || defined(OS_WIN)
  return widget->GetNativeWindow()->delegate()->GetMinimumSize();
#elif defined(USE_X11)
  XSizeHints hints;
  long supplied_return;
  XGetWMNormalHints(
      gfx::GetXDisplay(),
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget(), &hints,
      &supplied_return);
  return gfx::Size(hints.min_width, hints.min_height);
#else
  NOTREACHED();
  return gfx::Size();
#endif
}

// static
ui::EventProcessor* WidgetTest::GetEventProcessor(Widget* widget) {
  return widget->GetNativeWindow()->GetHost()->event_processor();
}

// static
ui::internal::InputMethodDelegate* WidgetTest::GetInputMethodDelegateForWidget(
    Widget* widget) {
  return widget->GetNativeWindow()->GetRootWindow()->GetHost();
}

}  // namespace test
}  // namespace views
