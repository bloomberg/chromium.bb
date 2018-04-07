// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_FRAME_DISPATCHER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_FRAME_DISPATCHER_IMPL_H_

#include <memory>
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_frame_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/wtf/compiler.h"

namespace blink {

class PLATFORM_EXPORT OffscreenCanvasFrameDispatcherImpl
    : public OffscreenCanvasFrameDispatcher,
      public viz::mojom::blink::CompositorFrameSinkClient {
 public:
  enum {
    kInvalidPlaceholderCanvasId = -1,
  };

  OffscreenCanvasFrameDispatcherImpl(OffscreenCanvasFrameDispatcherClient*,
                                     uint32_t client_id,
                                     uint32_t sink_id,
                                     int placeholder_canvas_id,
                                     int width,
                                     int height);

  // OffscreenCanvasFrameDispatcher implementation.
  virtual ~OffscreenCanvasFrameDispatcherImpl();
  void SetNeedsBeginFrame(bool) final;
  void SetSuspendAnimation(bool) final;
  bool NeedsBeginFrame() const final { return needs_begin_frame_; }
  bool IsAnimationSuspended() const final { return suspend_animation_; }
  void DispatchFrame(scoped_refptr<StaticBitmapImage>,
                     double commit_start_time,
                     const SkIRect& damage_rect) final;
  void ReclaimResource(unsigned resource_id) final;
  void Reshape(int width, int height) final;

  // viz::mojom::blink::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const WTF::Vector<viz::ReturnedResource>& resources) final;
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 mojo_base::mojom::blink::TimeTicksPtr,
                                 WTF::TimeDelta refresh,
                                 uint32_t flags) final;
  void DidDiscardCompositorFrame(uint32_t presentation_token) final;
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
  friend class OffscreenCanvasFrameDispatcherImplTest;

  // Surface-related
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
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
  void PostImageToPlaceholderIfNotBlocked(scoped_refptr<StaticBitmapImage>,
                                          unsigned resource_id);
  // virtual for testing
  virtual void PostImageToPlaceholder(scoped_refptr<StaticBitmapImage>,
                                      unsigned resource_id);

  viz::mojom::blink::CompositorFrameSinkPtr sink_;
  mojo::Binding<viz::mojom::blink::CompositorFrameSinkClient> binding_;

  int placeholder_canvas_id_;

  // The latest_unposted_resource_id_ always refers to the Id of the frame
  // resource used by the latest_unposted_image_.
  scoped_refptr<StaticBitmapImage> latest_unposted_image_;
  unsigned latest_unposted_resource_id_;
  unsigned num_unreclaimed_frames_posted_;

  viz::BeginFrameAck current_begin_frame_ack_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_FRAME_DISPATCHER_IMPL_H_
