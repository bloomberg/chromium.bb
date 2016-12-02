// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_HOST_MUS_DELEGATE_H_
#define UI_AURA_MUS_WINDOW_TREE_HOST_MUS_DELEGATE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "ui/aura/aura_export.h"

namespace gfx {
class Rect;
}

namespace aura {

class WindowPortMus;
class WindowTreeHostMus;

class AURA_EXPORT WindowTreeHostMusDelegate {
 public:
  // Called when the bounds of a WindowTreeHostMus is about to change.
  // |bounds| is the bounds supplied to WindowTreeHostMus::SetBounds() and is
  // in screen pixel coordinates.
  virtual void OnWindowTreeHostBoundsWillChange(
      WindowTreeHostMus* window_tree_host,
      const gfx::Rect& bounds) = 0;

  // Called when the client area of a WindowTreeHostMus is about to change.
  virtual void OnWindowTreeHostClientAreaWillChange(
      WindowTreeHostMus* window_tree_host,
      const gfx::Insets& client_area,
      const std::vector<gfx::Rect>& additional_client_areas) = 0;

  // Called when the hit test mask is about to be cleared.
  virtual void OnWindowTreeHostHitTestMaskWillChange(
      WindowTreeHostMus* window_tree_host,
      const base::Optional<gfx::Rect>& mask_rect) = 0;

  // Called when a WindowTreeHostMus is created without a WindowPort.
  // TODO: this should take an unordered_map, see http://crbug.com/670515.
  virtual std::unique_ptr<WindowPortMus> CreateWindowPortForTopLevel(
      const std::map<std::string, std::vector<uint8_t>>* properties) = 0;

  // Called from WindowTreeHostMus's constructor once the Window has been
  // created.
  virtual void OnWindowTreeHostCreated(WindowTreeHostMus* window_tree_host) = 0;

 protected:
  virtual ~WindowTreeHostMusDelegate() {}
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_HOST_MUS_DELEGATE_H_
