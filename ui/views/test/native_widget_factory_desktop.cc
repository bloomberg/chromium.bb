// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/native_widget_factory.h"

#if defined(USE_AURA)
#include "ui/views/widget/native_widget_aura.h"
#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif
#elif defined(OS_MACOSX)
#include "ui/views/widget/native_widget_mac.h"
#endif

namespace views {
namespace test {

#if !defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
using PlatformDesktopNativeWidget = NativeWidgetMac;
#else
using PlatformDesktopNativeWidget = DesktopNativeWidgetAura;
#endif

namespace {

class TestPlatformDesktopNativeWidget : public PlatformDesktopNativeWidget {
 public:
  TestPlatformDesktopNativeWidget(internal::NativeWidgetDelegate* delegate,
                                  bool* destroyed);
  ~TestPlatformDesktopNativeWidget() override;

 private:
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformDesktopNativeWidget);
};

// A widget that assumes mouse capture always works. It won't in testing, so we
// mock it.
TestPlatformDesktopNativeWidget::TestPlatformDesktopNativeWidget(
    internal::NativeWidgetDelegate* delegate,
    bool* destroyed)
    : PlatformDesktopNativeWidget(delegate), destroyed_(destroyed) {}

TestPlatformDesktopNativeWidget::~TestPlatformDesktopNativeWidget() {
  if (destroyed_)
    *destroyed_ = true;
}

}  // namespace

#endif

NativeWidget* CreatePlatformDesktopNativeWidgetImpl(
    const Widget::InitParams& init_params,
    Widget* widget,
    bool* destroyed) {
#if defined(OS_CHROMEOS)
  // Chromeos only has one NativeWidgetType.
  return CreatePlatformNativeWidgetImpl(init_params, widget, 0u, destroyed);
#else
  return new TestPlatformDesktopNativeWidget(widget, destroyed);
#endif
}

}  // namespace test
}  // namespace views
