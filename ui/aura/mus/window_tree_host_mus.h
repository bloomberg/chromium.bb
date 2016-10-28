// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_
#define UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_

#include <memory>

#include "base/macros.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/gfx/geometry/vector2d.h"

class SkBitmap;

namespace display {
class Display;
}

namespace service_manager {
class Connector;
}

namespace aura {

class InputMethodMus;
class WindowPortMus;
class WindowTreeHostMusDelegate;

enum class RootWindowType;

// WindowTreeHostMus is configured in two distinct modes:
// . with a content window. In this case the content window is added as a child
//   of the Window created by this class. Any changes to the size of the content
//   window is propagated to its parent. Additionally once the content window is
//   destroyed the WindowTreeHostMus is destroyed.
// . without a content window.
//
// If a content window is supplied WindowTreeHostMus deletes itself when the
// content window is destroyed. If no content window is supplied it is assumed
// the WindowTreeHostMus is explicitly deleted.
class AURA_EXPORT WindowTreeHostMus : public aura::WindowTreeHostPlatform {
 public:
  WindowTreeHostMus(std::unique_ptr<WindowPortMus> window_port,
                    WindowTreeHostMusDelegate* delegate,
                    RootWindowType root_window_type,
                    int64_t display_id,
                    Window* content_window = nullptr);
  ~WindowTreeHostMus() override;

  // Sets the bounds in dips.
  void SetBoundsFromServer(const gfx::Rect& bounds);

  ui::EventDispatchDetails SendEventToProcessor(ui::Event* event) {
    return aura::WindowTreeHostPlatform::SendEventToProcessor(event);
  }

  Window* content_window() { return content_window_; }

  InputMethodMus* input_method() { return input_method_.get(); }

  // Offset of the bounds from its parent. The Window (and content window if
  // present) always has an origin of 0x0 locally. This offset gives the offset
  // of the window in its parent.
  void set_origin_offset(const gfx::Vector2d& offset) {
    origin_offset_ = offset;
  }
  const gfx::Vector2d& origin_offset() const { return origin_offset_; }

  RootWindowType root_window_type() const { return root_window_type_; }

  void set_display_id(int64_t id) { display_id_ = id; }
  display::Display GetDisplay() const;

 private:
  class ContentWindowObserver;

  Window* GetWindowWithServerWindow();

  // Called when various things happen to the content window.
  void ContentWindowDestroyed();

  // aura::WindowTreeHostPlatform:
  void ShowImpl() override;
  void HideImpl() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() const override;
  gfx::Point GetLocationOnNativeScreen() const override;
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnActivationChanged(bool active) override;
  void OnCloseRequest() override;
  gfx::ICCProfile GetICCProfileForCurrentDisplay() override;

  int64_t display_id_;
  const RootWindowType root_window_type_;

  WindowTreeHostMusDelegate* delegate_;

  bool in_set_bounds_from_server_ = false;

  gfx::Vector2d origin_offset_;

  // May be null, see class description.
  Window* content_window_;

  std::unique_ptr<ContentWindowObserver> content_window_observer_;
  std::unique_ptr<InputMethodMus> input_method_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_
