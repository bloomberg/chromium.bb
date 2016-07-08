// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DEMO_MUS_DEMO_H_
#define SERVICES_UI_DEMO_MUS_DEMO_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "services/shell/public/cpp/service.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace bitmap_uploader {
class BitmapUploader;
}

namespace mus_demo {

// A simple MUS Demo mojo app. This app connects to the mojo:ui, creates a new
// window and draws a spinning square in the center of the window. Provides a
// simple way to demonstrate that the graphic stack works as intended.
class MusDemo : public shell::Service,
                public ui::WindowTreeClientDelegate,
                public ui::WindowManagerDelegate {
 public:
  MusDemo();
  ~MusDemo() override;

 private:
  // shell::Service:
  void OnStart(shell::Connector* connector,
               const shell::Identity& identity,
               uint32_t id) override;
  bool OnConnect(shell::Connection* connection) override;

  // WindowTreeClientDelegate:
  void OnEmbed(ui::Window* root) override;
  void OnDidDestroyClient(ui::WindowTreeClient* client) override;
  void OnEventObserved(const ui::Event& event, ui::Window* target) override;

  // WindowManagerDelegate:
  void SetWindowManagerClient(ui::WindowManagerClient* client) override;
  bool OnWmSetBounds(ui::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(
      ui::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override;
  ui::Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWmClientJankinessChanged(const std::set<ui::Window*>& client_windows,
                                  bool janky) override;
  void OnWmNewDisplay(ui::Window* window,
                      const display::Display& display) override;
  void OnWmPerformMoveLoop(ui::Window* window,
                           ui::mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override;
  void OnWmCancelMoveLoop(ui::Window* window) override;

  // Allocate a bitmap the same size as the window to draw into.
  void AllocBitmap();

  // Draws one frame, incrementing the rotation angle.
  void DrawFrame();

  shell::Connector* connector_ = nullptr;

  ui::Window* window_ = nullptr;
  ui::WindowTreeClient* window_tree_client_ = nullptr;

  // Used to send frames to mus.
  std::unique_ptr<bitmap_uploader::BitmapUploader> uploader_;

  // Bitmap that is the same size as our client window area.
  SkBitmap bitmap_;

  // Timer for calling DrawFrame().
  base::RepeatingTimer timer_;

  // Current rotation angle for drawing.
  double angle_ = 0.0;

  DISALLOW_COPY_AND_ASSIGN(MusDemo);
};

}  // namespace ui_demo

#endif  // SERVICES_UI_DEMO_MUS_DEMO_H_
