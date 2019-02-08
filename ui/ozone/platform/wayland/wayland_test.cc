// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_test.h"

#include "base/run_loop.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

using ::testing::_;
using ::testing::SaveArg;

namespace ui {

WaylandTest::WaylandTest()
    : scoped_task_environment_(
          base::test::ScopedTaskEnvironment::MainThreadType::UI) {
#if BUILDFLAG(USE_XKBCOMMON)
  KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
      std::make_unique<XkbKeyboardLayoutEngine>(xkb_evdev_code_converter_));
#else
  KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
      std::make_unique<StubKeyboardLayoutEngine>());
#endif
  connection_.reset(new WaylandConnection);
  connection_proxy_.reset(new WaylandConnectionProxy(connection_.get()));
  window_ = std::make_unique<WaylandWindow>(&delegate_, connection_.get());
}

WaylandTest::~WaylandTest() {}

void WaylandTest::SetUp() {
  ASSERT_TRUE(server_.Start(GetParam()));
  ASSERT_TRUE(connection_->Initialize());
  EXPECT_CALL(delegate_, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&widget_));
  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 800, 600);
  properties.type = PlatformWindowType::kWindow;
  ASSERT_TRUE(window_->Initialize(std::move(properties)));
  ASSERT_NE(widget_, gfx::kNullAcceleratedWidget);

  // Wait for the client to flush all pending requests from initialization.
  base::RunLoop().RunUntilIdle();

  // Pause the server after it has responded to all incoming events.
  server_.Pause();

  surface_ = server_.GetObject<wl::MockSurface>(widget_);
  ASSERT_TRUE(surface_);

  initialized_ = true;
}

void WaylandTest::TearDown() {
  if (initialized_)
    Sync();
}

void WaylandTest::Sync() {
  // Resume the server, flushing its pending events.
  server_.Resume();

  // Wait for the client to finish processing these events.
  base::RunLoop().RunUntilIdle();

  // Pause the server, after it has finished processing any follow-up requests
  // from the client.
  server_.Pause();
}

}  // namespace ui
