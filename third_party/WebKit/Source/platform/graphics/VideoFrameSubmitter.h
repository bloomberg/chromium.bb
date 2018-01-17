// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VideoFrameSubmitter_h
#define VideoFrameSubmitter_h

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/VideoFrameResourceProvider.h"
#include "platform/wtf/Functional.h"
#include "public/platform/WebVideoFrameSubmitter.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"

namespace blink {

// This single-threaded class facilitates the communication between the media
// stack and mojo, providing compositor frames containing video frames to the
// |compositor_frame_sink_|. This class has dependencies on classes that use
// the media thread's OpenGL ContextProvider, and thus, besides construction,
// should be consistently ran from the same media SingleThreadTaskRunner.
class PLATFORM_EXPORT VideoFrameSubmitter
    : public WebVideoFrameSubmitter,
      public viz::mojom::blink::CompositorFrameSinkClient {
 public:
  explicit VideoFrameSubmitter(std::unique_ptr<VideoFrameResourceProvider>);

  ~VideoFrameSubmitter() override;

  static void CreateCompositorFrameSink(
      const viz::FrameSinkId,
      mojo::Binding<viz::mojom::blink::CompositorFrameSinkClient>*,
      viz::mojom::blink::CompositorFrameSinkPtr*);

  bool Rendering() { return is_rendering_; };
  cc::VideoFrameProvider* Provider() { return provider_; }
  mojo::Binding<viz::mojom::blink::CompositorFrameSinkClient>* Binding() {
    return &binding_;
  }
  void SetSink(viz::mojom::blink::CompositorFrameSinkPtr* sink) {
    compositor_frame_sink_ = std::move(*sink);
  }

  // VideoFrameProvider::Client implementation.
  void StopUsingProvider() override;
  void StartRendering() override;
  void StopRendering() override;
  void DidReceiveFrame() override;

  // WebVideoFrameSubmitter implementation.
  void Initialize(cc::VideoFrameProvider*) override;
  void StartSubmitting(const viz::FrameSinkId&) override;

  // cc::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const WTF::Vector<viz::ReturnedResource>& resources) override;
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 ::mojo::common::mojom::blink::TimeTicksPtr,
                                 WTF::TimeDelta refresh,
                                 uint32_t flags) final;
  void DidDiscardCompositorFrame(uint32_t presentation_token) final;
  void OnBeginFrame(const viz::BeginFrameArgs&) override;
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(
      const WTF::Vector<viz::ReturnedResource>& resources) override;

 private:
  void SubmitFrame(viz::BeginFrameAck, scoped_refptr<media::VideoFrame>);

  cc::VideoFrameProvider* provider_ = nullptr;
  viz::mojom::blink::CompositorFrameSinkPtr compositor_frame_sink_;
  mojo::Binding<viz::mojom::blink::CompositorFrameSinkClient> binding_;
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
  viz::LocalSurfaceId current_local_surface_id_;
  std::unique_ptr<VideoFrameResourceProvider> resource_provider_;

  bool is_rendering_;
  gfx::Size current_size_in_pixels_;
  base::WeakPtrFactory<VideoFrameSubmitter> weak_ptr_factory_;

  THREAD_CHECKER(media_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(VideoFrameSubmitter);
};

}  // namespace blink

#endif  // VideoFrameSubmitter_h
