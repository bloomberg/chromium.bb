// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_
#define UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/output/copy_output_request.h"
#include "cc/resources/returned_resource.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "ui/android/ui_android_export.h"

namespace cc {

class CompositorFrame;
class SurfaceLayer;
enum class SurfaceDrawStatus;

}  // namespace cc

namespace viz {
class FrameSinkManagerImpl;
class HostFrameSinkManager;
}  // namespace viz

namespace ui {
class ViewAndroid;
class WindowAndroidCompositor;

class UI_ANDROID_EXPORT DelegatedFrameHostAndroid
    : public viz::CompositorFrameSinkSupportClient,
      public cc::ExternalBeginFrameSourceClient {
 public:
  class Client {
   public:
    virtual void SetBeginFrameSource(
        cc::BeginFrameSource* begin_frame_source) = 0;
    virtual void DidReceiveCompositorFrameAck() = 0;
    virtual void ReclaimResources(const std::vector<cc::ReturnedResource>&) = 0;
  };

  DelegatedFrameHostAndroid(ViewAndroid* view,
                            viz::HostFrameSinkManager* host_frame_sink_manager,
                            viz::FrameSinkManagerImpl* frame_sink_manager,
                            Client* client,
                            const viz::FrameSinkId& frame_sink_id);

  ~DelegatedFrameHostAndroid() override;

  void SubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                             cc::CompositorFrame frame);
  void DidNotProduceFrame(const cc::BeginFrameAck& ack);

  void DestroyDelegatedContent();

  bool HasDelegatedContent() const;

  viz::FrameSinkId GetFrameSinkId() const;

  // Should only be called when the host has a content layer.
  void RequestCopyOfSurface(
      WindowAndroidCompositor* compositor,
      const gfx::Rect& src_subrect_in_pixel,
      cc::CopyOutputRequest::CopyOutputRequestCallback result_callback);

  void CompositorFrameSinkChanged();

  // Called when this DFH is attached/detached from a parent browser compositor
  // and needs to be attached to the surface hierarchy.
  void AttachToCompositor(WindowAndroidCompositor* compositor);
  void DetachFromCompositor();

  // Returns the ID for the current Surface.
  viz::SurfaceId SurfaceId() const;

 private:
  // viz::CompositorFrameSinkSupportClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override;
  void WillDrawSurface(const viz::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;

  // cc::ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void CreateNewCompositorFrameSinkSupport();

  const viz::FrameSinkId frame_sink_id_;

  ViewAndroid* view_;

  viz::HostFrameSinkManager* const host_frame_sink_manager_;
  viz::FrameSinkManagerImpl* const frame_sink_manager_;
  WindowAndroidCompositor* registered_parent_compositor_ = nullptr;
  Client* client_;

  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
  cc::ExternalBeginFrameSource begin_frame_source_;

  viz::SurfaceInfo surface_info_;
  bool has_transparent_background_ = false;

  scoped_refptr<cc::SurfaceLayer> content_layer_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameHostAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_
