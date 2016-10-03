// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_FACTORY_IMPL_H_
#define SERVICES_UI_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_FACTORY_IMPL_H_

#include "cc/surfaces/surface_id_allocator.h"
#include "services/ui/gpu/display_compositor/compositor_frame_sink_delegate.h"
#include "services/ui/public/interfaces/gpu/display_compositor.mojom.h"
#include "services/ui/surfaces/display_compositor.h"

namespace ui {
namespace gpu {

class CompositorFrameSinkImpl;

class CompositorFrameSinkFactoryImpl : public mojom::CompositorFrameSinkFactory,
                                       public CompositorFrameSinkDelegate {
 public:
  CompositorFrameSinkFactoryImpl(
      uint32_t client_id,
      const scoped_refptr<DisplayCompositor>& display_compositor);
  ~CompositorFrameSinkFactoryImpl() override;

  uint32_t client_id() const { return client_id_; }

  void CompositorFrameSinkConnectionLost(int local_id) override;
  cc::SurfaceId GenerateSurfaceId() override;

  // mojom::CompositorFrameSinkFactory implementation.
  void CreateCompositorFrameSink(
      uint32_t local_id,
      uint64_t nonce,
      mojo::InterfaceRequest<mojom::CompositorFrameSink> sink,
      mojom::CompositorFrameSinkClientPtr client) override;

 private:
  const uint32_t client_id_;
  scoped_refptr<DisplayCompositor> display_compositor_;
  cc::SurfaceIdAllocator allocator_;
  using CompositorFrameSinkMap =
      std::map<uint32_t, std::unique_ptr<CompositorFrameSinkImpl>>;
  CompositorFrameSinkMap sinks_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSinkFactoryImpl);
};

}  // namespace gpu
}  // namespace ui

#endif  // SERVICES_UI_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_FACTORY_IMPL_H_
