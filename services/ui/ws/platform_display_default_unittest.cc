// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/platform_display_default.h"

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "services/ui/common/image_cursors_set.h"
#include "services/ui/ws/threaded_image_cursors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/system_input_injector.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ui {
namespace ws {
namespace {

// An EventSink that records events sent to it.
class TestEventSink : public EventSink {
 public:
  TestEventSink() { Reset(); }
  ~TestEventSink() override = default;

  void Reset() {
    count_ = 0;
    last_event_type_ = ET_UNKNOWN;
  }

  // EventSink:
  EventDispatchDetails OnEventFromSource(Event* event) override {
    count_++;
    last_event_type_ = event->type();
    return EventDispatchDetails();
  }

  int count_;
  EventType last_event_type_;
};

// A PlatformDisplayDelegate to connect the PlatformDisplay to a TestEventSink.
class TestPlatformDisplayDelegate : public PlatformDisplayDelegate {
 public:
  TestPlatformDisplayDelegate(TestEventSink* sink, OzonePlatform* platform)
      : event_sink_(sink), ozone_platform_(platform) {}
  ~TestPlatformDisplayDelegate() override = default;

  // PlatformDisplayDelegate:
  const display::Display& GetDisplay() override { return stub_display_; }
  ServerWindow* GetRootWindow() override { return nullptr; }
  EventSink* GetEventSink() override { return event_sink_; }
  void OnAcceleratedWidgetAvailable() override {}
  void OnNativeCaptureLost() override {}
  OzonePlatform* GetOzonePlatform() override { return ozone_platform_; }

 private:
  TestEventSink* event_sink_;
  OzonePlatform* ozone_platform_;
  display::Display stub_display_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformDisplayDelegate);
};

// An OzonePlatform that creates StubWindows.
class TestOzonePlatform : public OzonePlatform {
 public:
  TestOzonePlatform() = default;
  ~TestOzonePlatform() override = default;

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override { return nullptr; }
  ui::OverlayManagerOzone* GetOverlayManager() override { return nullptr; }
  ui::CursorFactoryOzone* GetCursorFactoryOzone() override { return nullptr; }
  ui::InputController* GetInputController() override { return nullptr; }
  ui::GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return nullptr;
  }
  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    return base::MakeUnique<StubWindow>(
        delegate, false /* use_default_accelerated_widget */);
  }
  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return nullptr;
  }
  void InitializeUI(const InitParams& params) override {}
  void InitializeGPU(const InitParams& params) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestOzonePlatform);
};

// Test fails in part because services_unittests appears to have its own ozone
// platform that it initializes. For some reason, this only started failing
// locally and on the trybots on 06/13/2017, while passing when run on the CQ
// and the builders. crbug.com/732987
TEST(PlatformDisplayDefaultTest, DISABLED_EventDispatch) {
  // ThreadTaskRunnerHandle needed required by ThreadedImageCursors.
  base::MessageLoop loop;
  // Setup ozone so the display can be initialized.
  TestOzonePlatform platform;

  // Create the display.
  display::ViewportMetrics metrics;
  metrics.bounds_in_pixels = gfx::Rect(1024, 768);
  metrics.device_scale_factor = 1.f;
  metrics.ui_scale_factor = 1.f;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  ImageCursorsSet image_cursors_set;
  std::unique_ptr<ThreadedImageCursors> threaded_image_cursors =
      base::MakeUnique<ThreadedImageCursors>(task_runner,
                                             image_cursors_set.GetWeakPtr());
  PlatformDisplayDefault display(nullptr, metrics,
                                 std::move(threaded_image_cursors));

  // Initialize the display with a test EventSink so we can sense events.
  TestEventSink event_sink;
  TestPlatformDisplayDelegate delegate(&event_sink, &platform);
  display.Init(&delegate);

  // Event dispatch is handled at the PlatformWindowDelegate level.
  PlatformWindowDelegate* display_for_dispatch =
      static_cast<PlatformWindowDelegate*>(&display);

  // Mouse events are converted to pointer events.
  MouseEvent mouse(ET_MOUSE_PRESSED, gfx::Point(1, 2), gfx::Point(1, 2),
                   base::TimeTicks(), EF_NONE, 0);
  display_for_dispatch->DispatchEvent(&mouse);
  EXPECT_EQ(ET_POINTER_DOWN, event_sink.last_event_type_);
  event_sink.Reset();

  // Touch events are converted to pointer events.
  TouchEvent touch(ET_TOUCH_PRESSED, gfx::Point(3, 4), base::TimeTicks(),
                   PointerDetails(EventPointerType::POINTER_TYPE_TOUCH, 0));
  display_for_dispatch->DispatchEvent(&touch);
  EXPECT_EQ(ET_POINTER_DOWN, event_sink.last_event_type_);
  event_sink.Reset();

  // Pressing a key dispatches exactly one event.
  KeyEvent key_pressed(ET_KEY_PRESSED, VKEY_A, EF_NONE);
  display_for_dispatch->DispatchEvent(&key_pressed);
  EXPECT_EQ(1, event_sink.count_);
  EXPECT_EQ(ET_KEY_PRESSED, event_sink.last_event_type_);
  event_sink.Reset();

  // Releasing the key dispatches exactly one event.
  KeyEvent key_released(ET_KEY_RELEASED, VKEY_A, EF_NONE);
  display_for_dispatch->DispatchEvent(&key_released);
  EXPECT_EQ(1, event_sink.count_);
  EXPECT_EQ(ET_KEY_RELEASED, event_sink.last_event_type_);
}

}  // namespace
}  // namespace ws
}  // namespace ui
