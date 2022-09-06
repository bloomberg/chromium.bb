// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SURFACE_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SURFACE_H_

#include <fuchsia/ui/composition/cpp/fidl.h>
#include <vulkan/vulkan.h>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/flatland/flatland_connection.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace ui {

class FlatlandSurfaceFactory;

// Holder for Flatland resources backing rendering surface.
//
// This object creates some simple Flatland resources for containing a window's
// texture, and attaches them to the parent View (by sending an IPC to the
// browser process).
//
// The texture is updated through an image pipe.
class FlatlandSurface : public ui::PlatformWindowSurface {
 public:
  FlatlandSurface(FlatlandSurfaceFactory* flatland_surface_factory,
                  gfx::AcceleratedWidget window);
  ~FlatlandSurface() override;
  FlatlandSurface(const FlatlandSurface&) = delete;
  FlatlandSurface& operator=(const FlatlandSurface&) = delete;

  // PlatformWindowSurface overrides.
  void Present(scoped_refptr<gfx::NativePixmap> primary_plane_pixmap,
               std::vector<ui::OverlayPlane> overlays,
               std::vector<gfx::GpuFenceHandle> acquire_fences,
               std::vector<gfx::GpuFenceHandle> release_fences,
               SwapCompletionCallback completion_callback,
               BufferPresentedCallback presentation_callback) override;

  // Creates a View for this surface, and returns a ViewHolderToken handle
  // that can be used to attach it into a scene graph.
  mojo::PlatformHandle CreateView();

  void AssertBelongsToCurrentThread() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

 private:
  friend class FlatlandSurfaceTest;

  struct PresentationState {
    base::TimeTicks presentation_time;
    base::TimeDelta interval;
  };

  struct PresentedFrame {
    PresentedFrame(fuchsia::ui::composition::ContentId image_id,
                   scoped_refptr<gfx::NativePixmap> primary_plane,
                   SwapCompletionCallback completion_callback,
                   BufferPresentedCallback presentation_callback);
    ~PresentedFrame();
    PresentedFrame(PresentedFrame&& other);
    PresentedFrame& operator=(PresentedFrame&& other);

    fuchsia::ui::composition::ContentId image_id;

    // Ensures the pixmap is not destroyed until after frame is presented.
    scoped_refptr<gfx::NativePixmap> primary_plane;

    SwapCompletionCallback completion_callback;
    BufferPresentedCallback presentation_callback;
  };

  // Contains Fuchsia's BufferCollection specific id that is used to refer to a
  // gfx::NativePixmap that can be presented through Flatland.
  struct FlatlandPixmapId {
    bool operator<(const FlatlandPixmapId& other_id) const {
      if (buffer_collection_id == other_id.buffer_collection_id) {
        return buffer_index < other_id.buffer_index;
      }
      return buffer_collection_id < other_id.buffer_collection_id;
    }

    base::UnguessableToken buffer_collection_id;
    uint32_t buffer_index;
  };

  // Contains TransformId and ContentId that are used to present an Image in
  // Flatland Scene Graph.
  struct FlatlandIds {
    fuchsia::ui::composition::ContentId image_id;
    fuchsia::ui::composition::TransformId transform_id;
  };

  void OnGetLayout(fuchsia::ui::composition::LayoutInfo info);

  void RemoveBufferCollection(FlatlandPixmapId pixmap_id);

  void OnPresentComplete(zx_time_t actual_presentation_time);

  FlatlandIds CreateOrGetFlatlandIds(gfx::NativePixmap* pixmap,
                                     bool is_primary_plane);

  void ClearScene();

  fuchsia::ui::composition::AllocatorPtr flatland_allocator_;
  FlatlandConnection flatland_;

  // Mapping between the NativePixmapHandle and ContentId id registered with
  // FlatlandConnection.
  base::flat_map<FlatlandPixmapId, FlatlandIds> pixmap_ids_to_flatland_ids_;

  base::circular_deque<PresentedFrame> pending_frames_;

  std::vector<zx::event> release_fences_from_last_present_;

  // Flatland resources used for the primary plane, that is not an overlay.
  fuchsia::ui::composition::TransformId root_transform_id_;
  // |child_transforms_| is expected to be sorted by z_order. Flatland relies on
  // the order of AddChild() calls to line the children from back-to-front, so
  // this container is used for order.
  std::map<int /* z_order */, fuchsia::ui::composition::TransformId>
      child_transforms_;
  fuchsia::ui::composition::TransformId primary_plane_transform_id_;

  fuchsia::ui::composition::ParentViewportWatcherPtr parent_viewport_watcher_;
  fuchsia::ui::composition::ChildViewWatcherPtr main_plane_view_watcher_;
  fuchsia::ui::composition::LayoutInfo layout_info_;

  FlatlandSurfaceFactory* const flatland_surface_factory_;
  const gfx::AcceleratedWidget window_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<FlatlandSurface> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SURFACE_H_
