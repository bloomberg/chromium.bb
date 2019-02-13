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
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/mus/input_method_mus_delegate.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/mojo/ui_base_types.mojom.h"

namespace display {
class Display;
}

namespace aura {

class InputMethodMus;
class WindowTreeClient;
class WindowTreeHostMusDelegate;

struct WindowTreeHostMusInitParams;

class AURA_EXPORT WindowTreeHostMus : public WindowTreeHostPlatform,
                                      public InputMethodMusDelegate {
 public:
  explicit WindowTreeHostMus(WindowTreeHostMusInitParams init_params);
  ~WindowTreeHostMus() override;

  // Returns the WindowTreeHostMus for |window|. This returns null if |window|
  // is null, or not in a WindowTreeHostMus.
  static WindowTreeHostMus* ForWindow(aura::Window* window);

  virtual void SetBounds(
      const gfx::Rect& bounds,
      const viz::LocalSurfaceIdAllocation& local_surface_id_allocation);
  void SetBoundsFromServer(
      const gfx::Rect& bounds,
      const viz::LocalSurfaceIdAllocation& local_surface_id_allocation);
  const gfx::Rect& bounds_in_dip() const { return bounds_in_dip_; }

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

  // Tells the window manager to take control of moving the window. Returns
  // true if the move wasn't canceled.
  void PerformWindowMove(Window* window,
                         ws::mojom::MoveLoopSource mus_source,
                         const gfx::Point& cursor_location,
                         int hit_test,
                         base::OnceCallback<void(bool)> callback);

  // Tells the window manager to abort any current move initiated by
  // PerformWindowMove().
  void CancelWindowMove();

  // Intended only for WindowTreeClient to call.
  void set_display_id(int64_t id) { display_id_ = id; }
  int64_t display_id() const { return display_id_; }
  display::Display GetDisplay() const;

  // Used to hold a LocalSurfaceIdAllocation from the server. This is used
  // when a LocalSurfaceIdAllocation is received from the server, and there are
  // inflight bounds changes. In such a scenario |id| is only applied once
  // all inflight changes have been acked, or the size changes requiring a new
  // LocalSurfaceIdAllocation. |id| is only applied after the changes have been
  // acked to ensure the client/server stay in sync with ids.
  void SetPendingLocalSurfaceIdFromServer(
      const viz::LocalSurfaceIdAllocation& id);
  bool has_pending_local_surface_id_from_server() const {
    return pending_local_surface_id_from_server_.has_value();
  }
  base::Optional<viz::LocalSurfaceIdAllocation>
  TakePendingLocalSurfaceIdFromServer();

  // aura::WindowTreeHostPlatform:
  void HideImpl() override;
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnActivationChanged(bool active) override;
  void OnCloseRequest() override;
  int64_t GetDisplayId() override;
  bool ShouldAllocateLocalSurfaceIdOnResize() override;
  gfx::Rect GetTransformedRootWindowBoundsInPixels(
      const gfx::Size& size_in_pixels) const override;

  // InputMethodMusDelegate:
  void SetTextInputState(ui::mojom::TextInputStatePtr state) override;
  void SetImeVisibility(bool visible,
                        ui::mojom::TextInputStatePtr state) override;

 protected:
  // This is in the protected section as SetBounds() is preferred.
  // aura::WindowTreeHostPlatform:
  void SetBoundsInPixels(
      const gfx::Rect& bounds,
      const viz::LocalSurfaceIdAllocation& local_surface_id_allocation =
          viz::LocalSurfaceIdAllocation()) override;

 private:
  int64_t display_id_;

  WindowTreeHostMusDelegate* delegate_;

  bool in_set_bounds_from_server_ = false;

  std::unique_ptr<InputMethodMus> input_method_;

  base::Optional<viz::LocalSurfaceIdAllocation>
      pending_local_surface_id_from_server_;

  gfx::Rect bounds_in_dip_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_HOST_MUS_H_
