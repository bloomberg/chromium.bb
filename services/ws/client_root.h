// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_CLIENT_ROOT_H_
#define SERVICES_WS_CLIENT_ROOT_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class ClientSurfaceEmbedder;
class Window;
}  // namespace aura

namespace aura_extra {
class WindowPositionInRootMonitor;
}

namespace viz {
class LocalSurfaceIdAllocation;
class SurfaceInfo;
}

namespace ws {

class ProxyWindow;
class WindowTree;

// WindowTree creates a ClientRoot for each window the client is embedded in. A
// ClientRoot is created as the result of another client using Embed(), or this
// client requesting a top-level window. ClientRoot is responsible for
// maintaining state associated with the root, as well as notifying the client
// of any changes to the root Window.
class COMPONENT_EXPORT(WINDOW_SERVICE) ClientRoot
    : public aura::WindowObserver,
      public aura::WindowTreeHostObserver,
      public viz::HostFrameSinkClient {
 public:
  ClientRoot(WindowTree* window_tree, aura::Window* window, bool is_top_level);
  ~ClientRoot() override;

  // Registers the necessary state needed for embedding in viz.
  void RegisterVizEmbeddingSupport();

  aura::Window* window() { return window_; }

  bool is_top_level() const { return is_top_level_; }

  // Sets the bounds from a client.
  bool SetBoundsInScreenFromClient(
      const gfx::Rect& bounds,
      const base::Optional<viz::LocalSurfaceIdAllocation>& allocation);

  // Updates the LocalSurfaceIdAllocation from the client.
  void UpdateLocalSurfaceIdFromChild(
      const viz::LocalSurfaceIdAllocation& local_surface_id_allocation);

  // Called when the LocalSurfaceId of the embedder changes.
  void OnLocalSurfaceIdChanged();

  void AllocateLocalSurfaceIdAndNotifyClient();

  // Attaches/unattaches proxy_window->attached_frame_sink_id() to the
  // HostFrameSinkManager.
  void AttachChildFrameSinkId(ProxyWindow* proxy_window);
  void UnattachChildFrameSinkId(ProxyWindow* proxy_window);

  // Recurses through all descendants with the same WindowTree calling
  // AttachChildFrameSinkId()/UnattachChildFrameSinkId().
  void AttachChildFrameSinkIdRecursive(ProxyWindow* proxy_window);
  void UnattachChildFrameSinkIdRecursive(ProxyWindow* proxy_window);

  // Returns true if the WindowService should assign the LocalSurfaceId. A value
  // of false means the client is expected to providate the LocalSurfaceId.
  bool ShouldAssignLocalSurfaceId() const {
    return parent_local_surface_id_allocator_.has_value();
  }

 private:
  friend class ClientRootTestHelper;

  // If necessary, this generates a new LocalSurfaceId. Generally you should
  // call UpdateLocalSurfaceIdAndClientSurfaceEmbedder(), not this. If you call
  // this, you need to ensure the ClientSurfaceEmbedder is updated.
  void GenerateLocalSurfaceIdIfNecessary();

  // Updates cached state specific to the current LocalSurfaceId. This is called
  // any time |parent_local_surface_id_allocator_| has a new id.
  void UpdateSurfacePropertiesCache();

  // Calls GenerateLocalSurfaceIdIfNecessary() and if the current LocalSurfaceId
  // is valid, updates ClientSurfaceEmbedder.
  void UpdateLocalSurfaceIdAndClientSurfaceEmbedder();

  // Calls HandleBoundsOrScaleFactorChange() it the scale factor has changed.
  void CheckForScaleFactorChange();

  // Called when the bounds or scale factor changes. |old_bounds| is the
  // previous bounds, which may not have changed if the scale factor changes.
  void HandleBoundsOrScaleFactorChange(const gfx::Rect& old_bounds);

  void NotifyClientOfNewBounds(const gfx::Rect& old_bounds);

  // If necessary, notifies the client that the visibility of the Window is
  // |new_value|. This does nothing for top-levels.
  void NotifyClientOfVisibilityChange(bool new_value);

  // Callback when the position of |window_|, relative to the root, changes.
  // This is *only* called for non-top-levels.
  void OnPositionInRootChanged();

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;
  void OnWillMoveWindowToDisplay(aura::Window* window,
                                 int64_t new_display_id) override;
  void OnDidMoveWindowToDisplay(aura::Window* window) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // aura::WindowTreeHostObserver:
  void OnHostResized(aura::WindowTreeHost* host) override;

  // viz::HostFrameSinkClient:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

  WindowTree* window_tree_;
  aura::Window* window_;
  const bool is_top_level_;

  // |last_surface_size_in_pixels_| and |last_device_scale_factor_| are only
  // used if a LocalSurfaceId is needed for the window. They represent the size
  // and device scale factor at the time the LocalSurfaceId was generated.
  gfx::Size last_surface_size_in_pixels_;
  float last_device_scale_factor_ = 1.0f;

  std::unique_ptr<aura::ClientSurfaceEmbedder> client_surface_embedder_;

  bool is_moving_across_displays_ = false;
  base::Optional<gfx::Rect> scheduled_change_old_bounds_;

  // Used for non-top-levels to watch for changes in screen coordinates.
  std::unique_ptr<aura_extra::WindowPositionInRootMonitor>
      root_position_monitor_;

  // Last bounds sent to the client.
  gfx::Rect last_bounds_;

  // Last visibility value sent to the client. This is not used for top-levels.
  bool last_visible_;

  // If true, SetBoundsInScreenFromClient() is setting the window bounds.
  bool setting_bounds_from_client_ = false;

  // Only used if ShouldAssignLocalSurfaceId() returns true. This is used
  // instead of the ParentLocalSurfaceIdAllocator maintained by
  // WindowPortLocal as ClientRoot needs to control when the allocations happen,
  // and avoid allocations in the case of resizes and clients supplying their
  // own LocalSurfaceId.
  base::Optional<viz::ParentLocalSurfaceIdAllocator>
      parent_local_surface_id_allocator_;

  DISALLOW_COPY_AND_ASSIGN(ClientRoot);
};

}  // namespace ws

#endif  // SERVICES_WS_CLIENT_ROOT_H_
