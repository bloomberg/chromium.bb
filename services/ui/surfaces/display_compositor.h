// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_H_
#define SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/local_frame_id.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_observer.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "ipc/ipc_channel_handle.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace gpu {
class GpuMemoryBufferManager;
class ImageFactory;
}

namespace cc {
class Display;
class SurfaceManager;
class SyntheticBeginFrameSource;
}

namespace ui {

namespace test {
class DisplayCompositorTest;
}

class DisplayCompositorClient;
class GpuCompositorFrameSink;

// The DisplayCompositor object is an object global to the Window Server app
// that holds the SurfaceServer and allocates new Surfaces namespaces.
// This object lives on the main thread of the Window Server.
// TODO(rjkroege, fsamuel): This object will need to change to support multiple
// displays.
class DisplayCompositor : public cc::SurfaceObserver,
                          public cc::mojom::DisplayCompositor {
 public:
  DisplayCompositor(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service,
      std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      cc::mojom::DisplayCompositorRequest request,
      cc::mojom::DisplayCompositorClientPtr client);
  ~DisplayCompositor() override;

  cc::SurfaceManager* manager() { return &manager_; }

  // Adds surface references. For each reference added, this will remove the
  // temporary reference to the child surface if one exists.
  void AddSurfaceReferences(
      const std::vector<cc::SurfaceReference>& references);

  // Removes surface references.
  void RemoveSurfaceReferences(
      const std::vector<cc::SurfaceReference>& references);

  // We must avoid destroying a GpuCompositorFrameSink until both the display
  // compositor host and the client drop their connection to avoid getting into
  // a state where surfaces references are inconsistent.
  void OnCompositorFrameSinkClientConnectionLost(
      const cc::FrameSinkId& frame_sink_id,
      bool destroy_compositor_frame_sink);
  void OnCompositorFrameSinkPrivateConnectionLost(
      const cc::FrameSinkId& frame_sink_id,
      bool destroy_compositor_frame_sink);

  // cc::mojom::DisplayCompositor implementation:
  void CreateCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client) override;

 private:
  friend class test::DisplayCompositorTest;

  void AddSurfaceReference(const cc::SurfaceReference& ref);
  void RemoveSurfaceReference(const cc::SurfaceReference& ref);

  std::unique_ptr<cc::Display> CreateDisplay(
      const cc::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      cc::SyntheticBeginFrameSource* begin_frame_source);

  const cc::SurfaceId& GetRootSurfaceId() const;

  // cc::SurfaceObserver implementation.
  void OnSurfaceCreated(const cc::SurfaceId& surface_id,
                        const gfx::Size& frame_size,
                        float device_scale_factor) override;
  void OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                        bool* changed) override;

  // SurfaceManager should be the first object constructed and the last object
  // destroyed in order to ensure that all other objects that depend on it have
  // access to a valid pointer for the entirety of their liftimes.
  cc::SurfaceManager manager_;

  // Will normally point to |manager_| as it provides the interface. For tests
  // it will be swapped out with a mock implementation.
  cc::SurfaceReferenceManager* reference_manager_;

  scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service_;
  std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  gpu::ImageFactory* image_factory_;

  std::unordered_map<cc::FrameSinkId,
                     std::unique_ptr<GpuCompositorFrameSink>,
                     cc::FrameSinkIdHash>
      compositor_frame_sinks_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // SurfaceIds that have temporary references from top level root so they
  // aren't GC'd before DisplayCompositorClient can add a real reference. This
  // is basically a collection of surface ids, for example:
  //   cc::SurfaceId surface_id(key, value[index]);
  // The LocalFrameIds are stored in the order the surfaces are created in.
  std::unordered_map<cc::FrameSinkId,
                     std::vector<cc::LocalFrameId>,
                     cc::FrameSinkIdHash>
      temp_references_;

  base::ThreadChecker thread_checker_;

  cc::mojom::DisplayCompositorClientPtr client_;
  mojo::Binding<cc::mojom::DisplayCompositor> binding_;

  DISALLOW_COPY_AND_ASSIGN(DisplayCompositor);
};

}  // namespace ui

#endif  //  SERVICES_UI_SURFACES_DISPLAY_COMPOSITOR_H_
