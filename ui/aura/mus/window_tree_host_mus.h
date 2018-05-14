// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_
#define UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_

#include <stdint.h>

#include <memory>
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
class WindowTreeClient;
class WindowTreeHostMusDelegate;

struct DisplayInitParams;
struct WindowTreeHostMusInitParams;

class AURA_EXPORT WindowTreeHostMus : public WindowTreeHostPlatform {
 public:
  explicit WindowTreeHostMus(WindowTreeHostMusInitParams init_params);

  ~WindowTreeHostMus() override;

  // Returns the WindowTreeHostMus for |window|. This returns null if |window|
  // is null, or not in a WindowTreeHostMus.
  static WindowTreeHostMus* ForWindow(aura::Window* window);

  // Sets the bounds in pixels.
  void SetBoundsFromServerInPixels(const gfx::Rect& bounds_in_pixels,
                                   const viz::LocalSurfaceId& local_surface_id);

  ui::EventDispatchDetails SendEventToSink(ui::Event* event) {
    return aura::WindowTreeHostPlatform::SendEventToSink(event);
  }

  InputMethodMus* input_method() { return input_method_.get(); }

  // Sets the client area on the underlying mus window.
  void SetClientArea(const gfx::Insets& insets,
                     const std::vector<gfx::Rect>& additional_client_area);

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

  // Requests that the window manager perform |action| on the window.
  void PerformWmAction(const std::string& action);

  // Tells the window manager to take control of moving the window. Returns
  // true if the move wasn't canceled.
  void PerformWindowMove(ui::mojom::MoveLoopSource mus_source,
                         const gfx::Point& cursor_location,
                         const base::Callback<void(bool)>& callback);

  // Tells the window manager to abort any current move initiated by
  // PerformWindowMove().
  void CancelWindowMove();

  // Tells the window manager to confine the cursor to these specific bounds.
  void ConfineCursorToBounds(const gfx::Rect& pixel_bounds);

  // Used during initial setup. Returns the DisplayInitParams
  // supplied to the constructor.
  std::unique_ptr<DisplayInitParams> ReleaseDisplayInitParams();

  // Intended only for WindowTreeClient to call.
  void set_display_id(int64_t id) { display_id_ = id; }
  int64_t display_id() const { return display_id_; }
  display::Display GetDisplay() const;

  // Forces WindowTreeHost to re-setup the compositor to use the provided
  // |widget|.
  void OverrideAcceleratedWidget(gfx::AcceleratedWidget widget);

  // aura::WindowTreeHostPlatform:
  void HideImpl() override;
  void SetBoundsInPixels(const gfx::Rect& bounds,
                         const viz::LocalSurfaceId& local_surface_id =
                             viz::LocalSurfaceId()) override;
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnActivationChanged(bool active) override;
  void OnCloseRequest() override;
  void MoveCursorToScreenLocationInPixels(
      const gfx::Point& location_in_pixels) override;
  gfx::Transform GetRootTransformForLocalEventCoordinates() const override;
  int64_t GetDisplayId() override;

 private:
  int64_t display_id_;

  WindowTreeHostMusDelegate* delegate_;

  bool in_set_bounds_from_server_ = false;

  std::unique_ptr<InputMethodMus> input_method_;

  std::unique_ptr<DisplayInitParams> display_init_params_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_
