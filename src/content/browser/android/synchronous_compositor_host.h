// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_
#define CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "content/common/input/synchronous_compositor.mojom.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/common/input_event_ack_state.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_f.h"

namespace ui {
class WindowAndroid;
struct DidOverscrollParams;
}

namespace content {

class RenderProcessHost;
class RenderWidgetHostViewAndroid;
class SynchronousCompositorClient;
class SynchronousCompositorSyncCallBridge;
struct SyncCompositorCommonRendererParams;

class SynchronousCompositorHost : public SynchronousCompositor,
                                  public mojom::SynchronousCompositorHost {
 public:
  static std::unique_ptr<SynchronousCompositorHost> Create(
      RenderWidgetHostViewAndroid* rwhva);

  ~SynchronousCompositorHost() override;

  // SynchronousCompositor overrides.
  scoped_refptr<FrameFuture> DemandDrawHwAsync(
      const gfx::Size& viewport_size,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override;
  bool DemandDrawSw(SkCanvas* canvas) override;
  void ReturnResources(
      uint32_t layer_tree_frame_sink_id,
      const std::vector<viz::ReturnedResource>& resources) override;
  void DidPresentCompositorFrames(viz::PresentationFeedbackMap feedbacks,
                                  uint32_t frame_token) override;
  void SetMemoryPolicy(size_t bytes_limit) override;
  void DidBecomeActive() override;
  void DidChangeRootLayerScrollOffset(
      const gfx::ScrollOffset& root_offset) override;
  void SynchronouslyZoomBy(float zoom_delta, const gfx::Point& anchor) override;
  void OnComputeScroll(base::TimeTicks animation_time) override;

  ui::ViewAndroid::CopyViewCallback GetCopyViewCallback();
  void DidOverscroll(const ui::DidOverscrollParams& over_scroll_params);
  void BeginFrame(ui::WindowAndroid* window_android,
                  const viz::BeginFrameArgs& args,
                  const viz::PresentationFeedbackMap& feedbacks);
  void SetBeginFramePaused(bool paused);

  // Called by SynchronousCompositorSyncCallBridge.
  int routing_id() const { return routing_id_; }
  void UpdateFrameMetaData(uint32_t version,
                           viz::CompositorFrameMetadata frame_metadata);

  // Called when the mojo channel should be created.
  void InitMojo();

  SynchronousCompositorClient* client() { return client_; }

  RenderProcessHost* GetRenderProcessHost();

  // mojom::SynchronousCompositorHost overrides.
  void LayerTreeFrameSinkCreated() override;
  void UpdateState(const SyncCompositorCommonRendererParams& params) override;
  void SetNeedsBeginFrames(bool needs_begin_frames) override;

  bool on_compute_scroll_called() { return on_compute_scroll_called_; }

 private:
  class ScopedSendZeroMemory;
  struct SharedMemoryWithSize;
  friend class ScopedSetZeroMemory;
  friend class SynchronousCompositorBase;

  SynchronousCompositorHost(RenderWidgetHostViewAndroid* rwhva,
                            bool use_in_proc_software_draw);
  SynchronousCompositor::Frame DemandDrawHw(
      const gfx::Size& viewport_size,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority);
  bool DemandDrawSwInProc(SkCanvas* canvas);
  void SetSoftwareDrawSharedMemoryIfNeeded(size_t stride, size_t buffer_size);
  void SendZeroMemory();
  mojom::SynchronousCompositor* GetSynchronousCompositor();
  // Whether the synchronous compositor host is ready to
  // handle blocking calls.
  bool IsReadyForSynchronousCall();
  void UpdateRootLayerStateOnClient();
  void UpdatePresentedFrameToken(uint32_t frame_token);

  RenderWidgetHostViewAndroid* const rwhva_;
  SynchronousCompositorClient* const client_;
  const int process_id_;
  const int routing_id_;
  const bool use_in_process_zero_copy_software_draw_;
  mojom::SynchronousCompositorAssociatedPtr sync_compositor_;
  mojo::AssociatedBinding<mojom::SynchronousCompositorHost> host_binding_;

  bool registered_with_filter_ = false;

  size_t bytes_limit_;
  std::unique_ptr<SharedMemoryWithSize> software_draw_shm_;

  // Make sure to send a synchronous IPC that succeeds first before sending
  // asynchronous ones. This shouldn't be needed. However we may have come
  // to rely on sending a synchronous message first on initialization. So
  // with an abundance of caution, keep that behavior until we are sure this
  // isn't required.
  bool allow_async_draw_ = false;

  // Indicates the next draw needs to be synchronous
  bool compute_scroll_needs_synchronous_draw_ = false;

  // Indicates begin frames are paused from the browser.
  bool begin_frame_paused_ = false;

  // Updated by both renderer and browser. This is in physical pixel when
  // use-zoom-for-dsf is enabled, otherwise in dip.
  gfx::ScrollOffset root_scroll_offset_;

  // Indicates that whether OnComputeScroll is called or overridden. The
  // fling_controller should advance the fling only when OnComputeScroll is not
  // overridden.
  bool on_compute_scroll_called_ = false;

  // From renderer.
  uint32_t renderer_param_version_;
  bool need_animate_scroll_;
  uint32_t need_invalidate_count_;
  bool invalidate_needs_draw_;
  uint32_t did_activate_pending_tree_count_;
  uint32_t frame_metadata_version_ = 0u;
  // Physical pixel when use-zoom-for-dsf is enabled, otherwise in dip.
  gfx::ScrollOffset max_scroll_offset_;
  gfx::SizeF scrollable_size_;
  float page_scale_factor_ = 0.f;
  float min_page_scale_factor_ = 0.f;
  float max_page_scale_factor_ = 0.f;

  // From viz display.
  uint32_t last_frame_token_ = 0u;

  scoped_refptr<SynchronousCompositorSyncCallBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_HOST_H_
