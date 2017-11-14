// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_
#define UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "ui/android/ui_android_export.h"

namespace cc {
class SurfaceLayer;
enum class SurfaceDrawStatus;
}  // namespace cc

namespace viz {
class CompositorFrame;
class FrameSinkManagerImpl;
class HostFrameSinkManager;
}  // namespace viz

namespace ui {
class ViewAndroid;
class WindowAndroidCompositor;

class UI_ANDROID_EXPORT DelegatedFrameHostAndroid
    : public viz::mojom::CompositorFrameSinkClient,
      public viz::ExternalBeginFrameSourceClient,
      public viz::HostFrameSinkClient {
 public:
  class Client {
   public:
    virtual void SetBeginFrameSource(
        viz::BeginFrameSource* begin_frame_source) = 0;
    virtual void DidReceiveCompositorFrameAck() = 0;
    virtual void ReclaimResources(
        const std::vector<viz::ReturnedResource>&) = 0;
    virtual void OnFrameTokenChanged(uint32_t frame_token) = 0;
  };

  DelegatedFrameHostAndroid(ViewAndroid* view,
                            viz::HostFrameSinkManager* host_frame_sink_manager,
                            viz::FrameSinkManagerImpl* frame_sink_manager,
                            Client* client,
                            const viz::FrameSinkId& frame_sink_id);

  ~DelegatedFrameHostAndroid() override;

  void SubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                             viz::CompositorFrame frame);
  void DidNotProduceFrame(const viz::BeginFrameAck& ack);

  void DestroyDelegatedContent();

  bool HasDelegatedContent() const;

  viz::FrameSinkId GetFrameSinkId() const;

  // Should only be called when the host has a content layer. Use this for one-
  // off screen capture, not for video. Always provides RGBA_BITMAP
  // CopyOutputResults.
  void RequestCopyOfSurface(
      WindowAndroidCompositor* compositor,
      const gfx::Rect& src_subrect_in_pixel,
      viz::CopyOutputRequest::CopyOutputRequestCallback result_callback);

  void CompositorFrameSinkChanged();

  // Called when this DFH is attached/detached from a parent browser compositor
  // and needs to be attached to the surface hierarchy.
  void AttachToCompositor(WindowAndroidCompositor* compositor);
  void DetachFromCompositor();

  // Returns the ID for the current Surface.
  viz::SurfaceId SurfaceId() const;

 private:
  // viz::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 base::TimeTicks time,
                                 base::TimeDelta refresh,
                                 uint32_t flags) override;
  void DidDiscardCompositorFrame(uint32_t presentation_token) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFramePausedChanged(bool paused) override;

  // viz::ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

  void CreateNewCompositorFrameSinkSupport();

  const viz::FrameSinkId frame_sink_id_;

  ViewAndroid* view_;

  viz::HostFrameSinkManager* const host_frame_sink_manager_;
  viz::FrameSinkManagerImpl* const frame_sink_manager_;
  WindowAndroidCompositor* registered_parent_compositor_ = nullptr;
  Client* client_;

  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
  viz::ExternalBeginFrameSource begin_frame_source_;

  viz::SurfaceInfo surface_info_;
  bool has_transparent_background_ = false;

  scoped_refptr<cc::SurfaceLayer> content_layer_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameHostAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_
