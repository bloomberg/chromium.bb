// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_
#define UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tree_host_platform.h"

namespace display {
class Display;
}

namespace aura {

class InputMethodMus;
class WindowPortMus;
class WindowTreeClient;
class WindowTreeHostMusDelegate;

class AURA_EXPORT WindowTreeHostMus : public aura::WindowTreeHostPlatform {
 public:
  // |properties| are applied to the window created by this class (using
  // PropertyConverter).
  // TODO: this should take an unordered_map, see http://crbug.com/670515.
  WindowTreeHostMus(
      std::unique_ptr<WindowPortMus> window_port,
      WindowTreeClient* window_tree_client,
      int64_t display_id,
      const std::map<std::string, std::vector<uint8_t>>* properties = nullptr);

  // This constructor is intended for creating top level windows in
  // non-window-manager code. |properties| are properties passed verbatim to
  // the server, that is, no conversion is done before sending |properties| to
  // the server. Additionally |properties| are passed to PropertyConverter and
  // any known properties are set on the Window created by this class.
  // TODO: this should take an unordered_map, see http://crbug.com/670515.
  explicit WindowTreeHostMus(
      WindowTreeClient* window_tree_client,
      const std::map<std::string, std::vector<uint8_t>>* properties = nullptr);

  ~WindowTreeHostMus() override;

  // Returns the WindowTreeHostMus for |window|. This returns null if |window|
  // is null, or not in a WindowTreeHostMus.
  static WindowTreeHostMus* ForWindow(aura::Window* window);

  // Sets the bounds in pixels.
  void SetBoundsFromServer(const gfx::Rect& bounds_in_pixels);

  ui::EventDispatchDetails SendEventToProcessor(ui::Event* event) {
    return aura::WindowTreeHostPlatform::SendEventToProcessor(event);
  }

  InputMethodMus* input_method() { return input_method_.get(); }

  // Sets the client area on the underlying mus window.
  void SetClientArea(const gfx::Insets& insets,
                     const std::vector<gfx::Rect>& additional_client_area);

  // Sets the hit test mask on the underlying mus window. Pass base::nullopt to
  // clear.
  void SetHitTestMask(const base::Optional<gfx::Rect>& rect);

  // Sets the opacity of the underlying mus window.
  void SetOpacity(float value);

  // Requests that the window manager change the activation to the next window.
  void DeactivateWindow();

  // Requests that our root window be stacked above this other root window
  // which our connection owns.
  void StackAbove(Window* window);

  // Requests that our root window be stacked above all other parallel root
  // windows which we might not own.
  void StackAtTop();

  // Tells the window manager to take control of moving the window. Returns
  // true if the move wasn't canceled.
  void PerformWindowMove(ui::mojom::MoveLoopSource mus_source,
                         const gfx::Point& cursor_location,
                         const base::Callback<void(bool)>& callback);

  // Tells the window manager to abort any current move initiated by
  // PerformWindowMove().
  void CancelWindowMove();

  // Intended only for WindowTreeClient to call.
  void set_display_id(int64_t id) { display_id_ = id; }
  int64_t display_id() const { return display_id_; }
  display::Display GetDisplay() const;

  // aura::WindowTreeHostPlatform:
  void HideImpl() override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnActivationChanged(bool active) override;
  void OnCloseRequest() override;
  gfx::ICCProfile GetICCProfileForCurrentDisplay() override;
  void MoveCursorToScreenLocationInPixels(
      const gfx::Point& location_in_pixels) override;

 private:
  int64_t display_id_;

  WindowTreeHostMusDelegate* delegate_;

  bool in_set_bounds_from_server_ = false;

  std::unique_ptr<InputMethodMus> input_method_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_
