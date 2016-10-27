// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_HELPER_H_
#define UI_AURA_TEST_AURA_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"

namespace base {
class MessageLoopForUI;
}

namespace ui {
class ContextFactory;
class InputMethod;
class ScopedAnimationDurationScaleMode;
}

namespace aura {
class TestScreen;
class TestWindowTree;
class TestWindowTreeClientSetup;
class Window;
class WindowPort;
class WindowManagerDelegate;
class WindowTreeClient;
class WindowTreeClientDelegate;

namespace client {
class CaptureClient;
class DefaultCaptureClient;
class FocusClient;
}
namespace test {
class TestWindowParentingClient;

// A helper class owned by tests that does common initialization required for
// Aura use. This class creates a root window with clients and other objects
// that are necessary to run test on Aura.
class AuraTestHelper {
 public:
  explicit AuraTestHelper(base::MessageLoopForUI* message_loop);
  ~AuraTestHelper();

  void EnableMus(WindowTreeClientDelegate* window_tree_delegate,
                 WindowManagerDelegate* window_manager_delegate);

  // Creates and initializes (shows and sizes) the RootWindow for use in tests.
  void SetUp(ui::ContextFactory* context_factory);

  // Clean up objects that are created for tests. This also deletes the Env
  // object.
  void TearDown();

  // Flushes message loop.
  void RunAllPendingInMessageLoop();

  Window* root_window() { return host_->window(); }
  ui::EventProcessor* event_processor() { return host_->event_processor(); }
  WindowTreeHost* host() { return host_.get(); }

  TestScreen* test_screen() { return test_screen_.get(); }

  TestWindowTree* window_tree();
  WindowTreeClient* window_tree_client();

  client::FocusClient* focus_client() { return focus_client_.get(); }
  client::CaptureClient* capture_client();

 private:
  Env::WindowPortFactory InitMus();

  std::unique_ptr<WindowPort> CreateWindowPortMus(Window* window);

  base::MessageLoopForUI* message_loop_;
  bool use_mus_ = false;
  bool setup_called_;
  bool teardown_called_;
  std::unique_ptr<TestWindowTreeClientSetup> window_tree_client_setup_;
  std::unique_ptr<aura::Env> env_;
  std::unique_ptr<WindowTreeHost> host_;
  std::unique_ptr<TestWindowParentingClient> parenting_client_;
  std::unique_ptr<client::DefaultCaptureClient> capture_client_;
  std::unique_ptr<client::FocusClient> focus_client_;
  std::unique_ptr<TestScreen> test_screen_;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
  WindowTreeClientDelegate* window_tree_delegate_ = nullptr;
  WindowManagerDelegate* window_manager_delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AuraTestHelper);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_HELPER_H_
