// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "ui/display/display.h"

namespace ui {
namespace test {

class TestWM : public shell::Service,
               public ui::WindowTreeClientDelegate,
               public ui::WindowManagerDelegate {
 public:
  TestWM() {}
  ~TestWM() override { delete window_tree_client_; }

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override {
    window_tree_client_ = new ui::WindowTreeClient(this, this, nullptr);
    window_tree_client_->ConnectAsWindowManager(connector());
  }

  // ui::WindowTreeClientDelegate:
  void OnEmbed(ui::Window* root) override {
    // WindowTreeClients configured as the window manager should never get
    // OnEmbed().
    NOTREACHED();
  }
  void OnDidDestroyClient(ui::WindowTreeClient* client) override {
    window_tree_client_ = nullptr;
  }
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              ui::Window* target) override {
    // Don't care.
  }

  // ui::WindowManagerDelegate:
  void SetWindowManagerClient(ui::WindowManagerClient* client) override {
    window_manager_client_ = client;
  }
  bool OnWmSetBounds(ui::Window* window, gfx::Rect* bounds) override {
    return true;
  }
  bool OnWmSetProperty(
      ui::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override {
    return true;
  }
  ui::Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override {
    ui::Window* window = root_->window_tree()->NewWindow(properties);
    window->SetBounds(gfx::Rect(10, 10, 500, 500));
    root_->AddChild(window);
    return window;
  }
  void OnWmClientJankinessChanged(const std::set<Window*>& client_windows,
                                  bool janky) override {
    // Don't care.
  }
  void OnWmNewDisplay(Window* window,
                      const display::Display& display) override {
    // Only handles a single root.
    DCHECK(!root_);
    root_ = window;
    DCHECK(window_manager_client_);
    window_manager_client_->AddActivationParent(root_);
    ui::mojom::FrameDecorationValuesPtr frame_decoration_values =
        ui::mojom::FrameDecorationValues::New();
    frame_decoration_values->max_title_bar_button_width = 0;
    window_manager_client_->SetFrameDecorationValues(
        std::move(frame_decoration_values));
  }
  void OnWmPerformMoveLoop(Window* window,
                           mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override {
    // Don't care.
  }
  void OnWmCancelMoveLoop(Window* window) override {}

  ui::Window* root_ = nullptr;
  ui::WindowManagerClient* window_manager_client_ = nullptr;
  // See WindowTreeClient for details on ownership.
  ui::WindowTreeClient* window_tree_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestWM);
};

}  // namespace test
}  // namespace ui

MojoResult ServiceMain(MojoHandle service_request_handle) {
  shell::ServiceRunner runner(new ui::test::TestWM);
  return runner.Run(service_request_handle);
}
