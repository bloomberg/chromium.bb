// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_H_
#define SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_observer.h"
#include "components/display_compositor/gpu_compositor_frame_sink_delegate.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ui {

class DisplayProvider;

// The DisplayCompositor object is an object global to the Window Server app
// that holds the SurfaceServer and allocates new Surfaces namespaces.
// This object lives on the main thread of the Window Server.
// TODO(rjkroege, fsamuel): This object will need to change to support multiple
// displays.
class DisplayCompositor
    : public cc::SurfaceObserver,
      public display_compositor::GpuCompositorFrameSinkDelegate,
      public cc::mojom::DisplayCompositor {
 public:
  DisplayCompositor(DisplayProvider* display_provider,
                    cc::mojom::DisplayCompositorRequest request,
                    cc::mojom::DisplayCompositorClientPtr client);
  ~DisplayCompositor() override;

  cc::SurfaceManager* manager() { return &manager_; }

  // cc::mojom::DisplayCompositor implementation:
  void CreateRootCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      cc::mojom::MojoCompositorFrameSinkAssociatedRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_private_request)
      override;
  void CreateCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client) override;
  void RegisterFrameSinkHierarchy(
      const cc::FrameSinkId& parent_frame_sink_id,
      const cc::FrameSinkId& child_frame_sink_id) override;
  void UnregisterFrameSinkHierarchy(
      const cc::FrameSinkId& parent_frame_sink_id,
      const cc::FrameSinkId& child_frame_sink_id) override;
  void DropTemporaryReference(const cc::SurfaceId& surface_id) override;

 private:
  // It is necessary to pass |frame_sink_id| by value because the id
  // is owned by the GpuCompositorFrameSink in the map. When the sink is
  // removed from the map, |frame_sink_id| would also be destroyed if it were a
  // reference. But the map can continue to iterate and try to use it. Passing
  // by value avoids this.
  void DestroyCompositorFrameSink(cc::FrameSinkId frame_sink_id);

  // cc::SurfaceObserver implementation.
  void OnSurfaceCreated(const cc::SurfaceInfo& surface_info) override;
  void OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                        bool* changed) override;

  // display_compositor::GpuCompositorFrameSinkDelegate implementation.
  void OnClientConnectionLost(const cc::FrameSinkId& frame_sink_id,
                              bool destroy_compositor_frame_sink) override;
  void OnPrivateConnectionLost(const cc::FrameSinkId& frame_sink_id,
                               bool destroy_compositor_frame_sink) override;

  // SurfaceManager should be the first object constructed and the last object
  // destroyed in order to ensure that all other objects that depend on it have
  // access to a valid pointer for the entirety of their liftimes.
  cc::SurfaceManager manager_;

  // Provides a cc::Display for CreateRootCompositorFrameSink().
  DisplayProvider* const display_provider_;

  std::unordered_map<cc::FrameSinkId,
                     std::unique_ptr<cc::mojom::MojoCompositorFrameSink>,
                     cc::FrameSinkIdHash>
      compositor_frame_sinks_;

  base::ThreadChecker thread_checker_;

  cc::mojom::DisplayCompositorClientPtr client_;
  mojo::Binding<cc::mojom::DisplayCompositor> binding_;

  DISALLOW_COPY_AND_ASSIGN(DisplayCompositor);
};

}  // namespace ui

#endif  //  SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_H_
