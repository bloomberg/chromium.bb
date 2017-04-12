// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasFrameDispatcherImpl_h
#define OffscreenCanvasFrameDispatcherImpl_h

#include <memory>
#include "cc/ipc/mojo_compositor_frame_sink.mojom-blink.h"
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
      NON_EXPORTED_BASE(
          public cc::mojom::blink::MojoCompositorFrameSinkClient) {
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
  void DispatchFrame(RefPtr<StaticBitmapImage>,
                     double commit_start_time,
                     bool is_web_gl_software_rendering = false) final;
  void ReclaimResource(unsigned resource_id) final;
  void Reshape(int width, int height) final;

  // cc::mojom::blink::MojoCompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const cc::ReturnedResourceArray& resources) final;
  void OnBeginFrame(const cc::BeginFrameArgs&) final;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) final;

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
  bool needs_begin_frame_;
  bool compositor_has_pending_frame_ = false;

  unsigned next_resource_id_;
  HashMap<unsigned, RefPtr<StaticBitmapImage>> cached_images_;
  HashMap<unsigned, std::unique_ptr<cc::SharedBitmap>> shared_bitmaps_;
  HashMap<unsigned, GLuint> cached_texture_ids_;
  HashSet<unsigned> spare_resource_locks_;

  bool VerifyImageSize(const IntSize);
  void PostImageToPlaceholder(RefPtr<StaticBitmapImage>);

  cc::mojom::blink::MojoCompositorFrameSinkPtr sink_;
  mojo::Binding<cc::mojom::blink::MojoCompositorFrameSinkClient> binding_;

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
