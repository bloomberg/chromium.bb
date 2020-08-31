// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_SYNCHRONOUS_COMPOSITOR_PROXY_H_
#define CONTENT_RENDERER_INPUT_SYNCHRONOUS_COMPOSITOR_PROXY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/optional.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "content/common/input/synchronous_compositor.mojom.h"
#include "content/renderer/android/synchronous_layer_tree_frame_sink.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/platform/input/synchronous_input_handler_proxy.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_f.h"

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace content {

class SynchronousLayerTreeFrameSink;

class SynchronousCompositorProxy : public blink::SynchronousInputHandler,
                                   public SynchronousLayerTreeFrameSinkClient,
                                   public mojom::SynchronousCompositor {
 public:
  SynchronousCompositorProxy(
      blink::SynchronousInputHandlerProxy* input_handler_proxy);
  ~SynchronousCompositorProxy() override;

  void Init();
  void BindChannel(
      mojo::PendingRemote<mojom::SynchronousCompositorControlHost> control_host,
      mojo::PendingAssociatedRemote<mojom::SynchronousCompositorHost> host,
      mojo::PendingAssociatedReceiver<mojom::SynchronousCompositor>
          compositor_request);

  // blink::SynchronousInputHandler overrides.
  void UpdateRootLayerState(const gfx::ScrollOffset& total_scroll_offset,
                            const gfx::ScrollOffset& max_scroll_offset,
                            const gfx::SizeF& scrollable_size,
                            float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor) final;

  // SynchronousLayerTreeFrameSinkClient overrides.
  void DidActivatePendingTree() final;
  void Invalidate(bool needs_draw) final;
  void SubmitCompositorFrame(
      uint32_t layer_tree_frame_sink_id,
      base::Optional<viz::CompositorFrame> frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list) final;
  void SetNeedsBeginFrames(bool needs_begin_frames) final;
  void SinkDestroyed() final;

  void SetLayerTreeFrameSink(
      SynchronousLayerTreeFrameSink* layer_tree_frame_sink);

  mojom::SyncCompositorCommonRendererParamsPtr PopulateNewCommonParams();

  // mojom::SynchronousCompositor overrides.
  void DemandDrawHwAsync(
      mojom::SyncCompositorDemandDrawHwParamsPtr draw_params) final;
  void DemandDrawHw(mojom::SyncCompositorDemandDrawHwParamsPtr params,
                    DemandDrawHwCallback callback) final;
  void SetSharedMemory(base::WritableSharedMemoryRegion shm_region,
                       SetSharedMemoryCallback callback) final;
  void DemandDrawSw(mojom::SyncCompositorDemandDrawSwParamsPtr params,
                    DemandDrawSwCallback callback) final;
  void WillSkipDraw() final;
  void ZeroSharedMemory() final;
  void ZoomBy(float zoom_delta, const gfx::Point& anchor, ZoomByCallback) final;
  void SetMemoryPolicy(uint32_t bytes_limit) final;
  void ReclaimResources(
      uint32_t layer_tree_frame_sink_id,
      const std::vector<viz::ReturnedResource>& resources) final;
  void SetScroll(const gfx::ScrollOffset& total_scroll_offset) final;
  void BeginFrame(const viz::BeginFrameArgs& args,
                  const viz::FrameTimingDetailsMap& timing_details) final;
  void SetBeginFrameSourcePaused(bool paused) final;

 protected:
  void SendAsyncRendererStateIfNeeded();
  void LayerTreeFrameSinkCreated();
  void SendBeginFrameResponse(mojom::SyncCompositorCommonRendererParamsPtr);
  void SendDemandDrawHwAsyncReply(
      mojom::SyncCompositorCommonRendererParamsPtr,
      uint32_t layer_tree_frame_sink_id,
      uint32_t metadata_version,
      base::Optional<viz::CompositorFrame>,
      base::Optional<viz::HitTestRegionList> hit_test_region_list);

  DemandDrawHwCallback hardware_draw_reply_;
  DemandDrawSwCallback software_draw_reply_;
  ZoomByCallback zoom_by_reply_;
  SynchronousLayerTreeFrameSink* layer_tree_frame_sink_ = nullptr;
  bool begin_frame_paused_ = false;

 private:
  void DoDemandDrawSw(mojom::SyncCompositorDemandDrawSwParamsPtr params);
  uint32_t NextMetadataVersion();
  void HostDisconnected();

  struct SharedMemoryWithSize;

  blink::SynchronousInputHandlerProxy* const input_handler_proxy_;
  mojo::Remote<mojom::SynchronousCompositorControlHost> control_host_;
  mojo::AssociatedRemote<mojom::SynchronousCompositorHost> host_;
  mojo::AssociatedReceiver<mojom::SynchronousCompositor> receiver_{this};
  const bool use_in_process_zero_copy_software_draw_;

  const bool viz_frame_submission_enabled_;

  bool needs_begin_frames_ = false;

  // From browser.
  std::unique_ptr<SharedMemoryWithSize> software_draw_shm_;

  // To browser.
  uint32_t version_ = 0;
  // |total_scroll_offset_| and |max_scroll_offset_| are in physical pixel when
  // use-zoom-for-dsf is enabled, otherwise in dip.
  gfx::ScrollOffset total_scroll_offset_;  // Modified by both.
  gfx::ScrollOffset max_scroll_offset_;
  gfx::SizeF scrollable_size_;
  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
  uint32_t need_invalidate_count_;
  bool invalidate_needs_draw_;
  uint32_t did_activate_pending_tree_count_;
  uint32_t metadata_version_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_SYNCHRONOUS_COMPOSITOR_PROXY_H_
