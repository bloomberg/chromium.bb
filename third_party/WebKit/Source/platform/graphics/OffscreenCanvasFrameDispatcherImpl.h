// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasFrameDispatcherImpl_h
#define OffscreenCanvasFrameDispatcherImpl_h

#include <memory>
#include "cc/ipc/compositor_frame_sink.mojom-blink.h"
#include "cc/output/begin_frame_args.h"
#include "cc/resources/shared_bitmap.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/graphics/OffscreenCanvasFrameDispatcher.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/wtf/Compiler.h"

namespace blink {

class PLATFORM_EXPORT OffscreenCanvasFrameDispatcherImpl final
    : public OffscreenCanvasFrameDispatcher,
      NON_EXPORTED_BASE(public cc::mojom::blink::CompositorFrameSinkClient) {
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

  // cc::mojom::blink::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const WTF::Vector<cc::ReturnedResource>& resources) final;
  void OnBeginFrame(const cc::BeginFrameArgs&) final;
  void ReclaimResources(
      const WTF::Vector<cc::ReturnedResource>& resources) final;

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
  cc::LocalSurfaceIdAllocator local_surface_id_allocator_;
  const cc::FrameSinkId frame_sink_id_;
  cc::LocalSurfaceId current_local_surface_id_;

  int width_;
  int height_;
  bool change_size_for_next_commit_;
  bool suspend_animation_ = false;
  bool needs_begin_frame_ = false;
  int pending_compositor_frames_ = 0;

  unsigned next_resource_id_;

  struct FrameResource {
    RefPtr<StaticBitmapImage> image_;
    std::unique_ptr<cc::SharedBitmap> shared_bitmap_;
    GLuint texture_id_ = 0;
    GLuint image_id_ = 0;
    bool spare_lock_ = true;
    gpu::Mailbox mailbox_;

    FrameResource() {}
    ~FrameResource();
  };

  std::unique_ptr<FrameResource> recycleable_resource_;
  std::unique_ptr<FrameResource> createOrRecycleFrameResource();

  void SetNeedsBeginFrameInternal();

  typedef HashMap<unsigned, std::unique_ptr<FrameResource>> ResourceMap;
  void ReclaimResourceInternal(const ResourceMap::iterator&);
  ResourceMap resources_;

  bool VerifyImageSize(const IntSize);
  void PostImageToPlaceholder(RefPtr<StaticBitmapImage>);

  cc::mojom::blink::CompositorFrameSinkPtr sink_;
  mojo::Binding<cc::mojom::blink::CompositorFrameSinkClient> binding_;

  int placeholder_canvas_id_;

  cc::BeginFrameAck current_begin_frame_ack_;

  void SetTransferableResourceToSharedBitmap(cc::TransferableResource&,
                                             RefPtr<StaticBitmapImage>);
  void SetTransferableResourceToSharedGPUContext(cc::TransferableResource&,
                                                 RefPtr<StaticBitmapImage>);
  void SetTransferableResourceToStaticBitmapImage(cc::TransferableResource&,
                                                  RefPtr<StaticBitmapImage>);
};

}  // namespace blink

#endif  // OffscreenCanvasFrameDispatcherImpl_h
