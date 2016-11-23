// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/ui/public/cpp/gpu/gpu_service.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/mus_context_factory.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/test/test_screen.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/wm_state.h"

namespace ui {
namespace test {

class TestWM : public service_manager::Service,
               public aura::WindowTreeClientDelegate,
               public aura::WindowManagerDelegate {
 public:
  TestWM() {}

  ~TestWM() override {
    // WindowTreeHost uses state from WindowTreeClient, so destroy it first.
    window_tree_host_.reset();

    // WindowTreeClient destruction may callback to us.
    window_tree_client_.reset();

    gpu_service_.reset();

    display::Screen::SetScreenInstance(nullptr);
  }

 private:
  // service_manager::Service:
  void OnStart() override {
    CHECK(!started_);
    started_ = true;
    test_screen_ = base::MakeUnique<display::test::TestScreen>();
    display::Screen::SetScreenInstance(test_screen_.get());
    aura_env_ = aura::Env::CreateInstance(aura::Env::Mode::MUS);
    gpu_service_ = ui::GpuService::Create(context()->connector(), nullptr);
    compositor_context_factory_ =
        base::MakeUnique<aura::MusContextFactory>(gpu_service_.get());
    aura_env_->set_context_factory(compositor_context_factory_.get());
    window_tree_client_ = base::MakeUnique<aura::WindowTreeClient>(this, this);
    aura_env_->SetWindowTreeClient(window_tree_client_.get());
    window_tree_client_->ConnectAsWindowManager(context()->connector());
  }

  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    return false;
  }

  // aura::WindowTreeClientDelegate:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override {
    // WindowTreeClients configured as the window manager should never get
    // OnEmbed().
    NOTREACHED();
  }
  void OnLostConnection(aura::WindowTreeClient* client) override {
    window_tree_host_.reset();
    window_tree_client_.reset();
  }
  void OnEmbedRootDestroyed(aura::Window* root) override {
    // WindowTreeClients configured as the window manager should never get
    // OnEmbedRootDestroyed().
    NOTREACHED();
  }
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              aura::Window* target) override {
    // Don't care.
  }
  aura::client::CaptureClient* GetCaptureClient() override {
    return wm_state_.capture_controller();
  }
  aura::PropertyConverter* GetPropertyConverter() override {
    return &property_converter_;
  }

  // aura::WindowManagerDelegate:
  void SetWindowManagerClient(aura::WindowManagerClient* client) override {
    window_manager_client_ = client;
  }
  bool OnWmSetBounds(aura::Window* window, gfx::Rect* bounds) override {
    return true;
  }
  bool OnWmSetProperty(
      aura::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override {
    return true;
  }
  aura::Window* OnWmCreateTopLevelWindow(
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties) override {
    aura::Window* window = new aura::Window(nullptr);
    SetWindowType(window, window_type);
    window->Init(LAYER_NOT_DRAWN);
    window->SetBounds(gfx::Rect(10, 10, 500, 500));
    root_->AddChild(window);
    return window;
  }
  void OnWmClientJankinessChanged(const std::set<aura::Window*>& client_windows,
                                  bool janky) override {
    // Don't care.
  }
  void OnWmNewDisplay(std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
                      const display::Display& display) override {
    // Only handles a single root.
    DCHECK(!root_);
    window_tree_host_ = std::move(window_tree_host);
    root_ = window_tree_host_->window();
    DCHECK(window_manager_client_);
    window_manager_client_->AddActivationParent(root_);
    ui::mojom::FrameDecorationValuesPtr frame_decoration_values =
        ui::mojom::FrameDecorationValues::New();
    frame_decoration_values->max_title_bar_button_width = 0;
    window_manager_client_->SetFrameDecorationValues(
        std::move(frame_decoration_values));
    aura::client::SetFocusClient(root_, &focus_client_);
  }
  void OnWmDisplayRemoved(aura::WindowTreeHostMus* window_tree_host) override {
    DCHECK_EQ(window_tree_host, window_tree_host_.get());
    root_ = nullptr;
    window_tree_host_.reset();
  }
  void OnWmDisplayModified(const display::Display& display) override {}
  void OnWmPerformMoveLoop(aura::Window* window,
                           mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override {
    // Don't care.
  }
  void OnWmCancelMoveLoop(aura::Window* window) override {}

  // Dummy screen required to be the screen instance.
  std::unique_ptr<display::test::TestScreen> test_screen_;

  std::unique_ptr<aura::Env> aura_env_;
  ::wm::WMState wm_state_;
  aura::PropertyConverter property_converter_;
  aura::test::TestFocusClient focus_client_;
  std::unique_ptr<aura::WindowTreeHostMus> window_tree_host_;
  aura::Window* root_ = nullptr;
  aura::WindowManagerClient* window_manager_client_ = nullptr;
  std::unique_ptr<aura::WindowTreeClient> window_tree_client_;
  std::unique_ptr<ui::GpuService> gpu_service_;
  std::unique_ptr<aura::MusContextFactory> compositor_context_factory_;

  bool started_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWM);
};

}  // namespace test
}  // namespace ui

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(new ui::test::TestWM);
  return runner.Run(service_request_handle);
}
