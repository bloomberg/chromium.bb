// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasFrameDispatcherImpl_h
#define OffscreenCanvasFrameDispatcherImpl_h

#include <memory>
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/graphics/OffscreenCanvasFrameDispatcher.h"
#include "platform/graphics/OffscreenCanvasResourceProvider.h"
#include "platform/wtf/Compiler.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"

namespace blink {

class PLATFORM_EXPORT OffscreenCanvasFrameDispatcherImpl final
    : public OffscreenCanvasFrameDispatcher,
      public viz::mojom::blink::CompositorFrameSinkClient {
 public:
  OffscreenCanvasFrameDispatcherImpl(OffscreenCanvasFrameDispatcherClient*,
                                     uint32_t client_id,
                                     uint32_t sink_id,
                                     int canvas_id,
                                     int width,
                                     int height);

  // OffscreenCanvasFrameDispatcher implementation.
  ~OffscreenCanvasFrameDispatcherImpl() final;
  void SetNeedsBeginFrame(bool) final;
  void SetSuspendAnimation(bool) final;
  bool NeedsBeginFrame() const final { return needs_begin_frame_; }
  bool IsAnimationSuspended() const final { return suspend_animation_; }
  void DispatchFrame(RefPtr<StaticBitmapImage>,
                     double commit_start_time,
                     const SkIRect& damage_rect,
                     bool is_web_gl_software_rendering = false) final;
  void ReclaimResource(unsigned resource_id) final;
  void Reshape(int width, int height) final;

  // viz::mojom::blink::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const WTF::Vector<viz::ReturnedResource>& resources) final;
  void OnBeginFrame(const viz::BeginFrameArgs&) final;
  void OnBeginFramePausedChanged(bool paused) final{};
  void ReclaimResources(
      const WTF::Vector<viz::ReturnedResource>& resources) final;

  // This enum is used in histogram, so it should be append-only.
  enum OffscreenCanvasCommitType {
    kCommitGPUCanvasGPUCompositing = 0,
    kCommitGPUCanvasSoftwareCompositing = 1,
    kCommitSoftwareCanvasGPUCompositing = 2,
    kCommitSoftwareCanvasSoftwareCompositing = 3,
    kOffscreenCanvasCommitTypeCount,
  };

 private:
  // Surface-related
  viz::LocalSurfaceIdAllocator local_surface_id_allocator_;
  const viz::FrameSinkId frame_sink_id_;
  viz::LocalSurfaceId current_local_surface_id_;

  int width_;
  int height_;
  bool change_size_for_next_commit_;
  bool suspend_animation_ = false;
  bool needs_begin_frame_ = false;
  int pending_compositor_frames_ = 0;

  void SetNeedsBeginFrameInternal();

  std::unique_ptr<OffscreenCanvasResourceProvider>
      offscreen_canvas_resource_provider_;

  bool VerifyImageSize(const IntSize);
  void PostImageToPlaceholder(RefPtr<StaticBitmapImage>);

  viz::mojom::blink::CompositorFrameSinkPtr sink_;
  mojo::Binding<viz::mojom::blink::CompositorFrameSinkClient> binding_;

  int placeholder_canvas_id_;

  viz::BeginFrameAck current_begin_frame_ack_;
};

}  // namespace blink

#endif  // OffscreenCanvasFrameDispatcherImpl_h
