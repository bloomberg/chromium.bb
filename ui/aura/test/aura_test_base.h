// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_BASE_H_
#define UI_AURA_TEST_AURA_TEST_BASE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/test/aura_test_helper.h"

namespace ui {
namespace mojom {
class WindowTreeClient;
}
}

namespace aura {
class Window;
class WindowDelegate;
class WindowManagerDelegate;
class WindowTreeClientDelegate;

namespace test {

enum class BackendType { CLASSIC, MUS };

// A base class for aura unit tests.
// TODO(beng): Instances of this test will create and own a RootWindow.
class AuraTestBase : public testing::Test,
                     public WindowTreeClientDelegate,
                     public WindowManagerDelegate {
 public:
  AuraTestBase();
  ~AuraTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Creates a normal window parented to |parent|.
  aura::Window* CreateNormalWindow(int id, Window* parent,
                                   aura::WindowDelegate* delegate);

 protected:
  void set_window_manager_delegate(
      WindowManagerDelegate* window_manager_delegate) {
    window_manager_delegate_ = window_manager_delegate;
  }

  void set_window_tree_client_delegate(
      WindowTreeClientDelegate* window_tree_client_delegate) {
    window_tree_client_delegate_ = window_tree_client_delegate;
  }

  // Turns on mus with a test WindowTree. Must be called before SetUp().
  void EnableMusWithTestWindowTree();

  // Used to configure the backend. This is exposed to make parameterized tests
  // easy to write. This *must* be called from SetUp().
  void ConfigureBackend(BackendType type);

  void RunAllPendingInMessageLoop();

  void ParentWindow(Window* window);

  // A convenience function for dispatching an event to |dispatcher()|.
  // Returns whether |event| was handled.
  bool DispatchEventUsingWindowDispatcher(ui::Event* event);

  Window* root_window() { return helper_->root_window(); }
  WindowTreeHost* host() { return helper_->host(); }
  ui::EventProcessor* event_processor() { return helper_->event_processor(); }
  TestScreen* test_screen() { return helper_->test_screen(); }

  TestWindowTree* window_tree() { return helper_->window_tree(); }
  WindowTreeClient* window_tree_client_impl() {
    return helper_->window_tree_client();
  }
  ui::mojom::WindowTreeClient* window_tree_client();

  // WindowTreeClientDelegate:
  void OnEmbed(std::unique_ptr<WindowTreeHostMus> window_tree_host) override;
  void OnUnembed(Window* root) override;
  void OnEmbedRootDestroyed(Window* root) override;
  void OnLostConnection(WindowTreeClient* client) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              Window* target) override;

  // WindowManagerDelegate:
  void SetWindowManagerClient(WindowManagerClient* client) override;
  bool OnWmSetBounds(Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(
      Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override;
  Window* OnWmCreateTopLevelWindow(
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWmClientJankinessChanged(const std::set<Window*>& client_windows,
                                  bool janky) override;
  void OnWmNewDisplay(std::unique_ptr<WindowTreeHostMus> window_tree_host,
                      const display::Display& display) override;
  void OnWmDisplayRemoved(WindowTreeHostMus* window_tree_host) override;
  void OnWmDisplayModified(const display::Display& display) override;
  ui::mojom::EventResult OnAccelerator(uint32_t id,
                                       const ui::Event& event) override;
  void OnWmPerformMoveLoop(Window* window,
                           ui::mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override;
  void OnWmCancelMoveLoop(Window* window) override;
  client::CaptureClient* GetCaptureClient() override;
  PropertyConverter* GetPropertyConverter() override;

 private:
  // Only used for mus. Both are are initialized to this, but may be reset.
  WindowManagerDelegate* window_manager_delegate_;
  WindowTreeClientDelegate* window_tree_client_delegate_;

  bool use_mus_ = false;
  bool setup_called_ = false;
  bool teardown_called_ = false;
  base::MessageLoopForUI message_loop_;
  PropertyConverter property_converter_;
  std::unique_ptr<AuraTestHelper> helper_;
  std::unique_ptr<WindowTreeHostMus> window_tree_host_mus_;

  DISALLOW_COPY_AND_ASSIGN(AuraTestBase);
};

// Use as a base class for tests that want to target both backends.
class AuraTestBaseWithType : public AuraTestBase,
                             public ::testing::WithParamInterface<BackendType> {
 public:
  AuraTestBaseWithType();
  ~AuraTestBaseWithType() override;

  // AuraTestBase:
  void SetUp() override;

 private:
  bool setup_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(AuraTestBaseWithType);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_BASE_H_
