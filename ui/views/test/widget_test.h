// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WIDGET_TEST_H_
#define UI_VIEWS_TEST_WIDGET_TEST_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/views_test_base.h"

#if defined(USE_AURA)
#include "ui/views/widget/native_widget_aura.h"
#elif defined(OS_WIN)
#include "ui/views/widget/native_widget_win.h"
#endif

namespace views {

class NativeWidget;
class Widget;

namespace internal {

class RootView;

}  // namespace internal

namespace test {

#if defined(USE_AURA)
// A typedef that inserts our mock-capture NativeWidget implementation for
// relevant platforms.
typedef NativeWidgetAura NativeWidgetPlatform;

// A widget that assumes mouse capture always works. It won't on Aura in
// testing, so we mock it.
class NativeWidgetCapture : public NativeWidgetPlatform {
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

// A generic typedef to pick up relevant NativeWidget implementations.
typedef NativeWidgetCapture NativeWidgetPlatformForTest;
#elif defined(OS_WIN)
typedef NativeWidgetWin NativeWidgetPlatform;
typedef NativeWidgetWin NativeWidgetPlatformForTest;
#endif

class WidgetTest : public ViewsTestBase {
 public:
  WidgetTest();
  virtual ~WidgetTest();

  NativeWidget* CreatePlatformNativeWidget(
      internal::NativeWidgetDelegate* delegate);

  Widget* CreateTopLevelPlatformWidget();

  Widget* CreateTopLevelFramelessPlatformWidget();

  Widget* CreateChildPlatformWidget(gfx::NativeView parent_native_view);

#if defined(OS_WIN) && !defined(USE_AURA)
  // On Windows, it is possible for us to have a child window that is
  // TYPE_POPUP.
  Widget* CreateChildPopupPlatformWidget(gfx::NativeView parent_native_view);
#endif

  Widget* CreateTopLevelNativeWidget();

  Widget* CreateChildNativeWidgetWithParent(Widget* parent);

  Widget* CreateChildNativeWidget();

  View* GetMousePressedHandler(internal::RootView* root_view);

  View* GetMouseMoveHandler(internal::RootView* root_view);

  View* GetGestureHandler(internal::RootView* root_view);

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetTest);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_WIDGET_TEST_H_
