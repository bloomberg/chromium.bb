// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DEMO_MUS_DEMO_H_
#define SERVICES_UI_DEMO_MUS_DEMO_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/display/screen_base.h"

namespace aura {
class Env;
class PropertyConverter;

namespace client {
class DefaultCaptureClient;
}
}  // namespace aura

namespace aura_extra {
class ImageWindowDelegate;
}

namespace wm {
class WMState;
}

namespace ui {
namespace demo {

// A simple MUS Demo service. This service connects to the service:ui, adds a
// new window to the root Window, and draws a spinning square in the center of
// the window. Provides a simple way to demonstrate that the graphic stack works
// as intended.
class MusDemo : public service_manager::Service,
                public aura::WindowTreeClientDelegate,
                public aura::WindowManagerDelegate {
 public:
  MusDemo();
  ~MusDemo() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // aura::WindowTreeClientDelegate:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override;
  void OnUnembed(aura::Window* root) override;
  void OnEmbedRootDestroyed(aura::WindowTreeHostMus* window_tree_host) override;
  void OnLostConnection(aura::WindowTreeClient* client) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              aura::Window* target) override;
  aura::client::CaptureClient* GetCaptureClient() override;
  aura::PropertyConverter* GetPropertyConverter() override;

  // aura::WindowManagerDelegate:
  void SetWindowManagerClient(aura::WindowManagerClient* client) override;
  bool OnWmSetBounds(aura::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(
      aura::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override;
  aura::Window* OnWmCreateTopLevelWindow(
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWmClientJankinessChanged(const std::set<aura::Window*>& client_windows,
                                  bool janky) override;
  void OnWmWillCreateDisplay(const display::Display& display) override;
  void OnWmNewDisplay(std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
                      const display::Display& display) override;
  void OnWmDisplayRemoved(aura::WindowTreeHostMus* window_tree_host) override;
  void OnWmDisplayModified(const display::Display& display) override;
  ui::mojom::EventResult OnAccelerator(uint32_t id,
                                       const ui::Event& event) override;
  void OnWmPerformMoveLoop(aura::Window* window,
                           ui::mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override;
  void OnWmCancelMoveLoop(aura::Window* window) override;
  void OnWmSetClientArea(
      aura::Window* window,
      const gfx::Insets& insets,
      const std::vector<gfx::Rect>& additional_client_areas) override;
  bool IsWindowActive(aura::Window* window) override;
  void OnWmDeactivateWindow(aura::Window* window) override;

  // Draws one frame, incrementing the rotation angle.
  void DrawFrame();

  aura::Window* root_window_ = nullptr;
  std::unique_ptr<aura::WindowTreeClient> window_tree_client_;
  std::unique_ptr<aura::WindowTreeHostMus> window_tree_host_;
  std::unique_ptr<aura::Env> env_;
  std::unique_ptr<display::ScreenBase> screen_;

  std::unique_ptr<aura::client::DefaultCaptureClient> capture_client_;
  std::unique_ptr<::wm::WMState> wm_state_;
  std::unique_ptr<aura::PropertyConverter> property_converter_;

  // Window to which we draw the bitmap.
  std::unique_ptr<aura::Window> bitmap_window_;

  // Destroys itself when the window gets destroyed.
  aura_extra::ImageWindowDelegate* window_delegate_ = nullptr;

  // Timer for calling DrawFrame().
  base::RepeatingTimer timer_;

  // Current rotation angle for drawing.
  double angle_ = 0.0;

  // Last time a frame was drawn.
  base::TimeTicks last_draw_frame_time_;

  DISALLOW_COPY_AND_ASSIGN(MusDemo);
};

}  // namespace demo
}  // namespace aura

#endif  // SERVICES_UI_DEMO_MUS_DEMO_H_
