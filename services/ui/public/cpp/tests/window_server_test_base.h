// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_BASE_H_
#define SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_BASE_H_

#include <memory>

#include "base/macros.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/ui/public/cpp/tests/window_server_shelltest_base.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/public/interfaces/window_tree_host.mojom.h"

namespace ui {

// WindowServerTestBase is a base class for use with shell tests that use
// WindowServer. SetUp() connects to the WindowServer and blocks until OnEmbed()
// has been invoked. window_manager() can be used to access the WindowServer
// established as part of SetUp().
class WindowServerTestBase
    : public WindowServerServiceTestBase,
      public WindowTreeClientDelegate,
      public WindowManagerDelegate,
      public shell::InterfaceFactory<mojom::WindowTreeClient> {
 public:
  WindowServerTestBase();
  ~WindowServerTestBase() override;

  // True if WindowTreeClientDelegate::OnDidDestroyClient() was called.
  bool window_tree_client_destroyed() const {
    return window_tree_client_destroyed_;
  }

  // Runs the MessageLoop until QuitRunLoop() is called, or a timeout occurs.
  // Returns true on success. Generally prefer running a RunLoop and
  // explicitly quiting that, but use this for times when that is not possible.
  static bool DoRunLoopWithTimeout() WARN_UNUSED_RESULT;

  // Quits a run loop started by DoRunLoopWithTimeout(). Returns true on
  // success, false if a RunLoop isn't running.
  static bool QuitRunLoop() WARN_UNUSED_RESULT;

  WindowTreeClient* window_manager() { return window_manager_; }
  WindowManagerClient* window_manager_client() {
    return window_manager_client_;
  }

 protected:
  mojom::WindowTreeHost* host() { return host_.get(); }
  WindowTreeClient* most_recent_client() {
    return most_recent_client_;
  }

  void set_window_manager_delegate(WindowManagerDelegate* delegate) {
    window_manager_delegate_ = delegate;
  }

  // testing::Test:
  void SetUp() override;

  // WindowServerServiceTestBase:
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // WindowTreeClientDelegate:
  void OnEmbed(Window* root) override;
  void OnDidDestroyClient(WindowTreeClient* client) override;
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
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWmClientJankinessChanged(const std::set<Window*>& client_windows,
                                  bool not_responding) override;
  void OnWmNewDisplay(Window* window, const display::Display& display) override;
  void OnWmPerformMoveLoop(Window* window,
                           mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override;
  void OnWmCancelMoveLoop(Window* window) override;
  mojom::EventResult OnAccelerator(uint32_t accelerator_id,
                                   const ui::Event& event) override;

  // InterfaceFactory<WindowTreeClient>:
  void Create(const shell::Identity& remote_identity,
              mojo::InterfaceRequest<mojom::WindowTreeClient> request) override;

  // Used to receive the most recent window tree client loaded by an embed
  // action.
  WindowTreeClient* most_recent_client_;

 private:
  mojom::WindowTreeHostPtr host_;

  // The window server connection held by the window manager (app running at
  // the root window).
  WindowTreeClient* window_manager_;

  // A test can override the WM-related behaviour by installing its own
  // WindowManagerDelegate during the test.
  WindowManagerDelegate* window_manager_delegate_;

  WindowManagerClient* window_manager_client_;

  bool window_tree_client_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WindowServerTestBase);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_BASE_H_
