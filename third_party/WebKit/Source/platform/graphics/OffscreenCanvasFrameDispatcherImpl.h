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
#include "wtf/Compiler.h"

namespace blink {

class PLATFORM_EXPORT OffscreenCanvasFrameDispatcherImpl final
    : public OffscreenCanvasFrameDispatcher,
      NON_EXPORTED_BASE(
          public cc::mojom::blink::MojoCompositorFrameSinkClient) {
 public:
  OffscreenCanvasFrameDispatcherImpl(OffscreenCanvasFrameDispatcherClient*,
                                     uint32_t clientId,
                                     uint32_t sinkId,
                                     int canvasId,
                                     int width,
                                     int height);

  // OffscreenCanvasFrameDispatcher implementation.
  ~OffscreenCanvasFrameDispatcherImpl() final;
  void setNeedsBeginFrame(bool) final;
  void dispatchFrame(RefPtr<StaticBitmapImage>,
                     double commitStartTime,
                     bool isWebGLSoftwareRendering = false) final;
  void reclaimResource(unsigned resourceId) final;
  void reshape(int width, int height) final;

  // cc::mojom::blink::MojoCompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck() final;
  void OnBeginFrame(const cc::BeginFrameArgs&) final;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) final;
  void WillDrawSurface(const cc::LocalSurfaceId&,
                       ::gfx::mojom::blink::RectPtr damageRect) final;

  // This enum is used in histogram, so it should be append-only.
  enum OffscreenCanvasCommitType {
    CommitGPUCanvasGPUCompositing = 0,
    CommitGPUCanvasSoftwareCompositing = 1,
    CommitSoftwareCanvasGPUCompositing = 2,
    CommitSoftwareCanvasSoftwareCompositing = 3,
    OffscreenCanvasCommitTypeCount,
  };

 private:
  // Surface-related
  cc::LocalSurfaceIdAllocator m_localSurfaceIdAllocator;
  const cc::FrameSinkId m_frameSinkId;
  cc::LocalSurfaceId m_currentLocalSurfaceId;

  int m_width;
  int m_height;
  bool m_changeSizeForNextCommit;
  bool m_needsBeginFrame;

  unsigned m_nextResourceId;
  HashMap<unsigned, RefPtr<StaticBitmapImage>> m_cachedImages;
  HashMap<unsigned, std::unique_ptr<cc::SharedBitmap>> m_sharedBitmaps;
  HashMap<unsigned, GLuint> m_cachedTextureIds;
  HashSet<unsigned> m_spareResourceLocks;

  bool verifyImageSize(const IntSize);
  void postImageToPlaceholder(RefPtr<StaticBitmapImage>);

  cc::mojom::blink::MojoCompositorFrameSinkPtr m_sink;
  mojo::Binding<cc::mojom::blink::MojoCompositorFrameSinkClient> m_binding;

  int m_placeholderCanvasId;

  void setTransferableResourceToSharedBitmap(cc::TransferableResource&,
                                             RefPtr<StaticBitmapImage>);
  void setTransferableResourceToSharedGPUContext(cc::TransferableResource&,
                                                 RefPtr<StaticBitmapImage>);
  void setTransferableResourceToStaticBitmapImage(cc::TransferableResource&,
                                                  RefPtr<StaticBitmapImage>);
};

}  // namespace blink

#endif  // OffscreenCanvasFrameDispatcherImpl_h
