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

class SkBitmap;

namespace service_manager {
class Connector;
}

namespace aura {

class InputMethodMus;
class WindowPortMus;

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
  explicit WindowTreeHostMus(std::unique_ptr<WindowPortMus> window_port,
                             Window* content_window = nullptr);
  ~WindowTreeHostMus() override;

  void CreateInputMethod(WindowPortMus* window_port_mus);

  ui::EventDispatchDetails SendEventToProcessor(ui::Event* event) {
    return aura::WindowTreeHostPlatform::SendEventToProcessor(event);
  }

  Window* content_window() { return content_window_; }

  InputMethodMus* input_method() { return input_method_.get(); }

 private:
  class ContentWindowObserver;

  // Called when various things happen to the content window.
  void ContentWindowDestroyed();
  void ContentWindowResized();
  void ContentWindowVisibilityChanging(bool visible);

  // aura::WindowTreeHostPlatform:
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnActivationChanged(bool active) override;
  void OnCloseRequest() override;
  gfx::ICCProfile GetICCProfileForCurrentDisplay() override;

  // May be null, see class description.
  Window* content_window_;

  std::unique_ptr<ContentWindowObserver> content_window_observer_;
  std::unique_ptr<InputMethodMus> input_method_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_
