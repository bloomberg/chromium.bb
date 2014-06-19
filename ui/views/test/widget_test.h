// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WIDGET_TEST_H_
#define UI_VIEWS_TEST_WIDGET_TEST_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/views_test_base.h"

#if defined(USE_AURA)
#include "ui/views/widget/native_widget_aura.h"
#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif
#elif defined(OS_MACOSX)
#include "ui/views/widget/native_widget_mac.h"
#endif

namespace ui {
class EventProcessor;
}

namespace views {

class NativeWidget;
class Widget;

#if defined(USE_AURA)
typedef NativeWidgetAura PlatformNativeWidget;
#if !defined(OS_CHROMEOS)
typedef DesktopNativeWidgetAura PlatformDesktopNativeWidget;
#endif
#elif defined(OS_MACOSX)
typedef NativeWidgetMac PlatformNativeWidget;
typedef NativeWidgetMac PlatformDesktopNativeWidget;
#endif

namespace internal {

class RootView;

}  // namespace internal

namespace test {

// A widget that assumes mouse capture always works. It won't on Aura in
// testing, so we mock it.
class NativeWidgetCapture : public PlatformNativeWidget {
 public:
  explicit NativeWidgetCapture(internal::NativeWidgetDelegate* delegate);
  virtual ~NativeWidgetCapture();

  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual bool HasCapture() const OVERRIDE;

 private:
  bool mouse_capture_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetCapture);
};

class WidgetTest : public ViewsTestBase {
 public:
  WidgetTest();
  virtual ~WidgetTest();

  NativeWidget* CreatePlatformNativeWidget(
      internal::NativeWidgetDelegate* delegate);

  Widget* CreateTopLevelPlatformWidget();

  Widget* CreateTopLevelFramelessPlatformWidget();

  Widget* CreateChildPlatformWidget(gfx::NativeView parent_native_view);

  Widget* CreateTopLevelNativeWidget();

  Widget* CreateChildNativeWidgetWithParent(Widget* parent);

  Widget* CreateChildNativeWidget();

  View* GetMousePressedHandler(internal::RootView* root_view);

  View* GetMouseMoveHandler(internal::RootView* root_view);

  View* GetGestureHandler(internal::RootView* root_view);

  // Simulate a OS-level destruction of the native widget held by |widget|.
  static void SimulateNativeDestroy(Widget* widget);

  // Return true if |window| is visible according to the native platform.
  static bool IsNativeWindowVisible(gfx::NativeWindow window);

  // Return the event processor for |widget|. On aura platforms, this is an
  // aura::WindowEventDispatcher. Otherwise, it is a bridge to the OS event
  // processor.
  static ui::EventProcessor* GetEventProcessor(Widget* widget);

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetTest);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_WIDGET_TEST_H_
