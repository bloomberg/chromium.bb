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

#if defined(USE_AURA)
using PlatformNativeWidget = NativeWidgetAura;
#elif defined(OS_MACOSX)
using PlatformNativeWidget = NativeWidgetMac;
#endif

namespace {

// NativeWidget implementation that adds the following:
// . capture can be mocked.
// . a boolean is set when the NativeWiget is destroyed.
class TestPlatformNativeWidget : public PlatformNativeWidget {
 public:
  TestPlatformNativeWidget(internal::NativeWidgetDelegate* delegate,
                           bool mock_capture,
                           bool* destroyed);
  ~TestPlatformNativeWidget() override;

  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;

 private:
  bool mouse_capture_;
  const bool mock_capture_;
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformNativeWidget);
};

// A widget that assumes mouse capture always works. It won't in testing, so we
// mock it.
TestPlatformNativeWidget::TestPlatformNativeWidget(
    internal::NativeWidgetDelegate* delegate,
    bool mock_capture,
    bool* destroyed)
    : PlatformNativeWidget(delegate),
      mouse_capture_(false),
      mock_capture_(mock_capture),
      destroyed_(destroyed) {}

TestPlatformNativeWidget::~TestPlatformNativeWidget() {
  if (destroyed_)
    *destroyed_ = true;
}

void TestPlatformNativeWidget::SetCapture() {
  if (mock_capture_)
    mouse_capture_ = true;
  else
    PlatformNativeWidget::SetCapture();
}

void TestPlatformNativeWidget::ReleaseCapture() {
  if (mock_capture_) {
    if (mouse_capture_)
      delegate()->OnMouseCaptureLost();
    mouse_capture_ = false;
  } else {
    PlatformNativeWidget::ReleaseCapture();
  }
}

bool TestPlatformNativeWidget::HasCapture() const {
  return mock_capture_ ? mouse_capture_ : PlatformNativeWidget::HasCapture();
}

}  // namespace

// Factory methods ------------------------------------------------------------

NativeWidget* CreatePlatformNativeWidgetImpl(
    const Widget::InitParams& init_params,
    Widget* widget,
    uint32_t type,
    bool* destroyed) {
  return new TestPlatformNativeWidget(widget, type == kStubCapture, destroyed);
}

}  // namespace test
}  // namespace views
