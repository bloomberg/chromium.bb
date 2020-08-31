// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/transform_operations.h"
#include "cc/base/histograms.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_overlay_scrollbar_layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/layers/viewport.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_mask_layer_impl.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/test/test_paint_worklet_layer_painter.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/render_frame_metadata.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scroll_and_scale_set.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

#define EXPECT_SCOPED(statements) \
  {                               \
    SCOPED_TRACE("");             \
    statements;                   \
  }

using ::testing::Mock;
using ::testing::Return;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::_;
using media::VideoFrame;

namespace viz {
struct FrameTimingDetails;
}

namespace cc {
namespace {

viz::SurfaceId MakeSurfaceId(const viz::FrameSinkId& frame_sink_id,
                             uint32_t parent_id) {
  return viz::SurfaceId(
      frame_sink_id,
      viz::LocalSurfaceId(parent_id,
                          base::UnguessableToken::Deserialize(0, 1u)));
}

struct TestFrameData : public LayerTreeHostImpl::FrameData {
  TestFrameData() {
    // Set ack to something valid, so DCHECKs don't complain.
    begin_frame_ack = viz::BeginFrameAck::CreateManualAckWithDamage();
  }
};

class LayerTreeHostImplTest : public testing::Test,
                              public LayerTreeHostImplClient {
 public:
  LayerTreeHostImplTest()
      : task_runner_provider_(base::ThreadTaskRunnerHandle::Get()),
        always_main_thread_blocked_(&task_runner_provider_),
        on_can_draw_state_changed_called_(false),
        did_notify_ready_to_activate_(false),
        did_request_commit_(false),
        did_request_redraw_(false),
        did_request_next_frame_(false),
        did_request_prepare_tiles_(false),
        did_prepare_tiles_(false),
        did_complete_page_scale_animation_(false),
        reduce_memory_result_(true),
        did_request_impl_side_invalidation_(false) {
    media::InitializeMediaLibrary();
  }

  LayerTreeSettings DefaultSettings() {
    LayerListSettings settings;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    settings.enable_smooth_scroll = true;
    return settings;
  }

  // Settings with GPU rasterization disabled. New tests should test with GPU
  // rasterization enabled by default.
  LayerTreeSettings LegacySWSettings() {
    LayerTreeSettings settings = DefaultSettings();
    settings.gpu_rasterization_disabled = true;
    return settings;
  }

  void SetUp() override {
    CreateHostImpl(DefaultSettings(), CreateLayerTreeFrameSink());

    // TODO(bokan): Mac wheel scrolls don't cause smooth scrolling in the real
    // world. In tests, we force it on for consistency. Can be removed when
    // https://crbug.com/574283 is fixed.
    host_impl_->set_force_smooth_wheel_scrolling_for_testing(true);
  }

  void CreatePendingTree() {
    host_impl_->CreatePendingTree();
    LayerTreeImpl* pending_tree = host_impl_->pending_tree();
    pending_tree->SetDeviceViewportRect(
        host_impl_->active_tree()->GetDeviceViewport());
    pending_tree->SetDeviceScaleFactor(
        host_impl_->active_tree()->device_scale_factor());
    // Normally a pending tree will not be fully painted until the commit has
    // happened and any PaintWorklets have been resolved. However many of the
    // unittests never actually commit the pending trees that they create, so to
    // enable them to still treat the tree as painted we forcibly override the
    // state here. Note that this marks a distinct departure from reality in the
    // name of easier testing.
    host_impl_->set_pending_tree_fully_painted_for_testing(true);
  }

  void TearDown() override {
    if (host_impl_)
      host_impl_->ReleaseLayerTreeFrameSink();
  }

  void DidLoseLayerTreeFrameSinkOnImplThread() override {}
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  void DidReceiveCompositorFrameAckOnImplThread() override {}
  void OnCanDrawStateChanged(bool can_draw) override {
    on_can_draw_state_changed_called_ = true;
  }
  void NotifyReadyToActivate() override {
    did_notify_ready_to_activate_ = true;
    host_impl_->ActivateSyncTree();
  }
  void NotifyReadyToDraw() override {}
  void SetNeedsRedrawOnImplThread() override { did_request_redraw_ = true; }
  void SetNeedsOneBeginImplFrameOnImplThread() override {
    did_request_next_frame_ = true;
  }
  void SetNeedsPrepareTilesOnImplThread() override {
    did_request_prepare_tiles_ = true;
  }
  void SetNeedsCommitOnImplThread() override { did_request_commit_ = true; }
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override {}
  bool IsInsideDraw() override { return false; }
  bool IsBeginMainFrameExpected() override { return true; }
  void RenewTreePriority() override {}
  void PostDelayedAnimationTaskOnImplThread(base::OnceClosure task,
                                            base::TimeDelta delay) override {
    animation_task_ = std::move(task);
    requested_animation_delay_ = delay;
  }
  void DidActivateSyncTree() override {
    // Make sure the active tree always has a valid LocalSurfaceId.
    host_impl_->active_tree()->SetLocalSurfaceIdAllocationFromParent(
        viz::LocalSurfaceIdAllocation(
            viz::LocalSurfaceId(1, base::UnguessableToken::Deserialize(2u, 3u)),
            base::TimeTicks::Now()));
  }
  void WillPrepareTiles() override {}
  void DidPrepareTiles() override { did_prepare_tiles_ = true; }
  void DidCompletePageScaleAnimationOnImplThread() override {
    did_complete_page_scale_animation_ = true;
  }
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw) override {
    std::unique_ptr<TestFrameData> frame(new TestFrameData);
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(frame.get()));
    last_on_draw_render_passes_.clear();
    viz::RenderPass::CopyAll(frame->render_passes,
                             &last_on_draw_render_passes_);
    host_impl_->DrawLayers(frame.get());
    host_impl_->DidDrawAllLayers(*frame);
    last_on_draw_frame_ = std::move(frame);
  }
  void NeedsImplSideInvalidation(bool needs_first_draw_on_activation) override {
    did_request_impl_side_invalidation_ = true;
  }
  void NotifyImageDecodeRequestFinished() override {}
  void RequestBeginMainFrameNotExpected(bool new_state) override {}
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
      const viz::FrameTimingDetails& details) override {}
  void NotifyAnimationWorkletStateChange(AnimationWorkletMutationState state,
                                         ElementListType tree_type) override {}
  void NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState state) override {}
  void NotifyThroughputTrackerResults(CustomTrackerResults results) override {}

  void set_reduce_memory_result(bool reduce_memory_result) {
    reduce_memory_result_ = reduce_memory_result;
  }

  AnimationHost* GetImplAnimationHost() const {
    return static_cast<AnimationHost*>(host_impl_->mutator_host());
  }

  virtual bool CreateHostImpl(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) {
    if (host_impl_)
      host_impl_->ReleaseLayerTreeFrameSink();
    host_impl_.reset();
    InitializeImageWorker(settings);
    host_impl_ = LayerTreeHostImpl::Create(
        settings, this, &task_runner_provider_, &stats_instrumentation_,
        &task_graph_runner_,
        AnimationHost::CreateForTesting(ThreadInstance::IMPL), 0,
        image_worker_ ? image_worker_->task_runner() : nullptr, nullptr);
    layer_tree_frame_sink_ = std::move(layer_tree_frame_sink);
    host_impl_->SetVisible(true);
    bool init = host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
    host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(10, 10));
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 1);
    host_impl_->active_tree()->SetLocalSurfaceIdAllocationFromParent(
        viz::LocalSurfaceIdAllocation(
            viz::LocalSurfaceId(1, base::UnguessableToken::Deserialize(2u, 3u)),
            base::TimeTicks::Now()));
    // Set the viz::BeginFrameArgs so that methods which use it are able to.
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 1,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    host_impl_->DidFinishImplFrame(args);

    timeline_ =
        AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
    GetImplAnimationHost()->AddAnimationTimeline(timeline_);

    return init;
  }

  template <typename T, typename... Args>
  T* SetupRootLayer(LayerTreeImpl* layer_tree_impl,
                    const gfx::Size& viewport_size,
                    Args&&... args) {
    const int kRootLayerId = 1;
    DCHECK(!layer_tree_impl->root_layer());
    DCHECK(!layer_tree_impl->LayerById(kRootLayerId));
    layer_tree_impl->SetRootLayerForTesting(
        T::Create(layer_tree_impl, kRootLayerId, std::forward<Args>(args)...));
    auto* root = layer_tree_impl->root_layer();
    root->SetBounds(viewport_size);
    layer_tree_impl->SetDeviceViewportRect(
        gfx::Rect(DipSizeToPixelSize(viewport_size)));
    SetupRootProperties(root);
    return static_cast<T*>(root);
  }

  LayerImpl* SetupDefaultRootLayer(const gfx::Size& viewport_size) {
    return SetupRootLayer<LayerImpl>(host_impl_->active_tree(), viewport_size);
  }

  LayerImpl* root_layer() { return host_impl_->active_tree()->root_layer(); }

  gfx::Size DipSizeToPixelSize(const gfx::Size& size) {
    return gfx::ScaleToRoundedSize(
        size, host_impl_->active_tree()->device_scale_factor());
  }

  static gfx::Vector2dF ScrollDelta(LayerImpl* layer_impl) {
    gfx::ScrollOffset delta = layer_impl->layer_tree_impl()
                                  ->property_trees()
                                  ->scroll_tree.GetScrollOffsetDeltaForTesting(
                                      layer_impl->element_id());
    return gfx::Vector2dF(delta.x(), delta.y());
  }

  static void ExpectClearedScrollDeltasRecursive(LayerImpl* root) {
    for (auto* layer : *root->layer_tree_impl())
      ASSERT_EQ(ScrollDelta(layer), gfx::Vector2d());
  }

  static ::testing::AssertionResult ScrollInfoContains(
      const ScrollAndScaleSet& scroll_info,
      ElementId id,
      const gfx::ScrollOffset& scroll_delta) {
    int times_encountered = 0;

    for (size_t i = 0; i < scroll_info.scrolls.size(); ++i) {
      if (scroll_info.scrolls[i].element_id != id)
        continue;

      if (scroll_delta != scroll_info.scrolls[i].scroll_delta) {
        return ::testing::AssertionFailure()
               << "Expected " << scroll_delta.ToString() << ", not "
               << scroll_info.scrolls[i].scroll_delta.ToString();
      }
      times_encountered++;
    }

    if (id == scroll_info.inner_viewport_scroll.element_id) {
      if (scroll_delta != scroll_info.inner_viewport_scroll.scroll_delta) {
        return ::testing::AssertionFailure()
               << "Expected " << scroll_delta.ToString() << ", not "
               << scroll_info.inner_viewport_scroll.scroll_delta.ToString();
      }
      times_encountered++;
    }

    if (times_encountered != 1)
      return ::testing::AssertionFailure() << "No scroll found with id " << id;
    return ::testing::AssertionSuccess();
  }

  static void ExpectNone(const ScrollAndScaleSet& scroll_info, ElementId id) {
    int times_encountered = 0;

    for (size_t i = 0; i < scroll_info.scrolls.size(); ++i) {
      if (scroll_info.scrolls[i].element_id != id)
        continue;
      times_encountered++;
    }

    ASSERT_EQ(0, times_encountered);
  }

  template <typename T, typename... Args>
  T* AddLayer(LayerTreeImpl* layer_tree_impl, Args&&... args) {
    std::unique_ptr<T> layer = T::Create(layer_tree_impl, next_layer_id_++,
                                         std::forward<Args>(args)...);
    T* result = layer.get();
    layer_tree_impl->AddLayer(std::move(layer));
    return result;
  }

  LayerImpl* AddLayer() {
    return AddLayer<LayerImpl>(host_impl_->active_tree());
  }

  void SetupViewportLayers(LayerTreeImpl* layer_tree_impl,
                           const gfx::Size& inner_viewport_size,
                           const gfx::Size& outer_viewport_size,
                           const gfx::Size& content_size) {
    DCHECK(!layer_tree_impl->root_layer());
    auto* root =
        SetupRootLayer<LayerImpl>(layer_tree_impl, inner_viewport_size);
    SetupViewport(root, outer_viewport_size, content_size);

    UpdateDrawProperties(layer_tree_impl);
    layer_tree_impl->DidBecomeActive();
  }

  // Calls SetupViewportLayers() passing |content_size| as that function's
  // |outer_viewport_size| and |content_size|, which makes the outer viewport
  // not scrollable, and scrolls will be applied directly to the inner viewport.
  void SetupViewportLayersInnerScrolls(const gfx::Size& inner_viewport_size,
                                       const gfx::Size& content_size) {
    const auto& outer_viewport_size = content_size;
    SetupViewportLayers(host_impl_->active_tree(), inner_viewport_size,
                        outer_viewport_size, content_size);
  }

  // Calls SetupViewportLayers() passing |viewport_size| as that function's
  // |inner_viewport_size| and |outer_viewport_size|, which makes the inner
  // viewport not scrollable, and scrolls will be applied to the outer
  // viewport only.
  void SetupViewportLayersOuterScrolls(const gfx::Size& viewport_size,
                                       const gfx::Size& content_size) {
    SetupViewportLayers(host_impl_->active_tree(), viewport_size, viewport_size,
                        content_size);
  }

  LayerImpl* AddContentLayer() {
    LayerImpl* scroll_layer = OuterViewportScrollLayer();
    DCHECK(scroll_layer);
    LayerImpl* layer = AddLayer();
    layer->SetBounds(scroll_layer->bounds());
    layer->SetDrawsContent(true);
    CopyProperties(scroll_layer, layer);
    return layer;
  }

  // Calls SetupViewportLayers() with the same |viewport_bounds|,
  // |inner_scroll_bounds| and |outer_scroll_bounds|, which makes neither
  // of inner viewport and outer viewport scrollable.
  void SetupViewportLayersNoScrolls(const gfx::Size& bounds) {
    SetupViewportLayers(host_impl_->active_tree(), bounds, bounds, bounds);
  }

  void CreateAndTestNonScrollableLayers(bool transparent_layer) {
    LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
    gfx::Size content_size = gfx::Size(360, 600);
    gfx::Size scroll_content_size = gfx::Size(345, 3800);
    gfx::Size scrollbar_size = gfx::Size(15, 600);

    SetupViewportLayersNoScrolls(content_size);
    LayerImpl* outer_scroll = OuterViewportScrollLayer();
    LayerImpl* scroll =
        AddScrollableLayer(outer_scroll, content_size, scroll_content_size);

    auto* squash2 = AddLayer<LayerImpl>(layer_tree_impl);
    squash2->SetBounds(gfx::Size(140, 300));
    squash2->SetDrawsContent(true);
    squash2->SetHitTestable(true);
    CopyProperties(scroll, squash2);
    squash2->SetOffsetToTransformParent(gfx::Vector2dF(220, 300));

    auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
        layer_tree_impl, VERTICAL, false, true);
    SetupScrollbarLayer(scroll, scrollbar);
    scrollbar->SetBounds(scrollbar_size);
    scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

    auto* squash1 = AddLayer<LayerImpl>(layer_tree_impl);
    squash1->SetBounds(gfx::Size(140, 300));
    CopyProperties(outer_scroll, squash1);
    squash1->SetOffsetToTransformParent(gfx::Vector2dF(220, 0));
    if (transparent_layer) {
      CreateEffectNode(squash1).opacity = 0.0f;
      // The transparent layer should still participate in hit testing even
      // through it does not draw content.
      squash1->SetHitTestable(true);
    } else {
      squash1->SetDrawsContent(true);
      squash1->SetHitTestable(true);
    }

    UpdateDrawProperties(layer_tree_impl);
    layer_tree_impl->DidBecomeActive();

    // The point hits squash1 layer and also scroll layer, because scroll layer
    // is not an ancestor of squash1 layer, we cannot scroll on impl thread.
    InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
        BeginState(gfx::Point(230, 150), gfx::Vector2dF(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    ASSERT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
    ASSERT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_scrolling_reasons);

    // The point hits squash1 layer and also scrollbar layer.
    status = host_impl_->ScrollBegin(
        BeginState(gfx::Point(350, 150), gfx::Vector2dF(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    ASSERT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
    ASSERT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_scrolling_reasons);

    // The point hits squash2 layer and also scroll layer, because scroll layer
    // is an ancestor of squash2 layer, we should scroll on impl.
    status = host_impl_->ScrollBegin(
        BeginState(gfx::Point(230, 450), gfx::Vector2dF(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    ASSERT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  }

  LayerImpl* AddScrollableLayer(LayerImpl* container,
                                const gfx::Size& scroll_container_bounds,
                                const gfx::Size& content_size) {
    LayerImpl* layer = AddLayer<LayerImpl>(container->layer_tree_impl());
    layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
    layer->SetDrawsContent(true);
    layer->SetBounds(content_size);
    layer->SetHitTestable(true);
    CopyProperties(container, layer);
    CreateTransformNode(layer);
    CreateScrollNode(layer, scroll_container_bounds);
    return layer;
  }

  void SetupScrollbarLayerCommon(LayerImpl* scroll_layer,
                                 ScrollbarLayerImplBase* scrollbar) {
    auto* tree_impl = scroll_layer->layer_tree_impl();
    scrollbar->SetScrollElementId(scroll_layer->element_id());
    scrollbar->SetDrawsContent(true);
    CopyProperties(scroll_layer, scrollbar);
    if (scroll_layer == tree_impl->OuterViewportScrollLayerForTesting()) {
      scrollbar->SetTransformTreeIndex(tree_impl->PageScaleTransformNode()->id);
      scrollbar->SetScrollTreeIndex(
          tree_impl->root_layer()->scroll_tree_index());
    } else {
      scrollbar->SetTransformTreeIndex(
          GetTransformNode(scroll_layer)->parent_id);
      scrollbar->SetScrollTreeIndex(GetScrollNode(scroll_layer)->parent_id);
    }
  }

  void SetupScrollbarLayer(LayerImpl* scroll_layer,
                           SolidColorScrollbarLayerImpl* scrollbar) {
    scrollbar->SetElementId(LayerIdToElementIdForTesting(scrollbar->id()));
    SetupScrollbarLayerCommon(scroll_layer, scrollbar);
    auto& effect = CreateEffectNode(scrollbar);
    effect.opacity = 0;
    effect.has_potential_opacity_animation = true;
  }

  void SetupScrollbarLayer(LayerImpl* scroll_layer,
                           PaintedScrollbarLayerImpl* scrollbar) {
    SetupScrollbarLayerCommon(scroll_layer, scrollbar);
    scrollbar->SetHitTestable(true);
    CreateEffectNode(scrollbar).opacity = 1;
  }

  LayerImpl* InnerViewportScrollLayer() {
    return host_impl_->active_tree()->InnerViewportScrollLayerForTesting();
  }
  LayerImpl* OuterViewportScrollLayer() {
    return host_impl_->active_tree()->OuterViewportScrollLayerForTesting();
  }

  std::unique_ptr<ScrollState> BeginState(const gfx::Point& point,
                                          const gfx::Vector2dF& delta_hint,
                                          ui::ScrollInputType type) {
    ScrollStateData scroll_state_data;
    scroll_state_data.is_beginning = true;
    scroll_state_data.position_x = point.x();
    scroll_state_data.position_y = point.y();
    scroll_state_data.delta_x_hint = delta_hint.x();
    scroll_state_data.delta_y_hint = delta_hint.y();
    scroll_state_data.is_direct_manipulation =
        type == ui::ScrollInputType::kTouchscreen;
    std::unique_ptr<ScrollState> scroll_state(
        new ScrollState(scroll_state_data));
    return scroll_state;
  }

  std::unique_ptr<ScrollState> UpdateState(const gfx::Point& point,
                                           const gfx::Vector2dF& delta,
                                           ui::ScrollInputType type) {
    ScrollStateData scroll_state_data;
    scroll_state_data.delta_x = delta.x();
    scroll_state_data.delta_y = delta.y();
    scroll_state_data.position_x = point.x();
    scroll_state_data.position_y = point.y();
    scroll_state_data.is_direct_manipulation =
        type == ui::ScrollInputType::kTouchscreen;
    std::unique_ptr<ScrollState> scroll_state(
        new ScrollState(scroll_state_data));
    return scroll_state;
  }

  std::unique_ptr<ScrollState> AnimatedUpdateState(
      const gfx::Point& point,
      const gfx::Vector2dF& delta) {
    auto state = UpdateState(point, delta, ui::ScrollInputType::kWheel);
    state->data()->delta_granularity = ui::ScrollGranularity::kScrollByPixel;
    return state;
  }

  void DrawFrame() {
    PrepareForUpdateDrawProperties(host_impl_->active_tree());
    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }

  RenderFrameMetadata StartDrawAndProduceRenderFrameMetadata() {
    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    return host_impl_->MakeRenderFrameMetadata(&frame);
  }

  void TestGPUMemoryForTilings(const gfx::Size& layer_size) {
    std::unique_ptr<FakeRecordingSource> recording_source =
        FakeRecordingSource::CreateFilledRecordingSource(layer_size);
    PaintImage checkerable_image =
        CreateDiscardablePaintImage(gfx::Size(500, 500));
    recording_source->add_draw_image(checkerable_image, gfx::Point(0, 0));

    recording_source->Rerecord();
    scoped_refptr<FakeRasterSource> raster_source =
        FakeRasterSource::CreateFromRecordingSource(recording_source.get());

    // Create the pending tree.
    host_impl_->BeginCommit();
    LayerTreeImpl* pending_tree = host_impl_->pending_tree();
    LayerImpl* root = SetupRootLayer<FakePictureLayerImpl>(
        pending_tree, layer_size, raster_source);
    root->SetDrawsContent(true);
    UpdateDrawProperties(pending_tree);

    // CompleteCommit which should perform a PrepareTiles, adding tilings for
    // the root layer, each one having a raster task.
    host_impl_->CommitComplete();
    // Activate the pending tree and ensure that all tiles are rasterized.
    while (!did_notify_ready_to_activate_)
      base::RunLoop().RunUntilIdle();

    DrawFrame();

    host_impl_->ReleaseLayerTreeFrameSink();
    host_impl_ = nullptr;
  }

  void WhiteListedTouchActionTestHelper(float device_scale_factor,
                                        float page_scale_factor) {
    SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
    DrawFrame();

    // Just hard code some random number, we care about the actual page scale
    // factor on the active tree.
    float min_page_scale_factor = 0.1f;
    float max_page_scale_factor = 5.0f;
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale_factor, max_page_scale_factor);
    host_impl_->active_tree()->SetDeviceScaleFactor(device_scale_factor);

    LayerImpl* child = AddLayer();
    child->SetDrawsContent(true);
    child->SetBounds(gfx::Size(25, 25));
    CopyProperties(InnerViewportScrollLayer(), child);

    TouchActionRegion root_touch_action_region;
    root_touch_action_region.Union(TouchAction::kPanX, gfx::Rect(0, 0, 50, 50));
    root_layer()->SetTouchActionRegion(root_touch_action_region);
    TouchActionRegion child_touch_action_region;
    child_touch_action_region.Union(TouchAction::kPanLeft,
                                    gfx::Rect(0, 0, 25, 25));
    child->SetTouchActionRegion(child_touch_action_region);

    TouchAction touch_action = TouchAction::kAuto;
    host_impl_->EventListenerTypeForTouchStartOrMoveAt(gfx::Point(10, 10),
                                                       &touch_action);
    EXPECT_EQ(TouchAction::kPanLeft, touch_action);
    touch_action = TouchAction::kAuto;
    host_impl_->EventListenerTypeForTouchStartOrMoveAt(gfx::Point(30, 30),
                                                       &touch_action);
    EXPECT_EQ(TouchAction::kPanX, touch_action);

    TouchActionRegion new_child_region;
    new_child_region.Union(TouchAction::kPanY, gfx::Rect(0, 0, 25, 25));
    child->SetTouchActionRegion(new_child_region);
    touch_action = TouchAction::kAuto;
    host_impl_->EventListenerTypeForTouchStartOrMoveAt(gfx::Point(10, 10),
                                                       &touch_action);
    EXPECT_EQ(TouchAction::kPanY, touch_action);
    touch_action = TouchAction::kAuto;
    host_impl_->EventListenerTypeForTouchStartOrMoveAt(gfx::Point(30, 30),
                                                       &touch_action);
    EXPECT_EQ(TouchAction::kPanX, touch_action);
  }

  LayerImpl* CreateLayerForSnapping() {
    SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

    gfx::Size overflow_size(400, 400);
    LayerImpl* overflow = AddScrollableLayer(
        OuterViewportScrollLayer(), gfx::Size(100, 100), overflow_size);
    SnapContainerData container_data(
        ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
        gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(300, 300));
    SnapAreaData area_data(ScrollSnapAlign(SnapAlignment::kStart),
                           gfx::RectF(50, 50, 100, 100), false, ElementId(10));
    container_data.AddSnapAreaData(area_data);
    GetScrollNode(overflow)->snap_container_data.emplace(container_data);
    DrawFrame();

    return overflow;
  }

  base::Optional<SnapContainerData> GetSnapContainerData(LayerImpl* layer) {
    return GetScrollNode(layer) ? GetScrollNode(layer)->snap_container_data
                                : base::nullopt;
  }

  void ClearLayersAndPropertyTrees(LayerTreeImpl* layer_tree_impl) {
    layer_tree_impl->SetRootLayerForTesting(nullptr);
    layer_tree_impl->DetachLayers();
    layer_tree_impl->property_trees()->clear();
    layer_tree_impl->SetViewportPropertyIds(
        LayerTreeImpl::ViewportPropertyIds());
  }

  void pinch_zoom_pan_viewport_forces_commit_redraw(float device_scale_factor);
  void pinch_zoom_pan_viewport_test(float device_scale_factor);
  void pinch_zoom_pan_viewport_and_scroll_test(float device_scale_factor);
  void pinch_zoom_pan_viewport_and_scroll_boundary_test(
      float device_scale_factor);

  void SetupMouseMoveAtWithDeviceScale(float device_scale_factor);

  void SetupMouseMoveAtTestScrollbarStates(bool main_thread_scrolling);

  scoped_refptr<AnimationTimeline> timeline() { return timeline_; }

 protected:
  virtual std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() {
    return FakeLayerTreeFrameSink::Create3dForGpuRasterization();
  }

  void DrawOneFrame() {
    PrepareForUpdateDrawProperties(host_impl_->active_tree());
    TestFrameData frame_data;
    host_impl_->PrepareToDraw(&frame_data);
    host_impl_->DidDrawAllLayers(frame_data);
  }

  static void SetScrollOffsetDelta(LayerImpl* layer_impl,
                                   const gfx::Vector2dF& delta) {
    if (layer_impl->layer_tree_impl()
            ->property_trees()
            ->scroll_tree.SetScrollOffsetDeltaForTesting(
                layer_impl->element_id(), delta))
      layer_impl->layer_tree_impl()->DidUpdateScrollOffset(
          layer_impl->element_id());
  }

  void BeginImplFrameAndAnimate(viz::BeginFrameArgs begin_frame_args,
                                base::TimeTicks frame_time) {
    begin_frame_args.frame_time = frame_time;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }

  void InitializeImageWorker(const LayerTreeSettings& settings) {
    if (settings.enable_checker_imaging) {
      image_worker_ = std::make_unique<base::Thread>("ImageWorker");
      ASSERT_TRUE(image_worker_->Start());
    } else {
      image_worker_.reset();
    }
  }

  FakeImplTaskRunnerProvider task_runner_provider_;
  DebugScopedSetMainThreadBlocked always_main_thread_blocked_;

  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink_;
  std::unique_ptr<LayerTreeHostImpl> host_impl_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  bool on_can_draw_state_changed_called_;
  bool did_notify_ready_to_activate_;
  bool did_request_commit_;
  bool did_request_redraw_;
  bool did_request_next_frame_;
  bool did_request_prepare_tiles_;
  bool did_prepare_tiles_;
  bool did_complete_page_scale_animation_;
  bool reduce_memory_result_;
  bool did_request_impl_side_invalidation_;
  base::OnceClosure animation_task_;
  base::TimeDelta requested_animation_delay_;
  std::unique_ptr<TestFrameData> last_on_draw_frame_;
  viz::RenderPassList last_on_draw_render_passes_;
  scoped_refptr<AnimationTimeline> timeline_;
  std::unique_ptr<base::Thread> image_worker_;
  int next_layer_id_ = 2;
};

class CommitToPendingTreeLayerTreeHostImplTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    LayerTreeSettings settings = DefaultSettings();
    settings.commit_to_active_tree = false;
    CreateHostImpl(settings, CreateLayerTreeFrameSink());
  }
};

// A test fixture for new animation timelines tests.
class LayerTreeHostImplTimelinesTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    CreateHostImpl(DefaultSettings(), CreateLayerTreeFrameSink());

    // TODO(bokan): Mac wheel scrolls don't cause smooth scrolling in the real
    // world. In tests, we force it on for consistency. Can be removed when
    // https://crbug.com/574283 is fixed.
    host_impl_->set_force_smooth_wheel_scrolling_for_testing(true);
  }
};

class TestInputHandlerClient : public InputHandlerClient {
 public:
  TestInputHandlerClient()
      : page_scale_factor_(0),
        min_page_scale_factor_(-1),
        max_page_scale_factor_(-1) {}
  ~TestInputHandlerClient() override = default;

  // InputHandlerClient implementation.
  void WillShutdown() override {}
  void Animate(base::TimeTicks time) override {}
  void ReconcileElasticOverscrollAndRootScroll() override {}
  void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::ScrollOffset& total_scroll_offset,
      const gfx::ScrollOffset& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) override {
    DCHECK(total_scroll_offset.x() <= max_scroll_offset.x());
    DCHECK(total_scroll_offset.y() <= max_scroll_offset.y());
    last_set_scroll_offset_ = total_scroll_offset;
    max_scroll_offset_ = max_scroll_offset;
    scrollable_size_ = scrollable_size;
    page_scale_factor_ = page_scale_factor;
    min_page_scale_factor_ = min_page_scale_factor;
    max_page_scale_factor_ = max_page_scale_factor;
  }
  void DeliverInputForBeginFrame(const viz::BeginFrameArgs& args) override {}
  void DeliverInputForHighLatencyMode() override {}

  gfx::ScrollOffset last_set_scroll_offset() { return last_set_scroll_offset_; }

  gfx::ScrollOffset max_scroll_offset() const { return max_scroll_offset_; }

  gfx::SizeF scrollable_size() const { return scrollable_size_; }

  float page_scale_factor() const { return page_scale_factor_; }

  float min_page_scale_factor() const { return min_page_scale_factor_; }

  float max_page_scale_factor() const { return max_page_scale_factor_; }

 private:
  gfx::ScrollOffset last_set_scroll_offset_;
  gfx::ScrollOffset max_scroll_offset_;
  gfx::SizeF scrollable_size_;
  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
};

TEST_F(LayerTreeHostImplTest, LocalAndExternalPinchState) {
  // PinchGestureBegin/End update pinch_gesture_active() properly.
  EXPECT_FALSE(host_impl_->pinch_gesture_active());
  host_impl_->PinchGestureBegin();
  EXPECT_TRUE(host_impl_->pinch_gesture_active());
  host_impl_->PinchGestureEnd(gfx::Point(), false /* snap_to_min */);
  EXPECT_FALSE(host_impl_->pinch_gesture_active());

  // set_external_pinch_gesture_active updates pinch_gesture_active() properly.
  host_impl_->set_external_pinch_gesture_active(true);
  EXPECT_TRUE(host_impl_->pinch_gesture_active());
  host_impl_->set_external_pinch_gesture_active(false);
  EXPECT_FALSE(host_impl_->pinch_gesture_active());

  // Clearing external_pinch_gesture_active doesn't affect
  // pinch_gesture_active() if it was set by PinchGestureBegin().
  host_impl_->PinchGestureBegin();
  EXPECT_TRUE(host_impl_->pinch_gesture_active());
  host_impl_->set_external_pinch_gesture_active(false);
  EXPECT_TRUE(host_impl_->pinch_gesture_active());
  host_impl_->PinchGestureEnd(gfx::Point(), false /* snap_to_min */);
  EXPECT_FALSE(host_impl_->pinch_gesture_active());
}

TEST_F(LayerTreeHostImplTest, NotifyIfCanDrawChanged) {
  // Note: It is not possible to disable the renderer once it has been set,
  // so we do not need to test that disabling the renderer notifies us
  // that can_draw changed.
  EXPECT_FALSE(host_impl_->CanDraw());
  on_can_draw_state_changed_called_ = false;

  // Set up the root layer, which allows us to draw.
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  // Toggle the root layer to make sure it toggles can_draw
  ClearLayersAndPropertyTrees(host_impl_->active_tree());
  EXPECT_FALSE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  // Toggle the device viewport size to make sure it toggles can_draw.
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect());
  EXPECT_FALSE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;
}

TEST_F(LayerTreeHostImplTest, ResourcelessDrawWithEmptyViewport) {
  CreateHostImpl(DefaultSettings(), FakeLayerTreeFrameSink::CreateSoftware());
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

  EXPECT_TRUE(host_impl_->CanDraw());
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect());
  EXPECT_FALSE(host_impl_->CanDraw());

  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());
  EXPECT_EQ(fake_layer_tree_frame_sink->num_sent_frames(), 0u);
  gfx::Transform identity;
  gfx::Rect viewport(100, 100);
  const bool resourceless_software_draw = true;
  host_impl_->OnDraw(identity, viewport, resourceless_software_draw, false);
  ASSERT_EQ(fake_layer_tree_frame_sink->num_sent_frames(), 1u);
}

TEST_F(LayerTreeHostImplTest, ScrollDeltaNoLayers) {
  host_impl_->active_tree()->SetRootLayerForTesting(nullptr);

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 0u);
}

TEST_F(LayerTreeHostImplTest, ScrollDeltaTreeButNoChanges) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(10, 10));
  {
    LayerImpl* child1 = AddLayer();
    CopyProperties(root, child1);
    LayerImpl* child2 = AddLayer();
    CopyProperties(root, child2);
    LayerImpl* grand_child1 = AddLayer();
    CopyProperties(child2, grand_child1);
    LayerImpl* grand_child2 = AddLayer();
    CopyProperties(child2, grand_child2);
    LayerImpl* great_grand_child = AddLayer();
    CopyProperties(grand_child1, great_grand_child);
  }

  ExpectClearedScrollDeltasRecursive(root);

  std::unique_ptr<ScrollAndScaleSet> scroll_info;

  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 0u);
  ExpectClearedScrollDeltasRecursive(root);

  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 0u);
  ExpectClearedScrollDeltasRecursive(root);
}

TEST_F(LayerTreeHostImplTest, ScrollDeltaRepeatedScrolls) {
  gfx::ScrollOffset scroll_offset(20, 30);
  gfx::ScrollOffset scroll_delta(11, -15);

  auto* root = SetupDefaultRootLayer(gfx::Size(110, 110));
  root->SetHitTestable(true);
  CreateScrollNode(root, gfx::Size(10, 10));
  root->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(root->element_id(),
                                                     scroll_offset);
  UpdateDrawProperties(host_impl_->active_tree());

  std::unique_ptr<ScrollAndScaleSet> scroll_info;

  root->ScrollBy(gfx::ScrollOffsetToVector2dF(scroll_delta));
  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 1u);
  EXPECT_TRUE(
      ScrollInfoContains(*scroll_info, root->element_id(), scroll_delta));

  gfx::ScrollOffset scroll_delta2(-5, 27);
  root->ScrollBy(gfx::ScrollOffsetToVector2dF(scroll_delta2));
  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 1u);
  EXPECT_TRUE(ScrollInfoContains(*scroll_info, root->element_id(),
                                 scroll_delta + scroll_delta2));

  root->ScrollBy(gfx::Vector2d());
  scroll_info = host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info, root->element_id(),
                                 scroll_delta + scroll_delta2));
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       GPUMemoryForSmallLayerHistogramTest) {
  base::HistogramTester histogram_tester;
  SetClientNameForMetrics("Renderer");
  // With default tile size being set to 256 * 256, the following layer needs
  // one tile only which costs 256 * 256 * 4 / 1024 = 256KB memory.
  TestGPUMemoryForTilings(gfx::Size(200, 200));
  histogram_tester.ExpectBucketCount(
      "Compositing.Renderer.GPUMemoryForTilingsInKb", 256, 1);
  histogram_tester.ExpectTotalCount(
      "Compositing.Renderer.GPUMemoryForTilingsInKb", 1);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       GPUMemoryForLargeLayerHistogramTest) {
  base::HistogramTester histogram_tester;
  SetClientNameForMetrics("Renderer");
  // With default tile size being set to 256 * 256, the following layer needs
  // 4 tiles which cost 256 * 256 * 4 * 4 / 1024 = 1024KB memory.
  TestGPUMemoryForTilings(gfx::Size(500, 500));
  histogram_tester.ExpectBucketCount(
      "Compositing.Renderer.GPUMemoryForTilingsInKb", 1024, 1);
  histogram_tester.ExpectTotalCount(
      "Compositing.Renderer.GPUMemoryForTilingsInKb", 1);
}

TEST_F(LayerTreeHostImplTest, ScrollBeforeRootLayerAttached) {
  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 1),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);

  status =
      host_impl_->RootScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 1),
                                             ui::ScrollInputType::kWheel)
                                      .get(),
                                  ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);
}

// Tests that receiving ScrollUpdate and ScrollEnd calls that don't have a
// matching ScrollBegin are just dropped and are a no-op. This can happen due
// to pre-commit input deferral which causes some input events to be dropped
// before the first commit in a renderer has occurred. See the flag
// kAllowPreCommitInput and how it's used.
TEST_F(LayerTreeHostImplTest, ScrollUpdateAndEndNoOpWithoutBegin) {
  SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(1000, 1000));

  // Simulate receiving a gesture mid-stream so that the Begin wasn't ever
  // processed. This shouldn't cause any state change but we should crash or
  // DCHECK.
  {
    EXPECT_FALSE(host_impl_->CurrentlyScrollingNode());
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    EXPECT_FALSE(host_impl_->CurrentlyScrollingNode());
    host_impl_->ScrollEnd();
  }

  // Ensure a new gesture is now able to correctly scroll.
  {
    InputHandler::ScrollStatus status =
        host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
    EXPECT_TRUE(host_impl_->CurrentlyScrollingNode());

    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 10),
                     CurrentScrollOffset(OuterViewportScrollLayer()));

    host_impl_->ScrollEnd();
  }
}

// Test that specifying a scroller to ScrollBegin (i.e. avoid hit testing)
// returns the correct status if the scroller cannot be scrolled on the
// compositor thread.
TEST_F(LayerTreeHostImplTest, TargetMainThreadScroller) {
  SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(1000, 1000));

  ScrollStateData scroll_state_data;
  scroll_state_data.set_current_native_scrolling_element(
      host_impl_->OuterViewportScrollNode()->element_id);
  std::unique_ptr<ScrollState> scroll_state(new ScrollState(scroll_state_data));

  // Try on a node that should scroll on the compositor. Confirm it works.
  {
    InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
        scroll_state.get(), ui::ScrollInputType::kWheel);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
    EXPECT_EQ(host_impl_->OuterViewportScrollNode(),
              host_impl_->CurrentlyScrollingNode());
    host_impl_->ScrollEnd();
  }

  // Now add a MainThreadScrollingReason. Trying to target the node this time
  // should result in a failed ScrollBegin with the appropriate return value.
  host_impl_->OuterViewportScrollNode()->main_thread_scrolling_reasons =
      MainThreadScrollingReason::kThreadedScrollingDisabled;

  {
    InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
        scroll_state.get(), ui::ScrollInputType::kWheel);
    EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
    EXPECT_EQ(
        host_impl_->OuterViewportScrollNode()->main_thread_scrolling_reasons,
        status.main_thread_scrolling_reasons);
    EXPECT_FALSE(host_impl_->CurrentlyScrollingNode());
  }
}

TEST_F(LayerTreeHostImplTest, ScrollRootCallsCommitAndRedraw) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);

  EXPECT_TRUE(host_impl_->CurrentlyScrollingNode());
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  host_impl_->ScrollEnd();
  EXPECT_FALSE(host_impl_->CurrentlyScrollingNode());
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);
}

// Ensure correct semantics for the IsActivelyPrecisionScrolling method. This
// method is used to determine scheduler policy so it wants to report true only
// when real scrolling is occurring (i.e. the compositor is consuming scroll
// delta, the page isn't handling the events itself). We also only consider
// this signal for non-animated scrolls. This is partially historical but also
// makes some sense since touchscreen/high-precision touchpad scrolling has a
// physical metaphor (movement sticks to finger) so smoothness should be
// prioritized.
TEST_F(LayerTreeHostImplTest, ActivelyTouchScrollingOnlyAfterScrollMovement) {
  SetupViewportLayersOuterScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  // Ensure a touch scroll reports true but only after some delta has been
  // consumed.
  {
    InputHandler::ScrollStatus status =
        host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_scrolling_reasons);
    EXPECT_FALSE(host_impl_->IsActivelyPrecisionScrolling());

    // There is no extent upwards so the scroll won't consume any delta.
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -10),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    EXPECT_FALSE(host_impl_->IsActivelyPrecisionScrolling());

    // This should scroll so ensure the bit flips to true.
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    EXPECT_TRUE(host_impl_->IsActivelyPrecisionScrolling());
    host_impl_->ScrollEnd();
    EXPECT_FALSE(host_impl_->IsActivelyPrecisionScrolling());
  }

  ASSERT_EQ(10, CurrentScrollOffset(OuterViewportScrollLayer()).y());

  // Ensure an animated wheel scroll doesn't cause the bit to flip even when
  // scrolling occurs.
  {
    InputHandler::ScrollStatus status =
        host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                           ui::ScrollInputType::kWheel)
                                    .get(),
                                ui::ScrollInputType::kWheel);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
    EXPECT_FALSE(host_impl_->IsActivelyPrecisionScrolling());

    host_impl_->ScrollUpdate(
        AnimatedUpdateState(gfx::Point(), gfx::Vector2dF(0, 10)).get());
    EXPECT_FALSE(host_impl_->IsActivelyPrecisionScrolling());

    base::TimeTicks cur_time =
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
    viz::BeginFrameArgs begin_frame_args =
        viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

#define ANIMATE(time_ms)                                  \
  cur_time += base::TimeDelta::FromMilliseconds(time_ms); \
  begin_frame_args.frame_time = (cur_time);               \
  begin_frame_args.frame_id.sequence_number++;            \
  host_impl_->WillBeginImplFrame(begin_frame_args);       \
  host_impl_->Animate();                                  \
  host_impl_->UpdateAnimationState(true);                 \
  host_impl_->DidFinishImplFrame(begin_frame_args);

    // The animation is setup in the first frame so tick at least twice to
    // actually animate it.
    ANIMATE(0);
    EXPECT_FALSE(host_impl_->IsActivelyPrecisionScrolling());
    ANIMATE(200);
    EXPECT_FALSE(host_impl_->IsActivelyPrecisionScrolling());
    ANIMATE(1000);
    EXPECT_FALSE(host_impl_->IsActivelyPrecisionScrolling());

#undef ANIMATE

    ASSERT_EQ(20, CurrentScrollOffset(OuterViewportScrollLayer()).y());

    host_impl_->ScrollEnd();
    EXPECT_FALSE(host_impl_->IsActivelyPrecisionScrolling());
  }
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutRootLayer) {
  // We should not crash when trying to scroll an empty layer tree.
  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutRenderer) {
  auto gl_owned = std::make_unique<viz::TestGLES2Interface>();
  gl_owned->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                                GL_INNOCENT_CONTEXT_RESET_ARB);

  // Initialization will fail.
  EXPECT_FALSE(
      CreateHostImpl(DefaultSettings(),
                     FakeLayerTreeFrameSink::Create3d(std::move(gl_owned))));

  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

  // We should not crash when trying to scroll after the renderer initialization
  // fails.
  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ReplaceTreeWhileScrolling) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  // We should not crash if the tree is replaced while we are scrolling.
  gfx::ScrollOffset scroll_delta(0, 10);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(),
                                   gfx::ScrollOffsetToVector2dF(scroll_delta),
                                   ui::ScrollInputType::kWheel)
                            .get(),
                        ui::ScrollInputType::kWheel)
          .thread);
  ClearLayersAndPropertyTrees(host_impl_->active_tree());

  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();

  // We should still be scrolling, because the scrolled layer also exists in the
  // new tree.
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), gfx::ScrollOffsetToVector2dF(scroll_delta),
                  ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                 scroll_delta));
}

TEST_F(LayerTreeHostImplTest, ActivateTreeScrollingNodeDisappeared) {
  SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(1000, 1000));

  auto status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(30, 30), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kWheel)
                               .get());
  EXPECT_TRUE(host_impl_->active_tree()->CurrentlyScrollingNode());

  // Create the pending tree containing only the root layer.
  CreatePendingTree();
  PropertyTrees pending_property_trees;
  pending_property_trees.sequence_number =
      host_impl_->active_tree()->property_trees()->sequence_number + 1;
  host_impl_->pending_tree()->SetPropertyTrees(&pending_property_trees);
  SetupRootLayer<LayerImpl>(host_impl_->pending_tree(), gfx::Size(100, 100));
  host_impl_->ActivateSyncTree();

  // The scroll should stop.
  EXPECT_FALSE(host_impl_->active_tree()->CurrentlyScrollingNode());
}

TEST_F(LayerTreeHostImplTest, ScrollBlocksOnWheelEventHandlers) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll = InnerViewportScrollLayer();
  scroll->SetWheelEventHandlerRegion(Region(gfx::Rect(20, 20)));
  DrawFrame();

  // Wheel handlers determine whether mouse events block scroll.
  host_impl_->active_tree()->set_event_listener_properties(
      EventListenerClass::kMouseWheel, EventListenerProperties::kBlocking);
  EXPECT_EQ(
      EventListenerProperties::kBlocking,
      host_impl_->GetEventListenerProperties(EventListenerClass::kMouseWheel));

  // LTHI should know the wheel event handler region and only block mouse events
  // in that region.
  EXPECT_TRUE(host_impl_->HasBlockingWheelEventHandlerAt(gfx::Point(10, 10)));
  EXPECT_FALSE(host_impl_->HasBlockingWheelEventHandlerAt(gfx::Point(30, 30)));

  // But they don't influence the actual handling of the scroll gestures.
  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);
  host_impl_->ScrollEnd();
}

TEST_F(LayerTreeHostImplTest, ScrollBlocksOnTouchEventHandlers) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* root = root_layer();
  LayerImpl* scroll = InnerViewportScrollLayer();
  LayerImpl* child = AddLayer();
  child->SetDrawsContent(true);
  child->SetBounds(gfx::Size(50, 50));
  CopyProperties(scroll, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(0, 20));

  // Touch handler regions determine whether touch events block scroll.
  TouchAction touch_action;
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kPanLeft, gfx::Rect(0, 0, 100, 100));
  touch_action_region.Union(TouchAction::kPanRight,
                            gfx::Rect(25, 25, 100, 100));
  root->SetTouchActionRegion(std::move(touch_action_region));
  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::HANDLER,
            host_impl_->EventListenerTypeForTouchStartOrMoveAt(
                gfx::Point(10, 10), &touch_action));
  EXPECT_EQ(TouchAction::kPanLeft, touch_action);

  // But they don't influence the actual handling of the scroll gestures.
  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);
  host_impl_->ScrollEnd();

  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::HANDLER,
            host_impl_->EventListenerTypeForTouchStartOrMoveAt(
                gfx::Point(10, 30), &touch_action));
  root->SetTouchActionRegion(TouchActionRegion());
  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER,
            host_impl_->EventListenerTypeForTouchStartOrMoveAt(
                gfx::Point(10, 30), &touch_action));
  EXPECT_EQ(TouchAction::kAuto, touch_action);
  touch_action_region = TouchActionRegion();
  touch_action_region.Union(TouchAction::kPanX, gfx::Rect(0, 0, 50, 50));
  child->SetTouchActionRegion(std::move(touch_action_region));
  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::HANDLER,
            host_impl_->EventListenerTypeForTouchStartOrMoveAt(
                gfx::Point(10, 30), &touch_action));
  EXPECT_EQ(TouchAction::kPanX, touch_action);
}

TEST_F(LayerTreeHostImplTest, ShouldScrollOnMainThread) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  host_impl_->InnerViewportScrollNode()->main_thread_scrolling_reasons =
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  DrawFrame();

  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            status.main_thread_scrolling_reasons);

  status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollWithOverlappingNonScrollableLayer) {
  CreateAndTestNonScrollableLayers(false);
}

TEST_F(LayerTreeHostImplTest,
       ScrollWithOverlappingTransparentNonScrollableLayer) {
  CreateAndTestNonScrollableLayers(true);
}

TEST_F(LayerTreeHostImplTest, ScrolledOverlappingDrawnScrollbarLayer) {
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  gfx::Size content_size = gfx::Size(360, 600);
  gfx::Size scroll_content_size = gfx::Size(345, 3800);
  gfx::Size scrollbar_size = gfx::Size(15, 600);

  SetupViewportLayersNoScrolls(content_size);
  LayerImpl* scroll = AddScrollableLayer(OuterViewportScrollLayer(),
                                         content_size, scroll_content_size);

  auto* drawn_scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, VERTICAL, false, true);
  SetupScrollbarLayer(scroll, drawn_scrollbar);
  drawn_scrollbar->SetBounds(scrollbar_size);
  drawn_scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  LayerImpl* squash = AddLayer();
  squash->SetBounds(gfx::Size(140, 300));
  squash->SetDrawsContent(true);
  squash->SetHitTestable(true);
  CopyProperties(scroll, squash);
  squash->SetOffsetToTransformParent(gfx::Vector2dF(220, 0));

  UpdateDrawProperties(layer_tree_impl);
  layer_tree_impl->DidBecomeActive();

  // The point hits squash layer and also scrollbar layer, but because the
  // scrollbar layer is a drawn scrollbar, we cannot scroll on the impl thread.
  auto status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(350, 150), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_scrolling_reasons);

  // The point hits the drawn scrollbar layer completely and should scroll on
  // the impl thread.
  status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(350, 500), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);
}

gfx::PresentationFeedback ExampleFeedback() {
  return gfx::PresentationFeedback(
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(42),
      base::TimeDelta::FromMicroseconds(18),
      gfx::PresentationFeedback::Flags::kVSync |
          gfx::PresentationFeedback::Flags::kHWCompletion);
}

class LayerTreeHostImplTestInvokeMainThreadCallbacks
    : public LayerTreeHostImplTest {
 public:
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
      const viz::FrameTimingDetails& details) override {
    for (LayerTreeHost::PresentationTimeCallback& callback : callbacks) {
      std::move(callback).Run(details.presentation_feedback);
    }
  }
};

// Tests that, when the LayerTreeHostImpl receives presentation feedback, the
// feedback gets routed to a properly registered callback.
TEST_F(LayerTreeHostImplTestInvokeMainThreadCallbacks,
       PresentationFeedbackCallbacksFire) {
  bool compositor_thread_callback_fired = false;
  bool main_thread_callback_fired = false;
  gfx::PresentationFeedback feedback_seen_by_compositor_thread_callback;
  gfx::PresentationFeedback feedback_seen_by_main_thread_callback;

  // Register a compositor-thread callback to run when the frame for
  // |frame_token_1| gets presented.
  constexpr uint32_t frame_token_1 = 1;
  host_impl_->RegisterCompositorPresentationTimeCallback(
      frame_token_1, base::BindLambdaForTesting(
                         [&](const gfx::PresentationFeedback& feedback) {
                           compositor_thread_callback_fired = true;
                           feedback_seen_by_compositor_thread_callback =
                               feedback;
                         }));

  // Register a main-thread callback to run when the frame for |frame_token_2|
  // gets presented.
  constexpr uint32_t frame_token_2 = 2;
  ASSERT_GT(frame_token_2, frame_token_1);
  host_impl_->RegisterMainThreadPresentationTimeCallback(
      frame_token_2, base::BindLambdaForTesting(
                         [&](const gfx::PresentationFeedback& feedback) {
                           main_thread_callback_fired = true;
                           feedback_seen_by_main_thread_callback = feedback;
                         }));

  viz::FrameTimingDetails mock_details;
  mock_details.presentation_feedback = ExampleFeedback();

  host_impl_->DidPresentCompositorFrame(frame_token_1, mock_details);

  EXPECT_TRUE(compositor_thread_callback_fired);
  EXPECT_EQ(feedback_seen_by_compositor_thread_callback,
            mock_details.presentation_feedback);

  // Since |frame_token_2| is strictly greater than |frame_token_1|, the
  // main-thread callback must remain queued for now.
  EXPECT_FALSE(main_thread_callback_fired);
  EXPECT_EQ(feedback_seen_by_main_thread_callback, gfx::PresentationFeedback());

  host_impl_->DidPresentCompositorFrame(frame_token_2, mock_details);

  EXPECT_TRUE(main_thread_callback_fired);
  EXPECT_EQ(feedback_seen_by_main_thread_callback,
            mock_details.presentation_feedback);
}

TEST_F(LayerTreeHostImplTest, NonFastScrollableRegionBasic) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  outer_scroll->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));

  DrawFrame();

  // All scroll types inside the non-fast scrollable region should fail.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(25, 25), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
            status.main_thread_scrolling_reasons);

  status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(25, 25), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
            status.main_thread_scrolling_reasons);

  // All scroll types outside this region should succeed.
  status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(75, 75), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);

  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();

  status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(75, 75), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  host_impl_->ScrollEnd();
}

TEST_F(LayerTreeHostImplTest, NonFastScrollableRegionWithOffset) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  outer_scroll->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));
  SetPostTranslation(outer_scroll, gfx::Vector2dF(-25, 0));
  outer_scroll->SetDrawsContent(true);

  DrawFrame();

  // This point would fall into the non-fast scrollable region except that we've
  // moved the layer left by 25 pixels.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(40, 10), gfx::Vector2d(0, 1),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);

  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 1),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();

  // This point is still inside the non-fast region.
  status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(10, 10), gfx::Vector2d(0, 1),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollHandlerNotPresent) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  EXPECT_FALSE(host_impl_->active_tree()->have_scroll_event_handlers());
  DrawFrame();

  EXPECT_FALSE(host_impl_->scroll_affects_scroll_handler());
  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  EXPECT_FALSE(host_impl_->scroll_affects_scroll_handler());
  host_impl_->ScrollEnd();
  EXPECT_FALSE(host_impl_->scroll_affects_scroll_handler());
}

TEST_F(LayerTreeHostImplTest, ScrollHandlerPresent) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  host_impl_->active_tree()->set_have_scroll_event_handlers(true);
  DrawFrame();

  EXPECT_FALSE(host_impl_->scroll_affects_scroll_handler());
  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  EXPECT_TRUE(host_impl_->scroll_affects_scroll_handler());
  host_impl_->ScrollEnd();
  EXPECT_FALSE(host_impl_->scroll_affects_scroll_handler());
}

TEST_F(LayerTreeHostImplTest, ScrollUpdateReturnsCorrectValue) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  DrawFrame();

  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(-10, 0),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);

  // Trying to scroll to the left/top will not succeed.
  EXPECT_FALSE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, 0),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_FALSE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -10),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_FALSE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, -10),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);

  // Scrolling to the right/bottom will succeed.
  EXPECT_TRUE(host_impl_
                  ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(10, 0),
                                             ui::ScrollInputType::kTouchscreen)
                                     .get())
                  .did_scroll);
  EXPECT_TRUE(host_impl_
                  ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                             ui::ScrollInputType::kTouchscreen)
                                     .get())
                  .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(10, 10),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);

  // Scrolling to left/top will now succeed.
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, 0),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -10),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, -10),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);

  // Scrolling diagonally against an edge will succeed.
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(10, -10),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, 0),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, 10),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);

  // Trying to scroll more than the available space will also succeed.
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(5000, 5000),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
}

// TODO(sunyunjia): Move scroll snap tests to a separate file.
// https://crbug.com/851690
TEST_F(LayerTreeHostImplTest, ScrollSnapOnX) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF x_delta(20, 0);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, x_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel)
          .get());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->ScrollEnd(true);

  // Snap target should not be set until the end of the animation.
  EXPECT_TRUE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(50));
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId()),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_F(LayerTreeHostImplTest, ScrollSnapOnY) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF y_delta(0, 20);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, y_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, y_delta, ui::ScrollInputType::kWheel)
          .get());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->ScrollEnd(true);

  // Snap target should not be set until the end of the animation.
  EXPECT_TRUE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(50));
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 50), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(), ElementId(10)),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_F(LayerTreeHostImplTest, ScrollSnapOnBoth) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF delta(20, 20);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, delta, ui::ScrollInputType::kWheel).get());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->ScrollEnd(true);

  // Snap target should not be set until the end of the animation.
  EXPECT_TRUE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(50));
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId(10)),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

// Simulate a ScrollBegin and ScrollEnd without any intervening ScrollUpdate.
// This test passes if it doesn't crash.
TEST_F(LayerTreeHostImplTest, SnapAfterEmptyScroll) {
  CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF y_delta(0, 20);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, y_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollEnd(true);
}

TEST_F(LayerTreeHostImplTest, ScrollSnapAfterAnimatedScroll) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF delta(20, 20);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(AnimatedUpdateState(pointer_position, delta).get());
  host_impl_->ScrollEnd();

  EXPECT_EQ(overflow->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);

  // Animating for the wheel scroll.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(50));
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  gfx::ScrollOffset current_offset = CurrentScrollOffset(overflow);
  EXPECT_LT(0, current_offset.x());
  EXPECT_GT(20, current_offset.x());
  EXPECT_LT(0, current_offset.y());
  EXPECT_GT(20, current_offset.y());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  // The scroll animation is finished, so the snap animation should begin.
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1000));

  // The snap target should not be set until the end of the animation.
  EXPECT_TRUE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  // Finish the animation.
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1500));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50), CurrentScrollOffset(overflow));
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId(10)),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_F(LayerTreeHostImplTest, SnapAnimationTargetUpdated) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF y_delta(0, 20);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, y_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, y_delta, ui::ScrollInputType::kWheel)
          .get());
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->ScrollEnd(true);
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);

  // Finish smooth wheel scroll animation which starts a snap animation.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  gfx::ScrollOffset current_offset = CurrentScrollOffset(overflow);
  EXPECT_GT(50, current_offset.y());
  EXPECT_LT(20, current_offset.y());

  // Update wheel scroll animation target. This should no longer be considered
  // as animating a snap scroll, which should happen at the end of this
  // animation.
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2dF(0, -10)).get());
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  // Finish the smooth scroll animation for wheel.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(150));

  // At the end of the previous scroll animation, a new animation for the
  // snapping should have started.
  EXPECT_TRUE(host_impl_->IsAnimatingForSnap());

  // Finish the snap animation.
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  // At the end of snap animation we should have updated the
  // TargetSnapAreaElementIds.
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(), ElementId(10)),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 50), CurrentScrollOffset(overflow));
}

TEST_F(LayerTreeHostImplTest, SnapAnimationCancelledByScroll) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF x_delta(20, 0);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, x_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel)
          .get());
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->ScrollEnd(true);
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);

  // Animating for the snap.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  gfx::ScrollOffset current_offset = CurrentScrollOffset(overflow);
  EXPECT_GT(50, current_offset.x());
  EXPECT_LT(20, current_offset.x());
  EXPECT_EQ(0, current_offset.y());

  // Interrupt the snap animation with ScrollBegin.
  auto begin_state =
      BeginState(pointer_position, x_delta, ui::ScrollInputType::kWheel);
  begin_state->data()->delta_granularity =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel)
          .thread);
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(150));
  EXPECT_VECTOR_EQ(gfx::ScrollOffsetToVector2dF(current_offset),
                   CurrentScrollOffset(overflow));
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1000));
  // Ensure that the snap target was not updated at the end of the scroll
  // animation.
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_F(LayerTreeHostImplTest,
       SnapAnimationShouldNotStartWhenScrollEndsAtSnapTarget) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF x_delta(50, 0);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, x_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  // There is a snap target at 50, scroll to it directly.
  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel)
          .get());
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->ScrollEnd(true);

  // No animation is created, but the snap target should be updated.
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId()),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);

  // We are already at a snap target so we should not animate for snap.
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());

  // Verify that we are not actually animating by running one frame and ensuring
  // scroll offset has not changed.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(100));
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId()),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_F(LayerTreeHostImplTest,
       GetSnapFlingInfoAndSetAnimatingSnapTargetWhenZoomed) {
  LayerImpl* overflow = CreateLayerForSnapping();
  // Scales the page to its 1/5.
  host_impl_->active_tree()->PushPageScaleFromMainThread(0.2f, 0.1f, 5);

  // Should be (10, 10) in the scroller's coordinate.
  gfx::Point pointer_position(2, 2);
  gfx::Vector2dF delta(4, 4);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  // Should be (20, 20) in the scroller's coordinate.
  InputHandlerScrollResult result = host_impl_->ScrollUpdate(
      UpdateState(pointer_position, delta, ui::ScrollInputType::kWheel).get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), CurrentScrollOffset(overflow));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(4, 4), result.current_visual_offset);

  gfx::Vector2dF initial_offset, target_offset;
  EXPECT_TRUE(host_impl_->GetSnapFlingInfoAndSetAnimatingSnapTarget(
      gfx::Vector2dF(10, 10), &initial_offset, &target_offset));
  EXPECT_TRUE(host_impl_->IsAnimatingForSnap());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(4, 4), initial_offset);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), target_offset);
  // Snap targets shouldn't be set until the fling animation is complete.
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  host_impl_->ScrollEndForSnapFling(true /* did_finish */);
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId(10)),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_F(LayerTreeHostImplTest, SnapFlingAnimationEndWithoutFinishing) {
  LayerImpl* overflow = CreateLayerForSnapping();
  // Scales the page to its 1/5.
  host_impl_->active_tree()->PushPageScaleFromMainThread(0.2f, 0.1f, 5.f);

  // Should be (10, 10) in the scroller's coordinate.
  gfx::Point pointer_position(2, 2);
  gfx::Vector2dF delta(4, 4);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  // Should be (20, 20) in the scroller's coordinate.
  InputHandlerScrollResult result = host_impl_->ScrollUpdate(
      UpdateState(pointer_position, delta, ui::ScrollInputType::kWheel).get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), CurrentScrollOffset(overflow));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(4, 4), result.current_visual_offset);

  gfx::Vector2dF initial_offset, target_offset;
  EXPECT_TRUE(host_impl_->GetSnapFlingInfoAndSetAnimatingSnapTarget(
      gfx::Vector2dF(10, 10), &initial_offset, &target_offset));
  EXPECT_TRUE(host_impl_->IsAnimatingForSnap());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(4, 4), initial_offset);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), target_offset);
  // Snap targets shouldn't be set until the fling animation is complete.
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  // The snap targets should not be set if the snap fling did not finish.
  host_impl_->ScrollEndForSnapFling(false /* did_finish */);
  EXPECT_FALSE(host_impl_->IsAnimatingForSnap());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_F(LayerTreeHostImplTest, OverscrollBehaviorPreventsPropagation) {
  const gfx::Size kViewportSize(100, 100);
  const gfx::Size kContentSize(200, 200);
  SetupViewportLayersOuterScrolls(kViewportSize, kContentSize);

  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  gfx::Size overflow_size(400, 400);
  LayerImpl* overflow = AddScrollableLayer(OuterViewportScrollLayer(),
                                           gfx::Size(100, 100), overflow_size);
  SetScrollOffset(scroll_layer, gfx::ScrollOffset(30, 30));

  DrawFrame();
  gfx::Point pointer_position(50, 50);
  gfx::Vector2dF x_delta(-10, 0);
  gfx::Vector2dF y_delta(0, -10);
  gfx::Vector2dF diagonal_delta(-10, -10);

  // OverscrollBehaviorTypeAuto shouldn't prevent scroll propagation.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, x_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(30, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  GetScrollNode(overflow)->overscroll_behavior =
      OverscrollBehavior(OverscrollBehavior::kOverscrollBehaviorTypeContain,
                         OverscrollBehavior::kOverscrollBehaviorTypeAuto);

  DrawFrame();

  // OverscrollBehaviorContain on x should prevent propagations of scroll
  // on x.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, x_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  // OverscrollBehaviorContain on x shouldn't prevent propagations of
  // scroll on y.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, y_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, y_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  // A scroll update with both x & y delta will adhere to the most restrictive
  // case.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, diagonal_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, diagonal_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  // Changing scroll-boundary-behavior to y axis.
  GetScrollNode(overflow)->overscroll_behavior =
      OverscrollBehavior(OverscrollBehavior::kOverscrollBehaviorTypeAuto,
                         OverscrollBehavior::kOverscrollBehaviorTypeContain);

  DrawFrame();

  // OverscrollBehaviorContain on y shouldn't prevent propagations of
  // scroll on x.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, x_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  // OverscrollBehaviorContain on y should prevent propagations of scroll
  // on y.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, y_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, y_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  // A scroll update with both x & y delta will adhere to the most restrictive
  // case.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, diagonal_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, diagonal_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  // Gesture scroll should latch to the first scroller that has non-auto
  // overscroll-behavior.
  GetScrollNode(overflow)->overscroll_behavior =
      OverscrollBehavior(OverscrollBehavior::kOverscrollBehaviorTypeContain,
                         OverscrollBehavior::kOverscrollBehaviorTypeContain);

  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(pointer_position, x_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, -x_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, y_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollUpdate(
      UpdateState(pointer_position, -y_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), CurrentScrollOffset(overflow));
}

TEST_F(LayerTreeHostImplTest, ScrollWithUserUnscrollableLayers) {
  const gfx::Size kViewportSize(100, 100);
  const gfx::Size kContentSize(200, 200);
  SetupViewportLayersOuterScrolls(kViewportSize, kContentSize);

  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  gfx::Size overflow_size(400, 400);
  LayerImpl* overflow =
      AddScrollableLayer(scroll_layer, gfx::Size(100, 100), overflow_size);

  DrawFrame();
  gfx::Point scroll_position(10, 10);
  gfx::Vector2dF scroll_delta(10, 10);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(scroll_position, scroll_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(scroll_position, scroll_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), CurrentScrollOffset(overflow));

  GetScrollNode(overflow)->user_scrollable_horizontal = false;

  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(scroll_position, scroll_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(scroll_position, scroll_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), CurrentScrollOffset(overflow));

  GetScrollNode(overflow)->user_scrollable_vertical = false;
  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(scroll_position, scroll_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), CurrentScrollOffset(overflow));

  host_impl_->ScrollUpdate(
      UpdateState(scroll_position, scroll_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), CurrentScrollOffset(scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), CurrentScrollOffset(overflow));
}

TEST_F(LayerTreeHostImplTest, ForceMainThreadScrollWithoutScrollLayer) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  ScrollNode* scroll_node = host_impl_->OuterViewportScrollNode();
  // Change the scroll node so that it no longer has an associated layer.
  scroll_node->element_id = ElementId(42);

  DrawFrame();

  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(25, 25), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
            status.main_thread_scrolling_reasons);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       AnimationSchedulingPendingTree) {
  EXPECT_FALSE(host_impl_->CommitToActiveTree());

  CreatePendingTree();
  auto* root =
      SetupRootLayer<LayerImpl>(host_impl_->pending_tree(), gfx::Size(50, 50));
  root->SetNeedsPushProperties();

  auto* child = AddLayer<LayerImpl>(host_impl_->pending_tree());
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  child->SetNeedsPushProperties();

  host_impl_->pending_tree()->SetElementIdsForTesting();
  CopyProperties(root, child);
  CreateTransformNode(child);

  AddAnimatedTransformToElementWithAnimation(child->element_id(), timeline(),
                                             10.0, 3, 0);
  UpdateDrawProperties(host_impl_->pending_tree());

  EXPECT_FALSE(did_request_next_frame_);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  host_impl_->AnimatePendingTreeAfterCommit();

  // An animation exists on the pending layer. Doing
  // AnimatePendingTreeAfterCommit() requests another frame.
  // In reality, animations without has_set_start_time() == true do not need to
  // be continuously ticked on the pending tree, so it should not request
  // another animation frame here. But we currently do so blindly if any
  // animation exists.
  EXPECT_TRUE(did_request_next_frame_);
  // The pending tree with an animation does not need to draw after animating.
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  did_request_next_frame_ = false;
  did_request_redraw_ = false;
  did_request_commit_ = false;

  host_impl_->ActivateSyncTree();

  // When the animation activates, we should request another animation frame
  // to keep the animation moving.
  EXPECT_TRUE(did_request_next_frame_);
  // On activation we don't need to request a redraw for the animation,
  // activating will draw on its own when it's ready.
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       AnimationSchedulingActiveTree) {
  EXPECT_FALSE(host_impl_->CommitToActiveTree());

  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(50, 50));
  LayerImpl* child = AddLayer();
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  host_impl_->active_tree()->SetElementIdsForTesting();
  CopyProperties(root, child);
  CreateTransformNode(child);

  // Add a translate from 6,7 to 8,9.
  TransformOperations start;
  start.AppendTranslate(6, 7, 0);
  TransformOperations end;
  end.AppendTranslate(8, 9, 0);
  AddAnimatedTransformToElementWithAnimation(child->element_id(), timeline(),
                                             4.0, start, end);
  UpdateDrawProperties(host_impl_->active_tree());

  base::TimeTicks now = base::TimeTicks::Now();
  host_impl_->WillBeginImplFrame(
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2, now));

  // TODO(crbug.com/551134): We always request a new frame and a draw for
  // animations that are on the pending tree, but we don't need to do that
  // unless they are waiting for some future time to start.
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
  did_request_next_frame_ = false;
  did_request_redraw_ = false;
  did_request_commit_ = false;

  host_impl_->ActivateAnimations();

  // On activating an animation, we should request another frame so that we'll
  // continue ticking the animation.
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
  did_request_next_frame_ = false;
  did_request_redraw_ = false;
  did_request_commit_ = false;

  // The next frame after activating, we'll tick the animation again.
  host_impl_->Animate();

  // An animation exists on the active layer. Doing Animate() requests another
  // frame after the current one.
  EXPECT_TRUE(did_request_next_frame_);
  // The animation should cause us to draw at the frame's deadline.
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, AnimationSchedulingCommitToActiveTree) {
  EXPECT_TRUE(host_impl_->CommitToActiveTree());

  auto* root = SetupDefaultRootLayer(gfx::Size(50, 50));

  auto* child = AddLayer();
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  host_impl_->active_tree()->SetElementIdsForTesting();
  CopyProperties(root, child);
  CreateTransformNode(child);

  AddAnimatedTransformToElementWithAnimation(child->element_id(), timeline(),
                                             10.0, 3, 0);

  // Set up the property trees so that UpdateDrawProperties will work in
  // CommitComplete below.
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(did_request_next_frame_);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  host_impl_->CommitComplete();

  // Animations on the active tree should be started and ticked, and a new frame
  // should be requested to continue ticking them.
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  // Delete the LayerTreeHostImpl before the TaskRunnerProvider goes away.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_F(LayerTreeHostImplTest, AnimationSchedulingOnLayerDestruction) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(50, 50));

  LayerImpl* child = AddLayer();
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  host_impl_->active_tree()->SetElementIdsForTesting();
  CopyProperties(root, child);
  CreateTransformNode(child);

  // Add a translate animation.
  TransformOperations start;
  start.AppendTranslate(6, 7, 0);
  TransformOperations end;
  end.AppendTranslate(8, 9, 0);
  AddAnimatedTransformToElementWithAnimation(child->element_id(), timeline(),
                                             4.0, start, end);
  UpdateDrawProperties(host_impl_->active_tree());

  base::TimeTicks now = base::TimeTicks::Now();
  host_impl_->WillBeginImplFrame(
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2, now));
  EXPECT_TRUE(did_request_next_frame_);
  did_request_next_frame_ = false;

  host_impl_->ActivateAnimations();
  // On activating an animation, we should request another frame so that we'll
  // continue ticking the animation.
  EXPECT_TRUE(did_request_next_frame_);
  did_request_next_frame_ = false;

  // The next frame after activating, we'll tick the animation again.
  host_impl_->Animate();
  // An animation exists on the active layer. Doing Animate() requests another
  // frame after the current one.
  EXPECT_TRUE(did_request_next_frame_);
  did_request_next_frame_ = false;

  // In the real code, you cannot remove a child on LayerImpl, but a child
  // removed on Layer will force a full tree sync which will rebuild property
  // trees without that child's property tree nodes. Clear active_tree (which
  // also clears property trees) to simulate the rebuild that would happen
  // before/during the commit.
  host_impl_->active_tree()->property_trees()->clear();
  host_impl_->UpdateElements(ElementListType::ACTIVE);

  // On updating state, we will send an animation event and request one last
  // frame.
  host_impl_->UpdateAnimationState(true);
  EXPECT_TRUE(did_request_next_frame_);
  did_request_next_frame_ = false;

  // Doing Animate() doesn't request another frame after the current one.
  host_impl_->Animate();
  EXPECT_FALSE(did_request_next_frame_);

  host_impl_->Animate();
  EXPECT_FALSE(did_request_next_frame_);
}

class MissingTilesLayer : public LayerImpl {
 public:
  MissingTilesLayer(LayerTreeImpl* layer_tree_impl, int id)
      : LayerImpl(layer_tree_impl, id), has_missing_tiles_(true) {}

  void set_has_missing_tiles(bool has_missing_tiles) {
    has_missing_tiles_ = has_missing_tiles;
  }

  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    append_quads_data->num_missing_tiles += has_missing_tiles_;
  }

 private:
  bool has_missing_tiles_;
};

TEST_F(LayerTreeHostImplTest, ImplPinchZoom) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  EXPECT_EQ(gfx::Size(50, 50), root_layer()->bounds());

  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 1;

  // The impl-based pinch zoom should adjust the max scroll position.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2;

    // TODO(bokan): What are the delta_hints for a GSB that's sent for a pinch
    // gesture that doesn't cause (initial) scrolling?
    // https://crbug.com/1030262
    host_impl_->ScrollBegin(BeginState(gfx::Point(50, 50), gfx::Vector2dF(),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd(gfx::Point(50, 50), true);
    host_impl_->ScrollEnd();
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_commit_);
    EXPECT_EQ(gfx::Size(50, 50), root_layer()->bounds());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);

    EXPECT_EQ(gfx::ScrollOffset(75.0, 75.0), MaxScrollOffset(scroll_layer));
  }

  // Scrolling after a pinch gesture should always be in local space.  The
  // scroll deltas have the page scale factor applied.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2;
    host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point());
    host_impl_->PinchGestureEnd(gfx::Point(), true);
    host_impl_->ScrollEnd();

    gfx::Vector2d scroll_delta(0, 10);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                           ui::ScrollInputType::kWheel)
                                    .get(),
                                ui::ScrollInputType::kWheel)
                  .thread);
    host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel)
            .get());
    host_impl_->ScrollEnd();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_TRUE(ScrollInfoContains(
        *scroll_info.get(), scroll_layer->element_id(),
        gfx::ScrollOffset(0, scroll_delta.y() / page_scale_delta)));
  }
}

TEST_F(LayerTreeHostImplTest, ViewportScrollbarGeometry) {
  // Tests for correct behavior of solid color scrollbars on unscrollable pages
  // under tricky fractional scale/size issues.

  // Nexus 6 viewport size.
  const gfx::Size viewport_size(412, 604);

  // The content size of a non-scrollable page we'd expect given Android's
  // behavior of a 980px layout width on non-mobile pages (often ceiled to 981
  // due to fractions resulting from DSF). Due to floating point error,
  // viewport_size.height() / minimum_scale ~= 1438.165 < 1439. Ensure we snap
  // correctly and err on the side of not showing the scrollbars.
  const gfx::Size content_size(981, 1439);
  const float minimum_scale =
      viewport_size.width() / static_cast<float>(content_size.width());

  // Since the page is unscrollable, the outer viewport matches the content
  // size.
  const gfx::Size outer_viewport_size = content_size;

  // Setup
  LayerTreeImpl* active_tree = host_impl_->active_tree();
  active_tree->PushPageScaleFromMainThread(1, minimum_scale, 4);

  // When Chrome on Android loads a non-mobile page, it resizes the main
  // frame (outer viewport) such that it matches the width of the content,
  // preventing horizontal scrolling. Replicate that behavior here.
  SetupViewportLayersInnerScrolls(viewport_size, content_size);

  LayerImpl* scroll = OuterViewportScrollLayer();
  ASSERT_EQ(GetScrollNode(scroll)->container_bounds, outer_viewport_size);
  scroll->SetHitTestable(true);
  ClipNode* outer_clip = host_impl_->active_tree()->OuterViewportClipNode();
  ASSERT_EQ(gfx::SizeF(outer_viewport_size), outer_clip->clip.size());

  // Add scrollbars. They will always exist - even if unscrollable - but their
  // visibility will be determined by whether the content can be scrolled.
  auto* v_scrollbar =
      AddLayer<PaintedScrollbarLayerImpl>(active_tree, VERTICAL, false, true);
  auto* h_scrollbar =
      AddLayer<PaintedScrollbarLayerImpl>(active_tree, HORIZONTAL, false, true);
  SetupScrollbarLayer(scroll, v_scrollbar);
  SetupScrollbarLayer(scroll, h_scrollbar);

  host_impl_->active_tree()->DidBecomeActive();

  // Zoom out to the minimum scale. The scrollbars shoud not be scrollable.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(0);
  EXPECT_FALSE(v_scrollbar->CanScrollOrientation());
  EXPECT_FALSE(h_scrollbar->CanScrollOrientation());

  // Zoom in a little and confirm that they're now scrollable.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(minimum_scale * 1.05f);
  EXPECT_TRUE(v_scrollbar->CanScrollOrientation());
  EXPECT_TRUE(h_scrollbar->CanScrollOrientation());
}

TEST_F(LayerTreeHostImplTest, ViewportScrollOrder) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.25f, 4);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_VECTOR_EQ(gfx::Vector2dF(500, 500),
                   MaxScrollOffset(outer_scroll_layer));

  host_impl_->ScrollBegin(BeginState(gfx::Point(250, 250), gfx::Vector2dF(),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, gfx::Point(0, 0));
  host_impl_->PinchGestureEnd(gfx::Point(0, 0), true);
  host_impl_->ScrollEnd();

  // Sanity check - we're zoomed in, starting from the origin.
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   CurrentScrollOffset(inner_scroll_layer));

  // Scroll down - only the inner viewport should scroll.
  host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                       gfx::Vector2dF(100, 100),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  host_impl_->ScrollEnd();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   CurrentScrollOffset(outer_scroll_layer));

  // Scroll down - outer viewport should start scrolling after the inner is at
  // its maximum.
  host_impl_->ScrollBegin(
      BeginState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                       gfx::Vector2dF(1000, 1000),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  host_impl_->ScrollEnd();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(250, 250),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(300, 300),
                   CurrentScrollOffset(outer_scroll_layer));
}

// Make sure scrolls smaller than a unit applied to the viewport don't get
// dropped. crbug.com/539334.
TEST_F(LayerTreeHostImplTest, ScrollViewportWithFractionalAmounts) {
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);
  UpdateDrawProperties(host_impl_->active_tree());

  // Sanity checks.
  EXPECT_VECTOR_EQ(gfx::Vector2dF(500, 500),
                   MaxScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(inner_scroll_layer));

  // Scroll only the layout viewport.
  host_impl_->ScrollBegin(
      BeginState(gfx::Point(250, 250), gfx::Vector2dF(0.125f, 0.125f),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(250, 250),
                                       gfx::Vector2dF(0.125f, 0.125f),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0.125f, 0.125f),
                      CurrentScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0, 0),
                      CurrentScrollOffset(inner_scroll_layer));
  host_impl_->ScrollEnd();

  host_impl_->active_tree()->PushPageScaleFromMainThread(2, 1, 2);

  // Now that we zoomed in, the scroll should be applied to the inner viewport.
  host_impl_->ScrollBegin(
      BeginState(gfx::Point(250, 250), gfx::Vector2dF(0.5f, 0.5f),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(250, 250),
                                       gfx::Vector2dF(0.5f, 0.5f),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0.125f, 0.125f),
                      CurrentScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0.25f, 0.25f),
                      CurrentScrollOffset(inner_scroll_layer));
  host_impl_->ScrollEnd();
}

// Tests that scrolls during a pinch gesture (i.e. "two-finger" scrolls) work
// as expected. That is, scrolling during a pinch should bubble from the inner
// to the outer viewport.
TEST_F(LayerTreeHostImplTest, ScrollDuringPinchGesture) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_VECTOR_EQ(gfx::Vector2dF(500, 500),
                   MaxScrollOffset(outer_scroll_layer));

  host_impl_->ScrollBegin(BeginState(gfx::Point(250, 250), gfx::Vector2dF(),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();

  host_impl_->PinchGestureUpdate(2, gfx::Point(250, 250));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(125, 125),
                   CurrentScrollOffset(inner_scroll_layer));

  // Needed so that the pinch is accounted for in draw properties.
  DrawFrame();

  host_impl_->ScrollUpdate(UpdateState(gfx::Point(250, 250),
                                       gfx::Vector2dF(10, 10),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(130, 130),
                   CurrentScrollOffset(inner_scroll_layer));

  DrawFrame();

  host_impl_->ScrollUpdate(UpdateState(gfx::Point(250, 250),
                                       gfx::Vector2dF(400, 400),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(80, 80),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(250, 250),
                   CurrentScrollOffset(inner_scroll_layer));

  host_impl_->PinchGestureEnd(gfx::Point(250, 250), true);
  host_impl_->ScrollEnd();
}

// Tests the "snapping" of pinch-zoom gestures to the screen edge. That is, when
// a pinch zoom is anchored within a certain margin of the screen edge, we
// should assume the user means to scroll into the edge of the screen.
TEST_F(LayerTreeHostImplTest, PinchZoomSnapsToScreenEdge) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  int offsetFromEdge = Viewport::kPinchZoomSnapMarginDips - 5;
  gfx::Point anchor(viewport_size.width() - offsetFromEdge,
                    viewport_size.height() - offsetFromEdge);

  // Pinch in within the margins. The scroll should stay exactly locked to the
  // bottom and right.
  host_impl_->ScrollBegin(
      BeginState(anchor, gfx::Vector2dF(), ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, anchor);
  host_impl_->PinchGestureEnd(anchor, true);
  host_impl_->ScrollEnd();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(250, 250),
                   CurrentScrollOffset(InnerViewportScrollLayer()));

  // Reset.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(1);
  SetScrollOffsetDelta(InnerViewportScrollLayer(), gfx::Vector2d());
  SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2d());

  // Pinch in within the margins. The scroll should stay exactly locked to the
  // top and left.
  anchor = gfx::Point(offsetFromEdge, offsetFromEdge);
  host_impl_->ScrollBegin(
      BeginState(anchor, gfx::Vector2dF(), ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, anchor);
  host_impl_->PinchGestureEnd(anchor, true);
  host_impl_->ScrollEnd();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   CurrentScrollOffset(InnerViewportScrollLayer()));

  // Reset.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(1);
  SetScrollOffsetDelta(InnerViewportScrollLayer(), gfx::Vector2d());
  SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2d());

  // Pinch in just outside the margin. There should be no snapping.
  offsetFromEdge = Viewport::kPinchZoomSnapMarginDips;
  anchor = gfx::Point(offsetFromEdge, offsetFromEdge);
  host_impl_->ScrollBegin(
      BeginState(anchor, gfx::Vector2dF(), ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, anchor);
  host_impl_->PinchGestureEnd(anchor, true);
  host_impl_->ScrollEnd();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   CurrentScrollOffset(InnerViewportScrollLayer()));

  // Reset.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(1);
  SetScrollOffsetDelta(InnerViewportScrollLayer(), gfx::Vector2d());
  SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2d());

  // Pinch in just outside the margin. There should be no snapping.
  offsetFromEdge = Viewport::kPinchZoomSnapMarginDips;
  anchor = gfx::Point(viewport_size.width() - offsetFromEdge,
                      viewport_size.height() - offsetFromEdge);
  host_impl_->ScrollBegin(
      BeginState(anchor, gfx::Vector2dF(), ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, anchor);
  host_impl_->PinchGestureEnd(anchor, true);
  host_impl_->ScrollEnd();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(200, 200),
                   CurrentScrollOffset(InnerViewportScrollLayer()));
}

TEST_F(LayerTreeHostImplTest, ImplPinchZoomWheelBubbleBetweenViewports) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();

  // Zoom into the page by a 2X factor
  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Scroll by a small amount, there should be no bubbling to the outer
  // viewport.
  host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(10, 20),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0), gfx::Vector2dF(10, 20),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 10),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));

  // Scroll by the inner viewport's max scroll extent, the remainder
  // should bubble up to the outer viewport.
  host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                       gfx::Vector2dF(100, 100),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 10),
                   CurrentScrollOffset(outer_scroll_layer));

  // Scroll by the outer viewport's max scroll extent, it should all go to the
  // outer viewport.
  host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(190, 180),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                       gfx::Vector2dF(190, 180),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(100, 100),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
}

TEST_F(LayerTreeHostImplTest, ScrollWithSwapPromises) {
  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(5);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
  std::unique_ptr<SwapPromise> swap_promise(
      new LatencyInfoSwapPromise(latency_info));

  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  host_impl_->QueueSwapPromiseForMainThreadScrollUpdate(
      std::move(swap_promise));
  host_impl_->ScrollEnd();

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_EQ(1u, scroll_info->swap_promises.size());
  EXPECT_EQ(latency_info.trace_id(), scroll_info->swap_promises[0]->TraceId());
}

// Test that scrolls targeting a layer with a non-null scroll_parent() don't
// bubble up.
TEST_F(LayerTreeHostImplTest, ScrollDoesntBubble) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  LayerImpl* viewport_scroll = InnerViewportScrollLayer();

  // Set up two scrolling children of the root, one of which is a scroll parent
  // to the other. Scrolls shouldn't bubbling from the child.
  LayerImpl* scroll_parent =
      AddScrollableLayer(viewport_scroll, gfx::Size(5, 5), gfx::Size(10, 10));

  LayerImpl* scroll_child_clip = AddLayer();
  // scroll_child_clip scrolls in scroll_parent, but under viewport_scroll's
  // effect.
  CopyProperties(scroll_parent, scroll_child_clip);
  scroll_child_clip->SetEffectTreeIndex(viewport_scroll->effect_tree_index());

  LayerImpl* scroll_child =
      AddScrollableLayer(scroll_child_clip, gfx::Size(5, 5), gfx::Size(10, 10));
  GetTransformNode(scroll_child)->post_translation = gfx::Vector2d(20, 20);

  DrawFrame();

  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(21, 21), gfx::Vector2d(5, 5),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(21, 21),
                                         gfx::Vector2d(5, 5),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(21, 21),
                                         gfx::Vector2d(100, 100),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    // The child should be fully scrolled by the first ScrollUpdate.
    EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 5), CurrentScrollOffset(scroll_child));

    // The scroll_parent shouldn't receive the second ScrollUpdate.
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), CurrentScrollOffset(scroll_parent));

    // The viewport shouldn't have been scrolled at all.
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                     CurrentScrollOffset(InnerViewportScrollLayer()));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                     CurrentScrollOffset(OuterViewportScrollLayer()));
  }

  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(21, 21), gfx::Vector2d(3, 4),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(21, 21),
                                         gfx::Vector2d(3, 4),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(21, 21),
                                         gfx::Vector2d(2, 1),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(21, 21),
                                         gfx::Vector2d(2, 1),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(21, 21),
                                         gfx::Vector2d(2, 1),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    // The ScrollUpdate's should scroll the parent to its extent.
    EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 5), CurrentScrollOffset(scroll_parent));

    // The viewport shouldn't receive any scroll delta.
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                     CurrentScrollOffset(InnerViewportScrollLayer()));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                     CurrentScrollOffset(OuterViewportScrollLayer()));
  }
}

TEST_F(LayerTreeHostImplTest, PinchGesture) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 1;
  float max_page_scale = 4;

  // Basic pinch zoom in gesture
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2;
    host_impl_->ScrollBegin(BeginState(gfx::Point(50, 50), gfx::Vector2dF(),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd(gfx::Point(50, 50), true);
    host_impl_->ScrollEnd();
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_commit_);

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
  }

  // Zoom-in clamping
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    float page_scale_delta = 10;

    host_impl_->ScrollBegin(BeginState(gfx::Point(50, 50), gfx::Vector2dF(),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd(gfx::Point(50, 50), true);
    host_impl_->ScrollEnd();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, max_page_scale);
  }

  // Zoom-out clamping
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.CollectScrollDeltasForTesting();
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(50, 50));

    float page_scale_delta = 0.1f;
    host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point());
    host_impl_->PinchGestureEnd(gfx::Point(), true);
    host_impl_->ScrollEnd();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, min_page_scale);

    EXPECT_TRUE(scroll_info->scrolls.empty());
  }

  // Two-finger panning should not happen based on pinch events only
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.CollectScrollDeltasForTesting();
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(20, 20));

    float page_scale_delta = 1;
    host_impl_->ScrollBegin(BeginState(gfx::Point(10, 10), gfx::Vector2dF(),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(20, 20));
    host_impl_->PinchGestureEnd(gfx::Point(20, 20), true);
    host_impl_->ScrollEnd();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
    EXPECT_TRUE(scroll_info->scrolls.empty());
  }

  // Two-finger panning should work with interleaved scroll events
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.CollectScrollDeltasForTesting();
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(20, 20));

    float page_scale_delta = 1;
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(10, 10), gfx::Vector2dF(-10, -10),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(10, 10),
                                         gfx::Vector2d(-10, -10),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(20, 20));
    host_impl_->PinchGestureEnd(gfx::Point(20, 20), true);
    host_impl_->ScrollEnd();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
    EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                   gfx::ScrollOffset(-10, -10)));
  }

  // Two-finger panning should work when starting fully zoomed out.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(0.5f, 0.5f, 4);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.CollectScrollDeltasForTesting();
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(0, 0));

    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(2, gfx::Point(0, 0));
    host_impl_->PinchGestureUpdate(1, gfx::Point(0, 0));

    // Needed so layer transform includes page scale.
    DrawFrame();

    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2d(10, 10),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->PinchGestureUpdate(1, gfx::Point(10, 10));
    host_impl_->PinchGestureEnd(gfx::Point(10, 10), true);
    host_impl_->ScrollEnd();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 2);
    EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                   gfx::ScrollOffset(10, 10)));
  }
}

TEST_F(LayerTreeHostImplTest, SyncSubpixelScrollDelta) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 1;
  float max_page_scale = 4;

  host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                         max_page_scale);
  SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.CollectScrollDeltasForTesting();
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset(0, 20));

  float page_scale_delta = 1;
  host_impl_->ScrollBegin(BeginState(gfx::Point(10, 10), gfx::Vector2dF(),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(10, 10),
                                       gfx::Vector2dF(0, -1.001f),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 9));
  host_impl_->PinchGestureEnd(gfx::Point(10, 9), true);
  host_impl_->ScrollEnd();

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
  EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                 gfx::ScrollOffset(0, -1)));

  // Verify this scroll delta is consistent with the snapped position of the
  // scroll layer.
  draw_property_utils::ComputeTransforms(
      &scroll_layer->layer_tree_impl()->property_trees()->transform_tree);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, -19),
                   scroll_layer->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(LayerTreeHostImplTest, SyncSubpixelScrollFromFractionalActiveBase) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.CollectScrollDeltasForTesting();
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          scroll_layer->element_id(), gfx::ScrollOffset(0, 20.5f));

  host_impl_->ScrollBegin(BeginState(gfx::Point(10, 10), gfx::Vector2dF(0, -1),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(10, 10),
                                       gfx::Vector2dF(0, -1),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();

  gfx::ScrollOffset active_base =
      host_impl_->active_tree()
          ->property_trees()
          ->scroll_tree.GetScrollOffsetBaseForTesting(
              scroll_layer->element_id());
  EXPECT_VECTOR_EQ(active_base, gfx::Vector2dF(0, 20.5));
  // Fractional active base should not affect the scroll delta.
  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_VECTOR_EQ(scroll_info->inner_viewport_scroll.scroll_delta,
                   gfx::Vector2dF(0, -1));
}

TEST_F(LayerTreeHostImplTest, PinchZoomTriggersPageScaleAnimation) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  float min_page_scale = 1;
  float max_page_scale = 4;
  float page_scale_delta = 1.04f;
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(200);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Zoom animation if page_scale is < 1.05 * min_page_scale.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd(gfx::Point(50, 50), true);
    host_impl_->ActivateSyncTree();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_commit_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 1);
  }

  start_time += base::TimeDelta::FromSeconds(10);
  halfway_through_animation += base::TimeDelta::FromSeconds(10);
  end_time += base::TimeDelta::FromSeconds(10);
  page_scale_delta = 1.06f;

  // No zoom animation if page_scale is >= 1.05 * min_page_scale.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd(gfx::Point(50, 50), true);
    host_impl_->ActivateSyncTree();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_commit_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
  }
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimation) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4;
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Non-anchor zoom-in
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(50, 50));

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    host_impl_->active_tree()->SetPendingPageScaleAnimation(
        std::unique_ptr<PendingPageScaleAnimation>(
            new PendingPageScaleAnimation(gfx::Vector2d(), false, 2,
                                          duration)));
    host_impl_->ActivateSyncTree();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_commit_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 2);
    EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                   gfx::ScrollOffset(-50, -50)));
  }

  start_time += base::TimeDelta::FromSeconds(10);
  halfway_through_animation += base::TimeDelta::FromSeconds(10);
  end_time += base::TimeDelta::FromSeconds(10);

  // Anchor zoom-out
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(50, 50));

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    host_impl_->active_tree()->SetPendingPageScaleAnimation(
        std::unique_ptr<PendingPageScaleAnimation>(
            new PendingPageScaleAnimation(gfx::Vector2d(25, 25), true,
                                          min_page_scale, duration)));
    host_impl_->ActivateSyncTree();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_commit_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_commit_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, min_page_scale);
    // Pushed to (0,0) via clamping against contents layer size.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                   gfx::ScrollOffset(-50, -50)));
  }
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimationNoOp) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4;
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Anchor zoom with unchanged page scale should not change scroll or scale.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(50, 50));

    host_impl_->active_tree()->SetPendingPageScaleAnimation(
        std::unique_ptr<PendingPageScaleAnimation>(
            new PendingPageScaleAnimation(gfx::Vector2d(), true, 1, duration)));
    host_impl_->ActivateSyncTree();
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->DidFinishImplFrame(begin_frame_args);

    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    begin_frame_args.frame_time = end_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_commit_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 1);
    ExpectNone(*scroll_info, scroll_layer->element_id());
  }
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimationTransferedOnSyncTreeActivate) {
  CreatePendingTree();
  host_impl_->pending_tree()->PushPageScaleFromMainThread(1, 1, 1);
  SetupViewportLayers(host_impl_->pending_tree(), gfx::Size(50, 50),
                      gfx::Size(100, 100), gfx::Size(100, 100));
  host_impl_->ActivateSyncTree();
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4;
  host_impl_->sync_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                       max_page_scale);
  host_impl_->ActivateSyncTree();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks third_through_animation = start_time + duration / 3;
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;
  float target_scale = 2;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset(50, 50));

  // Make sure TakePageScaleAnimation works properly.

  host_impl_->sync_tree()->SetPendingPageScaleAnimation(
      std::unique_ptr<PendingPageScaleAnimation>(new PendingPageScaleAnimation(
          gfx::Vector2d(), false, target_scale, duration)));
  std::unique_ptr<PendingPageScaleAnimation> psa =
      host_impl_->sync_tree()->TakePendingPageScaleAnimation();
  EXPECT_EQ(target_scale, psa->scale);
  EXPECT_EQ(duration, psa->duration);
  EXPECT_EQ(nullptr, host_impl_->sync_tree()->TakePendingPageScaleAnimation());

  // Recreate the PSA. Nothing should happen here since the tree containing the
  // PSA hasn't been activated yet.
  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  host_impl_->sync_tree()->SetPendingPageScaleAnimation(
      std::unique_ptr<PendingPageScaleAnimation>(new PendingPageScaleAnimation(
          gfx::Vector2d(), false, target_scale, duration)));
  begin_frame_args.frame_time = halfway_through_animation;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_request_next_frame_);
  EXPECT_FALSE(did_request_redraw_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Activate the sync tree. This should cause the animation to become enabled.
  // It should also clear the pointer on the sync tree.
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(nullptr,
            host_impl_->sync_tree()->TakePendingPageScaleAnimation().get());
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);

  start_time += base::TimeDelta::FromSeconds(10);
  third_through_animation += base::TimeDelta::FromSeconds(10);
  halfway_through_animation += base::TimeDelta::FromSeconds(10);
  end_time += base::TimeDelta::FromSeconds(10);

  // From here on, make sure the animation runs as normal.
  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  begin_frame_args.frame_time = third_through_animation;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Another activation shouldn't have any effect on the animation.
  host_impl_->ActivateSyncTree();

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  begin_frame_args.frame_time = halfway_through_animation;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  did_request_commit_ = false;
  begin_frame_args.frame_time = end_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_commit_);
  EXPECT_FALSE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_EQ(scroll_info->page_scale_delta, target_scale);
  EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                 gfx::ScrollOffset(-50, -50)));
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimationCompletedNotification) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset(50, 50));

  did_complete_page_scale_animation_ = false;
  host_impl_->active_tree()->SetPendingPageScaleAnimation(
      std::unique_ptr<PendingPageScaleAnimation>(
          new PendingPageScaleAnimation(gfx::Vector2d(), false, 2, duration)));
  host_impl_->ActivateSyncTree();
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_complete_page_scale_animation_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = halfway_through_animation;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_complete_page_scale_animation_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = end_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_complete_page_scale_animation_);
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

TEST_F(LayerTreeHostImplTest, MaxScrollOffsetAffectedByViewportBoundsDelta) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);
  DrawFrame();

  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  DCHECK(inner_scroll);
  EXPECT_EQ(gfx::ScrollOffset(50, 50), MaxScrollOffset(inner_scroll));

  PropertyTrees* property_trees = host_impl_->active_tree()->property_trees();
  property_trees->SetInnerViewportContainerBoundsDelta(gfx::Vector2dF(15, 15));
  property_trees->SetOuterViewportContainerBoundsDelta(gfx::Vector2dF(7, 7));
  EXPECT_EQ(gfx::ScrollOffset(42, 42), MaxScrollOffset(inner_scroll));

  property_trees->SetInnerViewportContainerBoundsDelta(gfx::Vector2dF());
  property_trees->SetOuterViewportContainerBoundsDelta(gfx::Vector2dF());
  inner_scroll->SetBounds(gfx::Size());
  GetScrollNode(inner_scroll)->bounds = inner_scroll->bounds();
  DrawFrame();

  property_trees->SetOuterViewportContainerBoundsDelta(gfx::Vector2dF(60, 60));
  EXPECT_EQ(gfx::ScrollOffset(10, 10), MaxScrollOffset(inner_scroll));
}

// Ensures scroll gestures coming from scrollbars cause animations in the
// appropriate scenarios.
TEST_F(LayerTreeHostImplTest, AnimatedGranularityCausesSmoothScroll) {
  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  gfx::Point position(295, 195);
  gfx::Vector2dF offset(0, 50);

// TODO(bokan): Unfortunately, Mac currently doesn't support smooth scrolling
// wheel events. https://crbug.com/574283.
#if defined(OS_MACOSX)
  std::vector<ui::ScrollInputType> types = {ui::ScrollInputType::kScrollbar};
#else
  std::vector<ui::ScrollInputType> types = {ui::ScrollInputType::kScrollbar,
                                            ui::ScrollInputType::kWheel};
#endif
  for (auto type : types) {
    auto begin_state = BeginState(position, offset, type);
    begin_state->data()->set_current_native_scrolling_element(
        host_impl_->OuterViewportScrollNode()->element_id);
    begin_state->data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;

    auto update_state = UpdateState(position, offset, type);
    update_state->data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;

    ASSERT_FALSE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());

    // Perform a scrollbar-like scroll (one injected by the
    // ScrollbarController). It should cause an animation to be created.
    {
      host_impl_->ScrollBegin(begin_state.get(), type);
      ASSERT_EQ(host_impl_->CurrentlyScrollingNode(),
                host_impl_->OuterViewportScrollNode());

      host_impl_->ScrollUpdate(update_state.get());
      EXPECT_TRUE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());

      host_impl_->ScrollEnd();
    }

    GetImplAnimationHost()->ScrollAnimationAbort();
    ASSERT_FALSE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());

    // Perform a scrollbar-like scroll (one injected by the
    // ScrollbarController). This time we change the granularity to precise (as
    // if thumb-dragging). This should not cause an animation.
    {
      begin_state->data()->delta_granularity =
          ui::ScrollGranularity::kScrollByPrecisePixel;
      update_state->data()->delta_granularity =
          ui::ScrollGranularity::kScrollByPrecisePixel;

      host_impl_->ScrollBegin(begin_state.get(), type);
      ASSERT_EQ(host_impl_->CurrentlyScrollingNode(),
                host_impl_->OuterViewportScrollNode());

      host_impl_->ScrollUpdate(update_state.get());
      EXPECT_FALSE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());

      host_impl_->ScrollEnd();
    }
  }
}

// Ensures scroll gestures coming from scrollbars don't cause animations if
// smooth scrolling is disabled.
TEST_F(LayerTreeHostImplTest, NonAnimatedGranularityCausesInstantScroll) {
  // Disable animated scrolling
  LayerTreeSettings settings = DefaultSettings();
  settings.enable_smooth_scroll = false;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  gfx::Point position(295, 195);
  gfx::Vector2dF offset(0, 50);

  std::vector<ui::ScrollInputType> types = {ui::ScrollInputType::kScrollbar,
                                            ui::ScrollInputType::kWheel};
  for (auto type : types) {
    auto begin_state = BeginState(position, offset, type);
    begin_state->data()->set_current_native_scrolling_element(
        host_impl_->OuterViewportScrollNode()->element_id);
    begin_state->data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;

    auto update_state = UpdateState(position, offset, type);
    update_state->data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;

    ASSERT_FALSE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());

    // Perform a scrollbar-like scroll (one injected by the
    // ScrollbarController). It should cause an animation to be created.
    {
      host_impl_->ScrollBegin(begin_state.get(), type);
      ASSERT_EQ(host_impl_->CurrentlyScrollingNode(),
                host_impl_->OuterViewportScrollNode());

      host_impl_->ScrollUpdate(update_state.get());
      EXPECT_FALSE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());

      host_impl_->ScrollEnd();
    }
  }
}

class LayerTreeHostImplOverridePhysicalTime : public LayerTreeHostImpl {
 public:
  LayerTreeHostImplOverridePhysicalTime(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner,
      RenderingStatsInstrumentation* rendering_stats_instrumentation)
      : LayerTreeHostImpl(settings,
                          client,
                          task_runner_provider,
                          rendering_stats_instrumentation,
                          task_graph_runner,
                          AnimationHost::CreateForTesting(ThreadInstance::IMPL),
                          0,
                          nullptr,
                          nullptr) {}

  const viz::BeginFrameArgs& CurrentBeginFrameArgs() const override {
    return current_begin_frame_args_;
  }

  void SetCurrentPhysicalTimeTicksForTest(base::TimeTicks fake_now) {
    current_begin_frame_args_ = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 1, fake_now);
  }

 private:
  viz::BeginFrameArgs current_begin_frame_args_;
};

class LayerTreeHostImplTestScrollbarAnimation : public LayerTreeHostImplTest {
 protected:
  void SetupLayers(LayerTreeSettings settings) {
    host_impl_->ReleaseLayerTreeFrameSink();
    host_impl_ = nullptr;

    gfx::Size viewport_size(50, 50);
    gfx::Size content_size(100, 100);

    LayerTreeHostImplOverridePhysicalTime* host_impl_override_time =
        new LayerTreeHostImplOverridePhysicalTime(
            settings, this, &task_runner_provider_, &task_graph_runner_,
            &stats_instrumentation_);
    host_impl_ = base::WrapUnique(host_impl_override_time);
    layer_tree_frame_sink_ = CreateLayerTreeFrameSink();
    host_impl_->SetVisible(true);
    host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());

    SetupViewportLayersInnerScrolls(viewport_size, content_size);
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 4);

    auto* scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
        host_impl_->active_tree(), VERTICAL, 10, 0, false);
    SetupScrollbarLayer(OuterViewportScrollLayer(), scrollbar);

    host_impl_->active_tree()->DidBecomeActive();
    host_impl_->active_tree()->HandleScrollbarShowRequestsFromMain();
    host_impl_->active_tree()->SetLocalSurfaceIdAllocationFromParent(
        viz::LocalSurfaceIdAllocation(
            viz::LocalSurfaceId(1, base::UnguessableToken::Deserialize(2u, 3u)),
            base::TimeTicks::Now()));

    DrawFrame();

    // SetScrollElementId will initialize the scrollbar which will cause it to
    // show and request a redraw.
    did_request_redraw_ = false;
  }

  void RunTest(LayerTreeSettings::ScrollbarAnimator animator) {
    LayerTreeSettings settings = DefaultSettings();
    settings.scrollbar_animator = animator;
    settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(20);
    settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(20);

    // If no animator is set, scrollbar won't show and no animation is expected.
    bool expecting_animations = animator != LayerTreeSettings::NO_ANIMATOR;

    SetupLayers(settings);

    base::TimeTicks fake_now = base::TimeTicks::Now();

    // Android Overlay Scrollbar does not have a initial show and fade out.
    if (animator == LayerTreeSettings::AURA_OVERLAY) {
      // A task will be posted to fade the initial scrollbar.
      EXPECT_FALSE(did_request_next_frame_);
      EXPECT_FALSE(did_request_redraw_);
      EXPECT_FALSE(animation_task_.is_null());
      requested_animation_delay_ = base::TimeDelta();
      animation_task_.Reset();
    } else {
      EXPECT_FALSE(did_request_next_frame_);
      EXPECT_FALSE(did_request_redraw_);
      EXPECT_TRUE(animation_task_.is_null());
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    }

    // If no scroll happened during a scroll gesture, it should have no effect.
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(), gfx::Vector2dF(), ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    host_impl_->ScrollEnd();
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    EXPECT_TRUE(animation_task_.is_null());

    // For Aura Overlay Scrollbar, if no scroll happened during a scroll
    // gesture, shows scrollbars and schedules a delay fade out.
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(), gfx::Vector2dF(), ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0, 0),
                                         ui::ScrollInputType::kWheel)
                                 .get());
    host_impl_->ScrollEnd();
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    if (animator == LayerTreeSettings::AURA_OVERLAY) {
      EXPECT_EQ(base::TimeDelta::FromMilliseconds(20),
                requested_animation_delay_);
      EXPECT_FALSE(animation_task_.is_null());
      requested_animation_delay_ = base::TimeDelta();
      animation_task_.Reset();
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.is_null());
    }

    // Before the scrollbar animation exists, we should not get redraws.
    viz::BeginFrameArgs begin_frame_args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 2, fake_now);
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_next_frame_);
    did_request_next_frame_ = false;
    EXPECT_FALSE(did_request_redraw_);
    did_request_redraw_ = false;
    EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    EXPECT_TRUE(animation_task_.is_null());
    host_impl_->DidFinishImplFrame(begin_frame_args);

    // After a scroll, a scrollbar animation should be scheduled about 20ms from
    // now.
    host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 5),
                                       ui::ScrollInputType::kWheel)
                                .get(),
                            ui::ScrollInputType::kWheel);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0, 5),
                                         ui::ScrollInputType::kWheel)
                                 .get());
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_redraw_);
    did_request_redraw_ = false;
    if (expecting_animations) {
      EXPECT_EQ(base::TimeDelta::FromMilliseconds(20),
                requested_animation_delay_);
      EXPECT_FALSE(animation_task_.is_null());
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.is_null());
    }

    host_impl_->ScrollEnd();
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    if (expecting_animations) {
      EXPECT_EQ(base::TimeDelta::FromMilliseconds(20),
                requested_animation_delay_);
      EXPECT_FALSE(animation_task_.is_null());
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.is_null());
    }

    if (expecting_animations) {
      // Before the scrollbar animation begins, we should not get redraws.
      begin_frame_args = viz::CreateBeginFrameArgsForTesting(
          BEGINFRAME_FROM_HERE, 0, 3, fake_now);
      host_impl_->WillBeginImplFrame(begin_frame_args);
      host_impl_->Animate();
      EXPECT_FALSE(did_request_next_frame_);
      did_request_next_frame_ = false;
      EXPECT_FALSE(did_request_redraw_);
      did_request_redraw_ = false;
      host_impl_->DidFinishImplFrame(begin_frame_args);

      // Start the scrollbar animation.
      fake_now += requested_animation_delay_;
      requested_animation_delay_ = base::TimeDelta();
      std::move(animation_task_).Run();
      EXPECT_TRUE(did_request_next_frame_);
      did_request_next_frame_ = false;
      EXPECT_FALSE(did_request_redraw_);

      // After the scrollbar animation begins, we should start getting redraws.
      begin_frame_args = viz::CreateBeginFrameArgsForTesting(
          BEGINFRAME_FROM_HERE, 0, 4, fake_now);
      host_impl_->WillBeginImplFrame(begin_frame_args);
      host_impl_->Animate();
      EXPECT_TRUE(did_request_next_frame_);
      did_request_next_frame_ = false;
      EXPECT_TRUE(did_request_redraw_);
      did_request_redraw_ = false;
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.is_null());
      host_impl_->DidFinishImplFrame(begin_frame_args);
    }

    // Setting the scroll offset outside a scroll should not cause the
    // scrollbar to appear or schedule a scrollbar animation.
    if (host_impl_->active_tree()
            ->property_trees()
            ->scroll_tree.UpdateScrollOffsetBaseForTesting(
                InnerViewportScrollLayer()->element_id(),
                gfx::ScrollOffset(5, 5)))
      host_impl_->active_tree()->DidUpdateScrollOffset(
          InnerViewportScrollLayer()->element_id());
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    EXPECT_TRUE(animation_task_.is_null());

    // Changing page scale triggers scrollbar animation.
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 4);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(1.1f);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    if (expecting_animations) {
      EXPECT_EQ(base::TimeDelta::FromMilliseconds(20),
                requested_animation_delay_);
      EXPECT_FALSE(animation_task_.is_null());
      requested_animation_delay_ = base::TimeDelta();
      animation_task_.Reset();
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.is_null());
    }
  }
};

TEST_F(LayerTreeHostImplTestScrollbarAnimation, Android) {
  RunTest(LayerTreeSettings::ANDROID_OVERLAY);
}

TEST_F(LayerTreeHostImplTestScrollbarAnimation, AuraOverlay) {
  RunTest(LayerTreeSettings::AURA_OVERLAY);
}

TEST_F(LayerTreeHostImplTestScrollbarAnimation, NoAnimator) {
  RunTest(LayerTreeSettings::NO_ANIMATOR);
}

class LayerTreeHostImplTestScrollbarOpacity : public LayerTreeHostImplTest {
 protected:
  void RunTest(LayerTreeSettings::ScrollbarAnimator animator) {
    LayerTreeSettings settings = DefaultSettings();
    settings.scrollbar_animator = animator;
    settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(20);
    settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(20);
    gfx::Size viewport_size(50, 50);
    gfx::Size content_size(100, 100);

    // If no animator is set, scrollbar won't show and no animation is expected.
    bool expecting_animations = animator != LayerTreeSettings::NO_ANIMATOR;

    CreateHostImpl(settings, CreateLayerTreeFrameSink());
    CreatePendingTree();
    SetupViewportLayers(host_impl_->pending_tree(), viewport_size, content_size,
                        content_size);

    LayerImpl* scroll =
        host_impl_->pending_tree()->OuterViewportScrollLayerForTesting();
    auto* scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
        host_impl_->pending_tree(), VERTICAL, 10, 0, false);
    SetupScrollbarLayer(scroll, scrollbar);
    scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(90, 0));

    host_impl_->pending_tree()->PushPageScaleFromMainThread(1, 1, 1);
    UpdateDrawProperties(host_impl_->pending_tree());
    host_impl_->ActivateSyncTree();

    LayerImpl* active_scrollbar_layer =
        host_impl_->active_tree()->LayerById(scrollbar->id());

    EffectNode* active_tree_node = GetEffectNode(active_scrollbar_layer);
    EXPECT_FLOAT_EQ(active_scrollbar_layer->Opacity(),
                    active_tree_node->opacity);

    if (expecting_animations) {
      host_impl_->ScrollbarAnimationControllerForElementId(scroll->element_id())
          ->DidMouseMove(gfx::PointF(0, 90));
    } else {
      EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                             scroll->element_id()));
    }
    host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 5),
                                       ui::ScrollInputType::kWheel)
                                .get(),
                            ui::ScrollInputType::kWheel);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0, 5),
                                         ui::ScrollInputType::kWheel)
                                 .get());
    host_impl_->ScrollEnd();

    CreatePendingTree();
    // To test the case where the effect tree index of scrollbar layer changes,
    // we create an effect node with a render surface above the scrollbar's
    // effect node.
    auto* pending_root = host_impl_->pending_tree()->root_layer();
    auto& new_effect_node = CreateEffectNode(
        GetPropertyTrees(pending_root), pending_root->effect_tree_index(),
        pending_root->transform_tree_index(), pending_root->clip_tree_index());
    new_effect_node.render_surface_reason = RenderSurfaceReason::kTest;
    LayerImpl* pending_scrollbar_layer =
        host_impl_->pending_tree()->LayerById(scrollbar->id());
    GetEffectNode(pending_scrollbar_layer)->parent_id = new_effect_node.id;
    pending_scrollbar_layer->SetNeedsPushProperties();
    UpdateDrawProperties(host_impl_->pending_tree());

    EffectNode* pending_tree_node = GetEffectNode(pending_scrollbar_layer);
    if (expecting_animations) {
      EXPECT_FLOAT_EQ(1, active_tree_node->opacity);
      EXPECT_FLOAT_EQ(1, active_scrollbar_layer->Opacity());
    } else {
      EXPECT_FLOAT_EQ(0, active_tree_node->opacity);
      EXPECT_FLOAT_EQ(0, active_scrollbar_layer->Opacity());
    }
    EXPECT_FLOAT_EQ(0, pending_tree_node->opacity);

    host_impl_->ActivateSyncTree();
    active_tree_node = GetEffectNode(active_scrollbar_layer);
    if (expecting_animations) {
      EXPECT_FLOAT_EQ(1, active_tree_node->opacity);
      EXPECT_FLOAT_EQ(1, active_scrollbar_layer->Opacity());
    } else {
      EXPECT_FLOAT_EQ(0, active_tree_node->opacity);
      EXPECT_FLOAT_EQ(0, active_scrollbar_layer->Opacity());
    }
  }
};

TEST_F(LayerTreeHostImplTestScrollbarOpacity, Android) {
  RunTest(LayerTreeSettings::ANDROID_OVERLAY);
}

TEST_F(LayerTreeHostImplTestScrollbarOpacity, AuraOverlay) {
  RunTest(LayerTreeSettings::AURA_OVERLAY);
}

TEST_F(LayerTreeHostImplTestScrollbarOpacity, NoAnimator) {
  RunTest(LayerTreeSettings::NO_ANIMATOR);
}

class LayerTreeHostImplTestMultiScrollable : public LayerTreeHostImplTest {
 public:
  void SetUpLayers(LayerTreeSettings settings) {
    is_aura_scrollbar_ =
        settings.scrollbar_animator == LayerTreeSettings::AURA_OVERLAY;
    gfx::Size viewport_size(300, 200);
    gfx::Size content_size(1000, 1000);
    gfx::Size child_layer_size(250, 150);
    gfx::Size scrollbar_size_1(gfx::Size(15, viewport_size.height()));
    gfx::Size scrollbar_size_2(gfx::Size(15, child_layer_size.height()));

    CreateHostImpl(settings, CreateLayerTreeFrameSink());
    host_impl_->active_tree()->SetDeviceScaleFactor(1);
    SetupViewportLayers(host_impl_->active_tree(), viewport_size, content_size,
                        content_size);
    LayerImpl* root_scroll = OuterViewportScrollLayer();

    // scrollbar_1 on root scroll.
    scrollbar_1_ = AddLayer<SolidColorScrollbarLayerImpl>(
        host_impl_->active_tree(), VERTICAL, 15, 0, true);
    SetupScrollbarLayer(root_scroll, scrollbar_1_);
    scrollbar_1_->SetBounds(scrollbar_size_1);
    TouchActionRegion touch_action_region;
    touch_action_region.Union(TouchAction::kNone, gfx::Rect(scrollbar_size_1));
    scrollbar_1_->SetTouchActionRegion(touch_action_region);

    // scrollbar_2 on child.
    auto* child =
        AddScrollableLayer(root_scroll, gfx::Size(100, 100), child_layer_size);
    GetTransformNode(child)->post_translation = gfx::Vector2dF(50, 50);

    scrollbar_2_ = AddLayer<SolidColorScrollbarLayerImpl>(
        host_impl_->active_tree(), VERTICAL, 15, 0, true);
    SetupScrollbarLayer(child, scrollbar_2_);
    scrollbar_2_->SetBounds(scrollbar_size_2);

    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->active_tree()->UpdateScrollbarGeometries();
    host_impl_->active_tree()->DidBecomeActive();

    ResetScrollbars();
  }

  void ResetScrollbars() {
    GetEffectNode(scrollbar_1_)->opacity = 0;
    GetEffectNode(scrollbar_2_)->opacity = 0;
    UpdateDrawProperties(host_impl_->active_tree());

    if (is_aura_scrollbar_)
      animation_task_.Reset();
  }

  bool is_aura_scrollbar_;
  SolidColorScrollbarLayerImpl* scrollbar_1_;
  SolidColorScrollbarLayerImpl* scrollbar_2_;
};

TEST_F(LayerTreeHostImplTestMultiScrollable,
       ScrollbarFlashAfterAnyScrollUpdate) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(500);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_flash_after_any_scroll_update = true;

  SetUpLayers(settings);

  EXPECT_EQ(scrollbar_1_->Opacity(), 0);
  EXPECT_EQ(scrollbar_2_->Opacity(), 0);

  // Scroll on root should flash all scrollbars.
  host_impl_->RootScrollBegin(
      BeginState(gfx::Point(20, 20), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(20, 20), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());

  EXPECT_FALSE(animation_task_.is_null());
  ResetScrollbars();

  // Scroll on child should flash all scrollbars.
  host_impl_->ScrollBegin(BeginState(gfx::Point(70, 70), gfx::Vector2dF(0, 100),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(70, 70), gfx::Vector2d(0, 100)).get());
  host_impl_->ScrollEnd();

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());

  EXPECT_FALSE(animation_task_.is_null());
}

TEST_F(LayerTreeHostImplTestMultiScrollable, ScrollbarFlashWhenMouseEnter) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(500);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_flash_when_mouse_enter = true;

  SetUpLayers(settings);

  EXPECT_EQ(scrollbar_1_->Opacity(), 0);
  EXPECT_EQ(scrollbar_2_->Opacity(), 0);

  // Scroll should flash when mouse enter.
  host_impl_->MouseMoveAt(gfx::Point(1, 1));

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_FALSE(scrollbar_2_->Opacity());
  EXPECT_FALSE(animation_task_.is_null());

  host_impl_->MouseMoveAt(gfx::Point(51, 51));

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());
  EXPECT_FALSE(animation_task_.is_null());
}

TEST_F(LayerTreeHostImplTest, ScrollHitTestOnScrollbar) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(500);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::NO_ANIMATOR;

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  gfx::Size child_layer_size(250, 150);
  gfx::Size scrollbar_size_1(gfx::Size(15, viewport_size.height()));
  gfx::Size scrollbar_size_2(gfx::Size(15, child_layer_size.height()));

  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetDeviceScaleFactor(1);
  SetupViewportLayersInnerScrolls(viewport_size, content_size);
  LayerImpl* root_scroll = OuterViewportScrollLayer();

  // scrollbar_1 on root scroll.
  auto* scrollbar_1 = AddLayer<PaintedScrollbarLayerImpl>(
      host_impl_->active_tree(), VERTICAL, true, true);
  SetupScrollbarLayer(root_scroll, scrollbar_1);
  scrollbar_1->SetBounds(scrollbar_size_1);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kNone, gfx::Rect(scrollbar_size_1));
  scrollbar_1->SetTouchActionRegion(touch_action_region);

  LayerImpl* child =
      AddScrollableLayer(root_scroll, gfx::Size(100, 100), child_layer_size);
  GetTransformNode(child)->post_translation = gfx::Vector2dF(50, 50);

  // scrollbar_2 on child.
  auto* scrollbar_2 = AddLayer<PaintedScrollbarLayerImpl>(
      host_impl_->active_tree(), VERTICAL, true, true);
  SetupScrollbarLayer(child, scrollbar_2);
  scrollbar_2->SetBounds(scrollbar_size_2);
  scrollbar_2->SetOffsetToTransformParent(gfx::Vector2dF(50, 50));

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  // Wheel scroll on root scrollbar should process on impl thread.
  {
    InputHandler::ScrollStatus status =
        host_impl_->ScrollBegin(BeginState(gfx::Point(1, 1), gfx::Vector2dF(),
                                           ui::ScrollInputType::kWheel)
                                    .get(),
                                ui::ScrollInputType::kWheel);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
    host_impl_->ScrollEnd();
  }

  // Touch scroll on root scrollbar should process on main thread.
  {
    InputHandler::ScrollStatus status =
        host_impl_->ScrollBegin(BeginState(gfx::Point(1, 1), gfx::Vector2dF(),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kScrollbarScrolling,
              status.main_thread_scrolling_reasons);
  }

  // Wheel scroll on scrollbar should process on impl thread.
  {
    InputHandler::ScrollStatus status =
        host_impl_->ScrollBegin(BeginState(gfx::Point(51, 51), gfx::Vector2dF(),
                                           ui::ScrollInputType::kWheel)
                                    .get(),
                                ui::ScrollInputType::kWheel);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_scrolling_reasons);
    host_impl_->ScrollEnd();
  }

  // Touch scroll on scrollbar should process on main thread.
  {
    InputHandler::ScrollStatus status =
        host_impl_->ScrollBegin(BeginState(gfx::Point(51, 51), gfx::Vector2dF(),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kScrollbarScrolling,
              status.main_thread_scrolling_reasons);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollbarVisibilityChangeCausesRedrawAndCommit) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(20);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(20);
  gfx::Size viewport_size(50, 50);
  gfx::Size content_size(100, 100);

  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  CreatePendingTree();
  SetupViewportLayers(host_impl_->pending_tree(), viewport_size, content_size,
                      content_size);
  LayerImpl* scroll =
      host_impl_->pending_tree()->OuterViewportScrollLayerForTesting();
  auto* scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->pending_tree(), VERTICAL, 10, 0, false);
  SetupScrollbarLayer(scroll, scrollbar);
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(90, 0));

  host_impl_->pending_tree()->PushPageScaleFromMainThread(1, 1, 1);
  host_impl_->ActivateSyncTree();

  ScrollbarAnimationController* scrollbar_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          scroll->element_id());

  // Scrollbars will flash shown but we should have a fade out animation
  // queued. Run it and fade out the scrollbars.
  {
    ASSERT_FALSE(animation_task_.is_null());
    ASSERT_FALSE(animation_task_.IsCancelled());
    std::move(animation_task_).Run();

    base::TimeTicks fake_now = base::TimeTicks::Now();
    scrollbar_controller->Animate(fake_now);
    fake_now += settings.scrollbar_fade_delay;
    scrollbar_controller->Animate(fake_now);

    ASSERT_TRUE(scrollbar_controller->ScrollbarsHidden());
  }

  // Move the mouse over the scrollbar region. This should post a delayed fade
  // in task. Execute it to fade in the scrollbars.
  {
    animation_task_.Reset();
    scrollbar_controller->DidMouseMove(gfx::PointF(90, 0));
    ASSERT_FALSE(animation_task_.is_null());
    ASSERT_FALSE(animation_task_.IsCancelled());
  }

  // The fade in task should cause the scrollbars to show. Ensure that we
  // requested a redraw and a commit.
  {
    did_request_redraw_ = false;
    did_request_commit_ = false;
    ASSERT_TRUE(scrollbar_controller->ScrollbarsHidden());
    std::move(animation_task_).Run();

    base::TimeTicks fake_now = base::TimeTicks::Now();
    scrollbar_controller->Animate(fake_now);
    fake_now += settings.scrollbar_fade_duration;
    scrollbar_controller->Animate(fake_now);

    ASSERT_FALSE(scrollbar_controller->ScrollbarsHidden());
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_commit_);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollbarInnerLargerThanOuter) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size inner_viewport_size(315, 200);
  gfx::Size outer_viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  SetupViewportLayers(host_impl_->active_tree(), inner_viewport_size,
                      outer_viewport_size, content_size);
  LayerImpl* root_scroll = OuterViewportScrollLayer();
  auto* horiz_scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      host_impl_->active_tree(), HORIZONTAL, true, true);
  SetupScrollbarLayer(root_scroll, horiz_scrollbar);
  LayerImpl* child = AddLayer();
  child->SetBounds(content_size);
  child->SetBounds(inner_viewport_size);

  host_impl_->active_tree()->UpdateScrollbarGeometries();

  EXPECT_EQ(300, horiz_scrollbar->clip_layer_length());
}

TEST_F(LayerTreeHostImplTest, ScrollbarRegistration) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_animator = LayerTreeSettings::ANDROID_OVERLAY;
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(20);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(20);
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);

  SetupViewportLayersInnerScrolls(viewport_size, content_size);

  auto* container = InnerViewportScrollLayer();
  auto* root_scroll = OuterViewportScrollLayer();
  auto* vert_1_scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), VERTICAL, 5, 5, true);
  CopyProperties(container, vert_1_scrollbar);

  auto* horiz_1_scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), HORIZONTAL, 5, 5, true);
  CopyProperties(container, horiz_1_scrollbar);

  auto* vert_2_scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), VERTICAL, 5, 5, true);
  CopyProperties(container, vert_2_scrollbar);

  auto* horiz_2_scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), HORIZONTAL, 5, 5, true);
  CopyProperties(container, horiz_2_scrollbar);

  UpdateDrawProperties(host_impl_->active_tree());

  // Check scrollbar registration on the viewport layers.
  EXPECT_EQ(0ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                         root_scroll->element_id()));
  vert_1_scrollbar->SetScrollElementId(root_scroll->element_id());
  EXPECT_EQ(1ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  EXPECT_TRUE(host_impl_->ScrollbarAnimationControllerForElementId(
      root_scroll->element_id()));
  horiz_1_scrollbar->SetScrollElementId(root_scroll->element_id());
  EXPECT_EQ(2ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  EXPECT_TRUE(host_impl_->ScrollbarAnimationControllerForElementId(
      root_scroll->element_id()));

  // Scrolling the viewport should result in a scrollbar animation update.
  animation_task_.Reset();
  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(10, 10),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(10, 10),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();
  EXPECT_FALSE(animation_task_.is_null());
  animation_task_.Reset();

  // Check scrollbar registration on a sublayer.
  LayerImpl* child =
      AddScrollableLayer(root_scroll, viewport_size, gfx::Size(200, 200));
  ElementId child_scroll_element_id = child->element_id();
  EXPECT_EQ(0ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                         child_scroll_element_id));
  vert_2_scrollbar->SetScrollElementId(child_scroll_element_id);
  EXPECT_EQ(1ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  EXPECT_TRUE(host_impl_->ScrollbarAnimationControllerForElementId(
      child_scroll_element_id));
  horiz_2_scrollbar->SetScrollElementId(child_scroll_element_id);
  EXPECT_EQ(2ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  EXPECT_TRUE(host_impl_->ScrollbarAnimationControllerForElementId(
      child_scroll_element_id));

  // Changing one of the child layers should result in a scrollbar animation
  // update.
  animation_task_.Reset();
  child->set_needs_show_scrollbars(true);
  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->HandleScrollbarShowRequestsFromMain();
  EXPECT_FALSE(animation_task_.is_null());
  animation_task_.Reset();

  // Check scrollbar unregistration.
  ElementId root_scroll_element_id = root_scroll->element_id();
  host_impl_->active_tree()->DetachLayers();
  EXPECT_EQ(0ul, host_impl_->ScrollbarsFor(root_scroll_element_id).size());
  EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                         root_scroll_element_id));
  EXPECT_EQ(0ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                         root_scroll_element_id));
}

TEST_F(LayerTreeHostImplTest, ScrollBeforeMouseMove) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(20);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(20);
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);

  SetupViewportLayersInnerScrolls(viewport_size, content_size);
  auto* root_scroll = OuterViewportScrollLayer();

  const int kScrollbarThickness = 5;
  auto* vert_scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), VERTICAL, kScrollbarThickness, 0, false);
  SetupScrollbarLayer(root_scroll, vert_scrollbar);
  vert_scrollbar->SetBounds(gfx::Size(10, 200));
  vert_scrollbar->SetOffsetToTransformParent(
      gfx::Vector2dF(300 - kScrollbarThickness, 0));

  host_impl_->active_tree()->UpdateScrollbarGeometries();

  EXPECT_EQ(1ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  auto* scrollbar_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          root_scroll->element_id());

  const float kDistanceToTriggerThumb =
      vert_scrollbar->ComputeThumbQuadRect().height() +
      SingleScrollbarAnimationControllerThinning::
          kMouseMoveDistanceToTriggerExpand;

  // Move the mouse near the thumb while its at the viewport top.
  auto near_thumb_at_top = gfx::Point(295, kDistanceToTriggerThumb - 1);
  host_impl_->MouseMoveAt(near_thumb_at_top);
  EXPECT_TRUE(scrollbar_controller->MouseIsNearScrollbarThumb(VERTICAL));

  // Move the mouse away from the thumb.
  host_impl_->MouseMoveAt(gfx::Point(295, kDistanceToTriggerThumb + 1));
  EXPECT_FALSE(scrollbar_controller->MouseIsNearScrollbarThumb(VERTICAL));

  // Scroll the page down which moves the thumb down to the viewport bottom.
  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 800),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 800),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();

  // Move the mouse near the thumb in the top position.
  host_impl_->MouseMoveAt(near_thumb_at_top);
  EXPECT_FALSE(scrollbar_controller->MouseIsNearScrollbarThumb(VERTICAL));

  // Scroll the page up which moves the thumb back up.
  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, -800),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -800),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();

  // Move the mouse near the thumb in the top position.
  host_impl_->MouseMoveAt(near_thumb_at_top);
  EXPECT_TRUE(scrollbar_controller->MouseIsNearScrollbarThumb(VERTICAL));
}

void LayerTreeHostImplTest::SetupMouseMoveAtWithDeviceScale(
    float device_scale_factor) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(500);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  gfx::Size scrollbar_size(gfx::Size(15, viewport_size.height()));

  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetDeviceScaleFactor(device_scale_factor);
  SetupViewportLayersInnerScrolls(viewport_size, content_size);
  LayerImpl* root_scroll = OuterViewportScrollLayer();
  // The scrollbar is on the left side.
  auto* scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), VERTICAL, 15, 0, true);
  SetupScrollbarLayer(root_scroll, scrollbar);
  scrollbar->SetBounds(scrollbar_size);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kNone, gfx::Rect(scrollbar_size));
  scrollbar->SetTouchActionRegion(touch_action_region);
  host_impl_->active_tree()->DidBecomeActive();

  DrawFrame();

  ScrollbarAnimationController* scrollbar_animation_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          root_scroll->element_id());

  const float kMouseMoveDistanceToTriggerFadeIn =
      ScrollbarAnimationController::kMouseMoveDistanceToTriggerFadeIn;

  const float kMouseMoveDistanceToTriggerExpand =
      SingleScrollbarAnimationControllerThinning::
          kMouseMoveDistanceToTriggerExpand;

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerFadeIn, 1));
  EXPECT_FALSE(scrollbar_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerExpand - 1, 10));
  EXPECT_TRUE(scrollbar_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerFadeIn, 100));
  EXPECT_FALSE(scrollbar_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));

  did_request_redraw_ = false;
  EXPECT_FALSE(
      scrollbar_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  host_impl_->MouseMoveAt(gfx::Point(10, 10));
  EXPECT_TRUE(
      scrollbar_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  host_impl_->MouseMoveAt(gfx::Point(10, 0));
  EXPECT_TRUE(
      scrollbar_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  host_impl_->MouseMoveAt(gfx::Point(150, 120));
  EXPECT_FALSE(
      scrollbar_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
}

TEST_F(LayerTreeHostImplTest, MouseMoveAtWithDeviceScaleOf1) {
  SetupMouseMoveAtWithDeviceScale(1);
}

TEST_F(LayerTreeHostImplTest, MouseMoveAtWithDeviceScaleOf2) {
  SetupMouseMoveAtWithDeviceScale(2);
}

// This test verifies that only SurfaceLayers in the viewport and have fallbacks
// that are different are included in viz::CompositorFrameMetadata's
// |activation_dependencies|.
TEST_F(LayerTreeHostImplTest, ActivationDependenciesInMetadata) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  LayerImpl* root = root_layer();

  std::vector<viz::SurfaceId> primary_surfaces = {
      MakeSurfaceId(viz::FrameSinkId(1, 1), 1),
      MakeSurfaceId(viz::FrameSinkId(2, 2), 2),
      MakeSurfaceId(viz::FrameSinkId(3, 3), 3)};

  std::vector<viz::SurfaceId> fallback_surfaces = {
      MakeSurfaceId(viz::FrameSinkId(4, 4), 1),
      MakeSurfaceId(viz::FrameSinkId(4, 4), 2),
      MakeSurfaceId(viz::FrameSinkId(4, 4), 3)};

  for (size_t i = 0; i < primary_surfaces.size(); ++i) {
    auto* child = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());
    child->SetBounds(gfx::Size(1, 1));
    child->SetDrawsContent(true);
    child->SetRange(
        viz::SurfaceRange(fallback_surfaces[i], primary_surfaces[i]), 2u);
    CopyProperties(root, child);
    child->SetOffsetToTransformParent(gfx::Vector2dF(25.0f * i, 0));
  }

  base::flat_set<viz::SurfaceRange> surfaces_set;
  // |fallback_surfaces| and |primary_surfaces| should have same size
  for (size_t i = 0; i < fallback_surfaces.size(); ++i) {
    surfaces_set.insert(
        viz::SurfaceRange(fallback_surfaces[i], primary_surfaces[i]));
  }

  host_impl_->active_tree()->SetSurfaceRanges(std::move(surfaces_set));
  host_impl_->SetFullViewportDamage();
  DrawFrame();

  {
    auto* fake_layer_tree_frame_sink = static_cast<FakeLayerTreeFrameSink*>(
        host_impl_->layer_tree_frame_sink());
    const viz::CompositorFrameMetadata& metadata =
        fake_layer_tree_frame_sink->last_sent_frame()->metadata;
    EXPECT_THAT(metadata.activation_dependencies,
                testing::UnorderedElementsAre(primary_surfaces[0],
                                              primary_surfaces[1]));
    EXPECT_THAT(
        metadata.referenced_surfaces,
        testing::UnorderedElementsAre(
            viz::SurfaceRange(fallback_surfaces[0], primary_surfaces[0]),
            viz::SurfaceRange(fallback_surfaces[1], primary_surfaces[1]),
            viz::SurfaceRange(fallback_surfaces[2], primary_surfaces[2])));
    EXPECT_EQ(2u, metadata.deadline.deadline_in_frames());
    EXPECT_FALSE(metadata.deadline.use_default_lower_bound_deadline());
  }

  // Verify that on the next frame generation that the deadline is reset.
  host_impl_->SetFullViewportDamage();
  DrawFrame();

  {
    auto* fake_layer_tree_frame_sink = static_cast<FakeLayerTreeFrameSink*>(
        host_impl_->layer_tree_frame_sink());
    const viz::CompositorFrameMetadata& metadata =
        fake_layer_tree_frame_sink->last_sent_frame()->metadata;
    EXPECT_THAT(metadata.activation_dependencies,
                testing::UnorderedElementsAre(primary_surfaces[0],
                                              primary_surfaces[1]));
    EXPECT_THAT(
        metadata.referenced_surfaces,
        testing::UnorderedElementsAre(
            viz::SurfaceRange(fallback_surfaces[0], primary_surfaces[0]),
            viz::SurfaceRange(fallback_surfaces[1], primary_surfaces[1]),
            viz::SurfaceRange(fallback_surfaces[2], primary_surfaces[2])));
    EXPECT_EQ(0u, metadata.deadline.deadline_in_frames());
    EXPECT_FALSE(metadata.deadline.use_default_lower_bound_deadline());
  }
}

// Verify that updating the set of referenced surfaces for the active tree
// causes a new CompositorFrame to be submitted, even if there is no other
// damage.
TEST_F(LayerTreeHostImplTest, SurfaceReferencesChangeCausesDamage) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());

  // Submit an initial CompositorFrame with an empty set of referenced surfaces.
  host_impl_->active_tree()->SetSurfaceRanges({});
  host_impl_->SetFullViewportDamage();
  DrawFrame();

  {
    const viz::CompositorFrameMetadata& metadata =
        fake_layer_tree_frame_sink->last_sent_frame()->metadata;
    EXPECT_THAT(metadata.referenced_surfaces, testing::IsEmpty());
  }

  const viz::SurfaceId surface_id = MakeSurfaceId(viz::FrameSinkId(1, 1), 1);

  // Update the set of referenced surfaces to contain |surface_id| but don't
  // make any other changes that would cause damage. This mimics updating the
  // SurfaceLayer for an offscreen tab.
  host_impl_->active_tree()->SetSurfaceRanges({viz::SurfaceRange(surface_id)});
  DrawFrame();

  {
    const viz::CompositorFrameMetadata& metadata =
        fake_layer_tree_frame_sink->last_sent_frame()->metadata;
    EXPECT_THAT(metadata.referenced_surfaces,
                testing::UnorderedElementsAre(viz::SurfaceRange(surface_id)));
  }
}

TEST_F(LayerTreeHostImplTest, CompositorFrameMetadata) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);
  DrawFrame();
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(), metadata.root_scroll_offset);
    EXPECT_EQ(1, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(50, 50), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
  }

  // Scrolling should update metadata immediately.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kWheel)
                               .get());
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0, 10), metadata.root_scroll_offset);
  }
  host_impl_->ScrollEnd();
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0, 10), metadata.root_scroll_offset);
  }

  // Page scale should update metadata correctly (shrinking only the viewport).
  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, gfx::Point());
  host_impl_->PinchGestureEnd(gfx::Point(), true);
  host_impl_->ScrollEnd();
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0, 10), metadata.root_scroll_offset);
    EXPECT_EQ(2, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(25, 25), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
  }

  // Likewise if set from the main thread.
  host_impl_->ProcessScrollDeltas();
  host_impl_->active_tree()->PushPageScaleFromMainThread(4, 0.5f, 4);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(4);
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0, 10), metadata.root_scroll_offset);
    EXPECT_EQ(4, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(12.5f, 12.5f), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
  }
}

class DidDrawCheckLayer : public LayerImpl {
 public:
  static std::unique_ptr<DidDrawCheckLayer> Create(LayerTreeImpl* tree_impl,
                                                   int id) {
    return base::WrapUnique(new DidDrawCheckLayer(tree_impl, id));
  }

  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* provider) override {
    if (!LayerImpl::WillDraw(draw_mode, provider))
      return false;
    if (will_draw_returns_false_)
      return false;
    will_draw_returned_true_ = true;
    return true;
  }

  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    append_quads_called_ = true;
    LayerImpl::AppendQuads(render_pass, append_quads_data);
  }

  void DidDraw(viz::ClientResourceProvider* provider) override {
    did_draw_called_ = true;
    LayerImpl::DidDraw(provider);
  }

  bool will_draw_returned_true() const { return will_draw_returned_true_; }
  bool append_quads_called() const { return append_quads_called_; }
  bool did_draw_called() const { return did_draw_called_; }

  void set_will_draw_returns_false() { will_draw_returns_false_ = true; }

  void ClearDidDrawCheck() {
    will_draw_returned_true_ = false;
    append_quads_called_ = false;
    did_draw_called_ = false;
  }

 protected:
  DidDrawCheckLayer(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id),
        will_draw_returns_false_(false),
        will_draw_returned_true_(false),
        append_quads_called_(false),
        did_draw_called_(false) {
    SetBounds(gfx::Size(10, 10));
    SetDrawsContent(true);
    draw_properties().visible_layer_rect = gfx::Rect(0, 0, 10, 10);
  }

 private:
  bool will_draw_returns_false_;
  bool will_draw_returned_true_;
  bool append_quads_called_;
  bool did_draw_called_;
};

TEST_F(LayerTreeHostImplTest, DamageShouldNotCareAboutContributingLayers) {
  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));

  // Make a child layer that draws.
  auto* layer = AddLayer<SolidColorLayerImpl>(host_impl_->active_tree());
  layer->SetBounds(gfx::Size(10, 10));
  layer->SetDrawsContent(true);
  layer->SetBackgroundColor(SK_ColorRED);
  CopyProperties(root, layer);

  UpdateDrawProperties(host_impl_->active_tree());

  {
    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

    EXPECT_FALSE(frame.has_no_damage);
    EXPECT_NE(frame.render_passes.size(), 0u);
    size_t total_quad_count = 0;
    for (const auto& pass : frame.render_passes)
      total_quad_count += pass->quad_list.size();
    EXPECT_NE(total_quad_count, 0u);
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }

  // Stops the child layer from drawing. We should have damage from this but
  // should not have any quads. This should clear the damaged area.
  layer->SetDrawsContent(false);
  GetEffectNode(root)->opacity = 0;

  UpdateDrawProperties(host_impl_->active_tree());
  // The background is default to transparent. If the background is opaque, we
  // would fill the frame with background colour when no layers are contributing
  // quads. This means we would end up with 0 quad.
  EXPECT_EQ(host_impl_->active_tree()->background_color(), SK_ColorTRANSPARENT);

  {
    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

    EXPECT_FALSE(frame.has_no_damage);
    EXPECT_NE(frame.render_passes.size(), 0u);
    size_t total_quad_count = 0;
    for (const auto& pass : frame.render_passes)
      total_quad_count += pass->quad_list.size();
    EXPECT_EQ(total_quad_count, 0u);
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }

  // Now tries to draw again. Nothing changes, so should have no damage, no
  // render pass, and no quad.
  {
    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

    EXPECT_TRUE(frame.has_no_damage);
    EXPECT_EQ(frame.render_passes.size(), 0u);
    size_t total_quad_count = 0;
    for (const auto& pass : frame.render_passes)
      total_quad_count += pass->quad_list.size();
    EXPECT_EQ(total_quad_count, 0u);
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }
}

TEST_F(LayerTreeHostImplTest, WillDrawReturningFalseDoesNotCall) {
  // The root layer is always drawn, so run this test on a child layer that
  // will be masked out by the root layer's bounds.
  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));
  auto* layer = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  CopyProperties(root, layer);

  DrawFrame();
  EXPECT_TRUE(layer->will_draw_returned_true());
  EXPECT_TRUE(layer->append_quads_called());
  EXPECT_TRUE(layer->did_draw_called());

  host_impl_->SetViewportDamage(gfx::Rect(10, 10));

  layer->set_will_draw_returns_false();
  layer->ClearDidDrawCheck();
  DrawFrame();
  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->append_quads_called());
  EXPECT_FALSE(layer->did_draw_called());
}

TEST_F(LayerTreeHostImplTest, DidDrawNotCalledOnHiddenLayer) {
  // The root layer is always drawn, so run this test on a child layer that
  // will be masked out by the root layer's bounds.
  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));
  CreateClipNode(root);
  auto* layer = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  layer->SetBounds(gfx::Size(10, 10));
  CopyProperties(root, layer);
  // Ensure visible_layer_rect for layer is not empty
  layer->SetOffsetToTransformParent(gfx::Vector2dF(100, 100));
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->did_draw_called());

  DrawFrame();

  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->did_draw_called());

  EXPECT_TRUE(layer->visible_layer_rect().IsEmpty());

  // Ensure visible_layer_rect for layer is not empty
  layer->SetOffsetToTransformParent(gfx::Vector2dF());
  layer->NoteLayerPropertyChanged();
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->did_draw_called());

  DrawFrame();

  EXPECT_TRUE(layer->will_draw_returned_true());
  EXPECT_TRUE(layer->did_draw_called());

  EXPECT_FALSE(layer->visible_layer_rect().IsEmpty());
}

TEST_F(LayerTreeHostImplTest, WillDrawNotCalledOnOccludedLayer) {
  gfx::Size big_size(1000, 1000);
  auto* root =
      SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(), big_size);

  auto* occluded_layer = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  CopyProperties(root, occluded_layer);
  auto* top_layer = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  // This layer covers the occluded_layer above. Make this layer large so it can
  // occlude.
  top_layer->SetBounds(big_size);
  top_layer->SetContentsOpaque(true);
  CopyProperties(occluded_layer, top_layer);
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(occluded_layer->will_draw_returned_true());
  EXPECT_FALSE(occluded_layer->did_draw_called());
  EXPECT_FALSE(top_layer->will_draw_returned_true());
  EXPECT_FALSE(top_layer->did_draw_called());

  DrawFrame();

  EXPECT_FALSE(occluded_layer->will_draw_returned_true());
  EXPECT_FALSE(occluded_layer->did_draw_called());
  EXPECT_TRUE(top_layer->will_draw_returned_true());
  EXPECT_TRUE(top_layer->did_draw_called());
}

TEST_F(LayerTreeHostImplTest, DidDrawCalledOnAllLayers) {
  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));
  auto* layer1 = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  auto* layer2 = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());

  CopyProperties(root, layer1);
  CreateTransformNode(layer1).flattens_inherited_transform = true;
  CreateEffectNode(layer1).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(layer1, layer2);

  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(root->did_draw_called());
  EXPECT_FALSE(layer1->did_draw_called());
  EXPECT_FALSE(layer2->did_draw_called());

  DrawFrame();

  EXPECT_TRUE(root->did_draw_called());
  EXPECT_TRUE(layer1->did_draw_called());
  EXPECT_TRUE(layer2->did_draw_called());

  EXPECT_NE(GetRenderSurface(root), GetRenderSurface(layer1));
  EXPECT_TRUE(GetRenderSurface(layer1));
}

class MissingTextureAnimatingLayer : public DidDrawCheckLayer {
 public:
  static std::unique_ptr<MissingTextureAnimatingLayer> Create(
      LayerTreeImpl* tree_impl,
      int id,
      bool tile_missing,
      bool had_incomplete_tile,
      bool animating,
      scoped_refptr<AnimationTimeline> timeline) {
    return base::WrapUnique(new MissingTextureAnimatingLayer(
        tree_impl, id, tile_missing, had_incomplete_tile, animating, timeline));
  }

  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    LayerImpl::AppendQuads(render_pass, append_quads_data);
    if (had_incomplete_tile_)
      append_quads_data->num_incomplete_tiles++;
    if (tile_missing_)
      append_quads_data->num_missing_tiles++;
  }

 private:
  MissingTextureAnimatingLayer(LayerTreeImpl* tree_impl,
                               int id,
                               bool tile_missing,
                               bool had_incomplete_tile,
                               bool animating,
                               scoped_refptr<AnimationTimeline> timeline)
      : DidDrawCheckLayer(tree_impl, id),
        tile_missing_(tile_missing),
        had_incomplete_tile_(had_incomplete_tile) {
    if (animating) {
      this->SetElementId(LayerIdToElementIdForTesting(id));
      AddAnimatedTransformToElementWithAnimation(this->element_id(), timeline,
                                                 10.0, 3, 0);
    }
  }

  bool tile_missing_;
  bool had_incomplete_tile_;
};

struct PrepareToDrawSuccessTestCase {
  explicit PrepareToDrawSuccessTestCase(DrawResult result)
      : expected_result(result) {}

  struct State {
    bool has_missing_tile = false;
    bool has_incomplete_tile = false;
    bool is_animating = false;
  };

  bool high_res_required = false;
  State layer_before;
  State layer_between;
  State layer_after;
  DrawResult expected_result;
};

class LayerTreeHostImplPrepareToDrawTest : public LayerTreeHostImplTest {
 public:
  void CreateLayerFromState(DidDrawCheckLayer* root,
                            const scoped_refptr<AnimationTimeline>& timeline,
                            const PrepareToDrawSuccessTestCase::State& state) {
    auto* layer = AddLayer<MissingTextureAnimatingLayer>(
        root->layer_tree_impl(), state.has_missing_tile,
        state.has_incomplete_tile, state.is_animating, timeline);
    CopyProperties(root, layer);
    if (state.is_animating)
      CreateTransformNode(layer).has_potential_animation = true;
  }
};

TEST_F(LayerTreeHostImplPrepareToDrawTest, PrepareToDrawSucceedsAndFails) {
  LayerTreeSettings settings = DefaultSettings();
  settings.commit_to_active_tree = false;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  std::vector<PrepareToDrawSuccessTestCase> cases;
  // 0. Default case.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  // 1. Animated layer first.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_before.is_animating = true;
  // 2. Animated layer between.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_between.is_animating = true;
  // 3. Animated layer last.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_after.is_animating = true;
  // 4. Missing tile first.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_before.has_missing_tile = true;
  // 5. Missing tile between.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_between.has_missing_tile = true;
  // 6. Missing tile last.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_after.has_missing_tile = true;
  // 7. Incomplete tile first.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_before.has_incomplete_tile = true;
  // 8. Incomplete tile between.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_between.has_incomplete_tile = true;
  // 9. Incomplete tile last.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_after.has_incomplete_tile = true;
  // 10. Animation with missing tile.
  cases.push_back(
      PrepareToDrawSuccessTestCase(DRAW_ABORTED_CHECKERBOARD_ANIMATIONS));
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_between.is_animating = true;
  // 11. Animation with incomplete tile.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_between.has_incomplete_tile = true;
  cases.back().layer_between.is_animating = true;

  // 12. High res required.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().high_res_required = true;
  // 13. High res required with incomplete tile.
  cases.push_back(
      PrepareToDrawSuccessTestCase(DRAW_ABORTED_MISSING_HIGH_RES_CONTENT));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_incomplete_tile = true;
  // 14. High res required with missing tile.
  cases.push_back(
      PrepareToDrawSuccessTestCase(DRAW_ABORTED_MISSING_HIGH_RES_CONTENT));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;

  // 15. High res required is higher priority than animating missing tiles.
  cases.push_back(
      PrepareToDrawSuccessTestCase(DRAW_ABORTED_MISSING_HIGH_RES_CONTENT));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_after.has_missing_tile = true;
  cases.back().layer_after.is_animating = true;
  // 16. High res required is higher priority than animating missing tiles.
  cases.push_back(
      PrepareToDrawSuccessTestCase(DRAW_ABORTED_MISSING_HIGH_RES_CONTENT));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_before.has_missing_tile = true;
  cases.back().layer_before.is_animating = true;

  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

  DrawFrame();

  for (size_t i = 0; i < cases.size(); ++i) {
    // Clean up host_impl_ state.
    const auto& testcase = cases[i];
    host_impl_->active_tree()->DetachLayersKeepingRootLayerForTesting();
    timeline()->ClearAnimations();

    std::ostringstream scope;
    scope << "Test case: " << i;
    SCOPED_TRACE(scope.str());

    CreateLayerFromState(root, timeline(), testcase.layer_before);
    CreateLayerFromState(root, timeline(), testcase.layer_between);
    CreateLayerFromState(root, timeline(), testcase.layer_after);
    UpdateDrawProperties(host_impl_->active_tree());

    if (testcase.high_res_required)
      host_impl_->SetRequiresHighResToDraw();

    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(testcase.expected_result, host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }
}

TEST_F(LayerTreeHostImplPrepareToDrawTest,
       PrepareToDrawWhenDrawAndSwapFullViewportEveryFrame) {
  CreateHostImpl(DefaultSettings(), FakeLayerTreeFrameSink::CreateSoftware());

  const gfx::Transform external_transform;
  const gfx::Rect external_viewport;
  const bool resourceless_software_draw = true;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  std::vector<PrepareToDrawSuccessTestCase> cases;

  // 0. Default case.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  // 1. Animation with missing tile.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_between.is_animating = true;
  // 2. High res required with incomplete tile.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_incomplete_tile = true;
  // 3. High res required with missing tile.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;

  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);

  for (size_t i = 0; i < cases.size(); ++i) {
    const auto& testcase = cases[i];
    host_impl_->active_tree()->DetachLayersKeepingRootLayerForTesting();

    std::ostringstream scope;
    scope << "Test case: " << i;
    SCOPED_TRACE(scope.str());

    CreateLayerFromState(root, timeline(), testcase.layer_before);
    CreateLayerFromState(root, timeline(), testcase.layer_between);
    CreateLayerFromState(root, timeline(), testcase.layer_after);

    if (testcase.high_res_required)
      host_impl_->SetRequiresHighResToDraw();

    host_impl_->OnDraw(external_transform, external_viewport,
                       resourceless_software_draw, false);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollRootIgnored) {
  SetupDefaultRootLayer(gfx::Size(10, 10));
  DrawFrame();

  // Scroll event is ignored because layer is not scrollable.
  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ClampingAfterActivation) {
  CreatePendingTree();
  host_impl_->pending_tree()->PushPageScaleFromMainThread(1, 1, 1);
  SetupViewportLayers(host_impl_->pending_tree(), gfx::Size(50, 50),
                      gfx::Size(100, 100), gfx::Size(100, 100));
  host_impl_->ActivateSyncTree();

  CreatePendingTree();
  const gfx::ScrollOffset pending_scroll = gfx::ScrollOffset(-100, -100);
  LayerImpl* active_outer_layer = OuterViewportScrollLayer();
  LayerImpl* pending_outer_layer =
      host_impl_->pending_tree()->OuterViewportScrollLayerForTesting();
  pending_outer_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          pending_outer_layer->element_id(), pending_scroll);

  host_impl_->ActivateSyncTree();
  // Scrolloffsets on the active tree will be clamped after activation.
  EXPECT_EQ(CurrentScrollOffset(active_outer_layer), gfx::ScrollOffset(0, 0));
}

class LayerTreeHostImplBrowserControlsTest : public LayerTreeHostImplTest {
 public:
  LayerTreeHostImplBrowserControlsTest()
      // Make the clip size the same as the layer (content) size so the layer is
      // non-scrollable.
      : layer_size_(10, 10), clip_size_(layer_size_), top_controls_height_(50) {
    viewport_size_ = gfx::Size(clip_size_.width(),
                               clip_size_.height() + top_controls_height_);
  }

  bool CreateHostImpl(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) override {
    bool init = LayerTreeHostImplTest::CreateHostImpl(
        settings, std::move(layer_tree_frame_sink));
    if (init) {
      host_impl_->active_tree()->SetBrowserControlsParams(
          {top_controls_height_, 0, 0, 0, false, false});
      host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1.f, 1.f);
      host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
    }
    return init;
  }

 protected:
  void SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      const gfx::Size& inner_viewport_size,
      const gfx::Size& outer_viewport_size,
      const gfx::Size& scroll_layer_size) {
    SetupBrowserControlsAndScrollLayerWithVirtualViewport(
        host_impl_->active_tree(), inner_viewport_size, outer_viewport_size,
        scroll_layer_size);
  }

  void SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      LayerTreeImpl* tree_impl,
      const gfx::Size& inner_viewport_size,
      const gfx::Size& outer_viewport_size,
      const gfx::Size& scroll_layer_size) {
    tree_impl->SetBrowserControlsParams(
        {top_controls_height_, 0, 0, 0, false, true});
    tree_impl->SetCurrentBrowserControlsShownRatio(1.f, 1.f);
    tree_impl->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
    host_impl_->DidChangeBrowserControlsPosition();

    SetupViewportLayers(tree_impl, inner_viewport_size, outer_viewport_size,
                        scroll_layer_size);
  }

  gfx::Size layer_size_;
  gfx::Size clip_size_;
  gfx::Size viewport_size_;
  float top_controls_height_;

  LayerTreeSettings settings_;
};  // class LayerTreeHostImplBrowserControlsTest

#define EXPECT_VIEWPORT_GEOMETRIES(expected_browser_controls_shown_ratio)    \
  do {                                                                       \
    auto* tree = host_impl_->active_tree();                                  \
    auto* property_trees = tree->property_trees();                           \
    EXPECT_EQ(expected_browser_controls_shown_ratio,                         \
              tree->CurrentTopControlsShownRatio());                         \
    EXPECT_EQ(                                                               \
        tree->top_controls_height() * expected_browser_controls_shown_ratio, \
        host_impl_->browser_controls_manager()->ContentTopOffset());         \
    int delta =                                                              \
        (tree->top_controls_height() + tree->bottom_controls_height()) *     \
        (1 - expected_browser_controls_shown_ratio);                         \
    int scaled_delta = delta / tree->min_page_scale_factor();                \
    gfx::Size inner_scroll_bounds = tree->InnerViewportScrollNode()->bounds; \
    inner_scroll_bounds.Enlarge(0, scaled_delta);                            \
    EXPECT_EQ(inner_scroll_bounds, InnerViewportScrollLayer()->bounds());    \
    EXPECT_EQ(gfx::RectF(gfx::SizeF(inner_scroll_bounds)),                   \
              tree->OuterViewportClipNode()->clip);                          \
    EXPECT_EQ(gfx::Vector2dF(0, delta),                                      \
              property_trees->inner_viewport_container_bounds_delta());      \
    EXPECT_EQ(gfx::Vector2dF(0, scaled_delta),                               \
              property_trees->outer_viewport_container_bounds_delta());      \
  } while (false)

// Tests that, on a page with content the same size as the viewport, hiding
// the browser controls also increases the ScrollableSize (i.e. the content
// size). Since the viewport got larger, the effective scrollable "content" also
// did. This ensures, for one thing, that the overscroll glow is shown in the
// right place.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       HidingBrowserControlsExpandsScrollableSize) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(50, 50), gfx::Size(50, 50), gfx::Size(50, 50));

  LayerTreeImpl* active_tree = host_impl_->active_tree();
  DrawFrame();

  // The browser controls should start off showing so the viewport should be
  // shrunk.
  EXPECT_VIEWPORT_GEOMETRIES(1);
  EXPECT_EQ(gfx::SizeF(50, 50), active_tree->ScrollableSize());

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 25),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);

  host_impl_->browser_controls_manager()->ScrollBegin();

  // Hide the browser controls by a bit, the scrollable size should increase but
  // the actual content bounds shouldn't.
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
  EXPECT_VIEWPORT_GEOMETRIES(0.5f);
  EXPECT_EQ(gfx::SizeF(50, 75), active_tree->ScrollableSize());

  // Fully hide the browser controls.
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
  EXPECT_VIEWPORT_GEOMETRIES(0);
  EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());

  // Scrolling additionally shouldn't have any effect.
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
  EXPECT_VIEWPORT_GEOMETRIES(0);
  EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());

  host_impl_->browser_controls_manager()->ScrollEnd();
  host_impl_->ScrollEnd();
}

// Ensure that moving the browser controls (i.e. omnibox/url-bar on mobile) on
// pages with a non-1 minimum page scale factor (e.g. legacy desktop page)
// correctly scales the clipping adjustment performed to show the newly exposed
// region of the page.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       MovingBrowserControlsOuterClipDeltaScaled) {
  gfx::Size inner_size = gfx::Size(100, 100);
  gfx::Size outer_size = gfx::Size(100, 100);
  gfx::Size content_size = gfx::Size(200, 1000);
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(inner_size, outer_size,
                                                        content_size);

  LayerTreeImpl* active_tree = host_impl_->active_tree();

  // Create a content layer beneath the outer viewport scroll layer.
  LayerImpl* content = AddLayer();
  content->SetBounds(gfx::Size(100, 100));
  CopyProperties(OuterViewportScrollLayer(), content);
  active_tree->PushPageScaleFromMainThread(0.5f, 0.5f, 4);

  DrawFrame();

  // The browser controls should start off showing so the viewport should be
  // shrunk.
  EXPECT_VIEWPORT_GEOMETRIES(1.0f);
  EXPECT_EQ(gfx::SizeF(200, 1000), active_tree->ScrollableSize());

  ASSERT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 25),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);

  // Hide the browser controls by 25px. The outer clip should expand by 50px as
  // because the outer viewport is sized based on the minimum scale, in this
  // case 0.5. Therefore, changes to the outer viewport need to be divided by
  // the minimum scale as well.
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0), gfx::Vector2dF(0, 25),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  EXPECT_VIEWPORT_GEOMETRIES(0.5f);

  host_impl_->ScrollEnd();
}

// Tests that browser controls affect the position of horizontal scrollbars.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       HidingBrowserControlsAdjustsScrollbarPosition) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(50, 50), gfx::Size(50, 50), gfx::Size(50, 50));

  LayerTreeImpl* active_tree = host_impl_->active_tree();

  // Create a horizontal scrollbar.
  gfx::Size scrollbar_size(gfx::Size(50, 15));
  auto* scrollbar_layer = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), HORIZONTAL, 3, 20, false);
  SetupScrollbarLayer(OuterViewportScrollLayer(), scrollbar_layer);
  scrollbar_layer->SetBounds(scrollbar_size);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kNone, gfx::Rect(scrollbar_size));
  scrollbar_layer->SetTouchActionRegion(touch_action_region);
  scrollbar_layer->SetOffsetToTransformParent(gfx::Vector2dF(0, 35));
  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->UpdateScrollbarGeometries();

  DrawFrame();

  // The browser controls should start off showing so the viewport should be
  // shrunk.
  EXPECT_VIEWPORT_GEOMETRIES(1.0f);
  EXPECT_EQ(gfx::SizeF(50, 50), active_tree->ScrollableSize());
  EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
  EXPECT_EQ(gfx::Rect(20, 0, 10, 3), scrollbar_layer->ComputeThumbQuadRect());

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 25),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);

  host_impl_->browser_controls_manager()->ScrollBegin();

  // Hide the browser controls by a bit, the scrollable size should increase but
  // the actual content bounds shouldn't.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
    host_impl_->active_tree()->UpdateScrollbarGeometries();
    EXPECT_VIEWPORT_GEOMETRIES(0.5f);
    EXPECT_EQ(gfx::SizeF(50, 75), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
    EXPECT_EQ(gfx::Rect(20, 25, 10, 3),
              scrollbar_layer->ComputeThumbQuadRect());
  }

  // Fully hide the browser controls.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
    host_impl_->active_tree()->UpdateScrollbarGeometries();
    EXPECT_VIEWPORT_GEOMETRIES(0);
    EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
    EXPECT_EQ(gfx::Rect(20, 50, 10, 3),
              scrollbar_layer->ComputeThumbQuadRect());
  }

  // Additional scrolling shouldn't have any effect.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
    EXPECT_VIEWPORT_GEOMETRIES(0);
    EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
    EXPECT_EQ(gfx::Rect(20, 50, 10, 3),
              scrollbar_layer->ComputeThumbQuadRect());
  }

  host_impl_->browser_controls_manager()->ScrollEnd();
  host_impl_->ScrollEnd();
}

TEST_F(LayerTreeHostImplBrowserControlsTest,
       ScrollBrowserControlsByFractionalAmount) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(10, 10), gfx::Size(10, 10), gfx::Size(10, 10));
  DrawFrame();

  gfx::Vector2dF top_controls_scroll_delta(0, 5.25f);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(), top_controls_scroll_delta,
                                   ui::ScrollInputType::kTouchscreen)
                            .get(),
                        ui::ScrollInputType::kTouchscreen)
          .thread);

  // Make the test scroll delta a fractional amount, to verify that the
  // fixed container size delta is (1) non-zero, and (2) fractional, and
  // (3) matches the movement of the browser controls.
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  host_impl_->browser_controls_manager()->ScrollEnd();

  host_impl_->ScrollEnd();
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_FLOAT_EQ(top_controls_scroll_delta.y(),
                  property_trees->inner_viewport_container_bounds_delta().y());
}

// In this test, the outer viewport is initially unscrollable. We test that a
// scroll initiated on the inner viewport, causing the browser controls to show
// and thus making the outer viewport scrollable, still scrolls the outer
// viewport.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsOuterViewportBecomesScrollable) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(10, 50), gfx::Size(10, 50), gfx::Size(10, 100));
  DrawFrame();

  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  inner_scroll->SetDrawsContent(true);
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  outer_scroll->SetDrawsContent(true);

  // Need SetDrawsContent so ScrollBegin's hit test finds an actual layer.
  outer_scroll->SetDrawsContent(true);
  host_impl_->active_tree()->PushPageScaleFromMainThread(2, 1, 2);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 50),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0, 50),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());

  // The entire scroll delta should have been used to hide the browser controls.
  // The viewport layers should be resized back to their full sizes.
  EXPECT_EQ(0, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(0, CurrentScrollOffset(inner_scroll).y());
  EXPECT_EQ(100, inner_scroll->bounds().height());
  EXPECT_EQ(100, outer_scroll->bounds().height());

  // The inner viewport should be scrollable by 50px * page_scale.
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0, 100),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  EXPECT_EQ(50, CurrentScrollOffset(inner_scroll).y());
  EXPECT_EQ(0, CurrentScrollOffset(outer_scroll).y());
  EXPECT_EQ(gfx::ScrollOffset(), MaxScrollOffset(outer_scroll));

  host_impl_->ScrollEnd();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, -50),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
            outer_scroll->scroll_tree_index());

  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0, -50),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());

  // The entire scroll delta should have been used to show the browser controls.
  // The outer viewport should be resized to accomodate and scrolled to the
  // bottom of the document to keep the viewport in place.
  EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(50, inner_scroll->bounds().height());
  EXPECT_EQ(100, outer_scroll->bounds().height());
  EXPECT_EQ(25, CurrentScrollOffset(outer_scroll).y());
  EXPECT_EQ(25, CurrentScrollOffset(inner_scroll).y());

  // Now when we continue scrolling, make sure the outer viewport gets scrolled
  // since it wasn't scrollable when the scroll began.
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0, -20),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  EXPECT_EQ(25, CurrentScrollOffset(outer_scroll).y());
  EXPECT_EQ(15, CurrentScrollOffset(inner_scroll).y());

  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0, -30),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  EXPECT_EQ(25, CurrentScrollOffset(outer_scroll).y());
  EXPECT_EQ(0, CurrentScrollOffset(inner_scroll).y());

  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0.f, -50),
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  host_impl_->ScrollEnd();

  EXPECT_EQ(0, CurrentScrollOffset(outer_scroll).y());
  EXPECT_EQ(0, CurrentScrollOffset(inner_scroll).y());
}

// Test that the fixed position container delta is appropriately adjusted
// by the browser controls showing/hiding and page scale doesn't affect it.
TEST_F(LayerTreeHostImplBrowserControlsTest, FixedContainerDelta) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(100, 100), gfx::Size(100, 100));
  DrawFrame();
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);

  float page_scale = 1.5f;
  // Zoom in, since the fixed container is the outer viewport, the delta should
  // not be scaled.
  host_impl_->active_tree()->PushPageScaleFromMainThread(page_scale, 1, 2);

  gfx::Vector2dF top_controls_scroll_delta(0, 20);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(), top_controls_scroll_delta,
                                   ui::ScrollInputType::kTouchscreen)
                            .get(),
                        ui::ScrollInputType::kTouchscreen)
          .thread);

  // Scroll down, the browser controls hiding should expand the viewport size so
  // the delta should be equal to the scroll distance.
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  EXPECT_FLOAT_EQ(top_controls_height_ - top_controls_scroll_delta.y(),
                  host_impl_->browser_controls_manager()->ContentTopOffset());

  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_FLOAT_EQ(top_controls_scroll_delta.y(),
                  property_trees->outer_viewport_container_bounds_delta().y());
  host_impl_->ScrollEnd();

  // Scroll past the maximum extent. The delta shouldn't be greater than the
  // browser controls height.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(), top_controls_scroll_delta,
                                   ui::ScrollInputType::kTouchscreen)
                            .get(),
                        ui::ScrollInputType::kTouchscreen)
          .thread);
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, top_controls_height_),
                   property_trees->outer_viewport_container_bounds_delta());
  host_impl_->ScrollEnd();

  // Scroll in the direction to make the browser controls show.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(), -top_controls_scroll_delta,
                                   ui::ScrollInputType::kTouchscreen)
                            .get(),
                        ui::ScrollInputType::kTouchscreen)
          .thread);
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(-top_controls_scroll_delta);
  EXPECT_EQ(top_controls_scroll_delta.y(),
            host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_VECTOR_EQ(
      gfx::Vector2dF(0, top_controls_height_ - top_controls_scroll_delta.y()),
      property_trees->outer_viewport_container_bounds_delta());
  host_impl_->browser_controls_manager()->ScrollEnd();
  host_impl_->ScrollEnd();
}

// Push a browser controls ratio from the main thread that we didn't send as a
// delta and make sure that the ratio is clamped to the [0, 1] range.
TEST_F(LayerTreeHostImplBrowserControlsTest, BrowserControlsPushUnsentRatio) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(10, 50), gfx::Size(10, 50), gfx::Size(10, 100));
  DrawFrame();

  // Need SetDrawsContent so ScrollBegin's hit test finds an actual layer.
  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  inner_scroll->SetDrawsContent(true);
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  outer_scroll->SetDrawsContent(true);

  host_impl_->active_tree()->PushBrowserControlsFromMainThread(1, 1);
  ASSERT_EQ(1.0f, host_impl_->active_tree()->CurrentTopControlsShownRatio());

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0.5f, 0.5f);
  ASSERT_EQ(0.5f, host_impl_->active_tree()->CurrentTopControlsShownRatio());

  host_impl_->active_tree()->PushBrowserControlsFromMainThread(0, 0);

  ASSERT_EQ(0, host_impl_->active_tree()->CurrentTopControlsShownRatio());
}

// Test that if a scrollable sublayer doesn't consume the scroll,
// browser controls should hide when scrolling down.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsScrollableSublayer) {
  gfx::Size sub_content_size(100, 400);
  gfx::Size sub_content_layer_size(100, 300);
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 50), gfx::Size(100, 100), gfx::Size(100, 100));
  DrawFrame();

  // Show browser controls
  EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());

  LayerImpl* outer_viewport_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* child = AddLayer();

  child->SetHitTestable(true);
  child->SetElementId(LayerIdToElementIdForTesting(child->id()));
  child->SetBounds(sub_content_size);
  child->SetDrawsContent(true);

  CopyProperties(outer_viewport_scroll_layer, child);
  CreateTransformNode(child);
  CreateScrollNode(child, sub_content_layer_size);
  UpdateDrawProperties(host_impl_->active_tree());

  // Scroll child to the limit.
  SetScrollOffsetDelta(child, gfx::Vector2dF(0, 100));

  // Scroll 25px to hide browser controls
  gfx::Vector2dF scroll_delta(0, 25);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen)
          .get());
  host_impl_->ScrollEnd();

  // Browser controls should be hidden
  EXPECT_EQ(scroll_delta.y(),
            top_controls_height_ -
                host_impl_->browser_controls_manager()->ContentTopOffset());
}

// Ensure setting the browser controls position explicitly using the setters on
// the TreeImpl correctly affects the browser controls manager and viewport
// bounds for the active tree.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       PositionBrowserControlsToActiveTreeExplicitly) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0, 0);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(
      30 / top_controls_height_);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushPendingToActive();
  EXPECT_FLOAT_EQ(30,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_FLOAT_EQ(-20,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0, 0);
  EXPECT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_FLOAT_EQ(-50,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());

  host_impl_->DidChangeBrowserControlsPosition();

  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());
}

// Ensure setting the browser controls position explicitly using the setters on
// the TreeImpl correctly affects the browser controls manager and viewport
// bounds for the pending tree.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       PositionBrowserControlsToPendingTreeExplicitly) {
  CreatePendingTree();
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      host_impl_->pending_tree(), layer_size_, layer_size_, layer_size_);

  // Changing SetCurrentBrowserControlsShownRatio is one way to cause the
  // pending tree to update it's viewport.
  host_impl_->SetCurrentBrowserControlsShownRatio(0, 0);
  EXPECT_FLOAT_EQ(top_controls_height_,
                  host_impl_->pending_tree()
                      ->property_trees()
                      ->inner_viewport_container_bounds_delta()
                      .y());
  host_impl_->SetCurrentBrowserControlsShownRatio(0.5f, 0.5f);
  EXPECT_FLOAT_EQ(0.5f * top_controls_height_,
                  host_impl_->pending_tree()
                      ->property_trees()
                      ->inner_viewport_container_bounds_delta()
                      .y());
  host_impl_->SetCurrentBrowserControlsShownRatio(1, 1);
  EXPECT_FLOAT_EQ(0, host_impl_->pending_tree()
                         ->property_trees()
                         ->inner_viewport_container_bounds_delta()
                         .y());

  // Pushing changes from the main thread is the second way. These values are
  // added to the 1 set above.
  host_impl_->pending_tree()->PushBrowserControlsFromMainThread(-0.5f, -0.5f);
  EXPECT_FLOAT_EQ(0.5f * top_controls_height_,
                  host_impl_->pending_tree()
                      ->property_trees()
                      ->inner_viewport_container_bounds_delta()
                      .y());
  host_impl_->pending_tree()->PushBrowserControlsFromMainThread(-1, -1);
  EXPECT_FLOAT_EQ(top_controls_height_,
                  host_impl_->pending_tree()
                      ->property_trees()
                      ->inner_viewport_container_bounds_delta()
                      .y());
}

// Test that the top_controls delta and sent delta are appropriately
// applied on sync tree activation. The total browser controls offset shouldn't
// change after the activation.
TEST_F(LayerTreeHostImplBrowserControlsTest, ApplyDeltaOnTreeActivation) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(
      20 / top_controls_height_);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushPendingToActive();
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(
      15 / top_controls_height_, 15 / top_controls_height_);
  host_impl_->active_tree()
      ->top_controls_shown_ratio()
      ->PullDeltaForMainThread();
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0, 0);
  host_impl_->sync_tree()->PushBrowserControlsFromMainThread(
      15 / top_controls_height_, 15 / top_controls_height_);

  host_impl_->DidChangeBrowserControlsPosition();
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());

  host_impl_->ActivateSyncTree();

  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());
  EXPECT_FLOAT_EQ(
      -15, host_impl_->active_tree()->top_controls_shown_ratio()->Delta() *
               top_controls_height_);
  EXPECT_FLOAT_EQ(
      15, host_impl_->active_tree()->top_controls_shown_ratio()->ActiveBase() *
              top_controls_height_);
}

// Test that changing the browser controls layout height is correctly applied to
// the inner viewport container bounds. That is, the browser controls layout
// height is the amount that the inner viewport container was shrunk outside
// the compositor to accommodate the browser controls.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsLayoutHeightChanged) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  host_impl_->sync_tree()->PushBrowserControlsFromMainThread(1, 1);
  host_impl_->sync_tree()->SetBrowserControlsParams(
      {top_controls_height_, 0, 0, 0, false, true});

  host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(1);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushPendingToActive();
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0, 0);

  host_impl_->DidChangeBrowserControlsPosition();
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());

  host_impl_->ActivateSyncTree();

  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());

  // The total bounds should remain unchanged since the bounds delta should
  // account for the difference between the layout height and the current
  // browser controls offset.
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1, 1);
  host_impl_->DidChangeBrowserControlsPosition();

  EXPECT_EQ(1, host_impl_->browser_controls_manager()->TopControlsShownRatio());
  EXPECT_EQ(50, host_impl_->browser_controls_manager()->TopControlsHeight());
  EXPECT_EQ(50, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(gfx::Vector2dF(),
            property_trees->inner_viewport_container_bounds_delta());
}

// Test that showing/hiding the browser controls when the viewport is fully
// scrolled doesn't incorrectly change the viewport offset due to clamping from
// changing viewport bounds.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsViewportOffsetClamping) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(200, 200), gfx::Size(200, 400));
  DrawFrame();

  EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();

  // Scroll the viewports to max scroll offset.
  SetScrollOffsetDelta(outer_scroll, gfx::Vector2dF(0, 200));
  SetScrollOffsetDelta(inner_scroll, gfx::Vector2dF(100, 100));

  gfx::ScrollOffset viewport_offset =
      host_impl_->active_tree()->TotalScrollOffset();
  EXPECT_EQ(host_impl_->active_tree()->TotalMaxScrollOffset(), viewport_offset);

  // Hide the browser controls by 25px.
  gfx::Vector2dF scroll_delta(0, 25);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen)
          .get());

  // scrolling down at the max extents no longer hides the browser controls
  EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());

  // forcefully hide the browser controls by 25px
  host_impl_->browser_controls_manager()->ScrollBy(scroll_delta);
  host_impl_->ScrollEnd();

  EXPECT_FLOAT_EQ(
      scroll_delta.y(),
      top_controls_height_ -
          host_impl_->browser_controls_manager()->ContentTopOffset());

  // We should still be fully scrolled.
  EXPECT_EQ(host_impl_->active_tree()->TotalMaxScrollOffset(),
            host_impl_->active_tree()->TotalScrollOffset());

  viewport_offset = host_impl_->active_tree()->TotalScrollOffset();

  // Bring the browser controls down by 25px.
  scroll_delta = gfx::Vector2dF(0, -25);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen)
          .get());
  host_impl_->ScrollEnd();

  // The viewport offset shouldn't have changed.
  EXPECT_EQ(viewport_offset, host_impl_->active_tree()->TotalScrollOffset());

  // Scroll the viewports to max scroll offset.
  SetScrollOffsetDelta(outer_scroll, gfx::Vector2dF(0, 200));
  SetScrollOffsetDelta(inner_scroll, gfx::Vector2dF(100, 100));
  EXPECT_EQ(host_impl_->active_tree()->TotalMaxScrollOffset(),
            host_impl_->active_tree()->TotalScrollOffset());
}

// Test that the browser controls coming in and out maintains the same aspect
// ratio between the inner and outer viewports.
TEST_F(LayerTreeHostImplBrowserControlsTest, BrowserControlsAspectRatio) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(200, 200), gfx::Size(200, 400));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 2);
  DrawFrame();

  EXPECT_FLOAT_EQ(top_controls_height_,
                  host_impl_->browser_controls_manager()->ContentTopOffset());

  gfx::Vector2dF scroll_delta(0, 25);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen)
          .get());
  host_impl_->ScrollEnd();

  EXPECT_FLOAT_EQ(
      scroll_delta.y(),
      top_controls_height_ -
          host_impl_->browser_controls_manager()->ContentTopOffset());

  // Browser controls were hidden by 25px so the inner viewport should have
  // expanded by that much.
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_EQ(gfx::Vector2dF(0, 25),
            property_trees->inner_viewport_container_bounds_delta());

  // Outer viewport should match inner's aspect ratio. The bounds are ceiled.
  float aspect_ratio = 100.0f / 125.0f;
  auto expected = gfx::ToCeiledSize(gfx::SizeF(200, 200 / aspect_ratio));
  EXPECT_EQ(expected, InnerViewportScrollLayer()->bounds());
}

// Test that scrolling the outer viewport affects the browser controls.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsScrollOuterViewport) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(200, 200), gfx::Size(200, 400));
  DrawFrame();

  EXPECT_EQ(top_controls_height_,
            host_impl_->browser_controls_manager()->ContentTopOffset());

  // Send a gesture scroll that will scroll the outer viewport, make sure the
  // browser controls get scrolled.
  gfx::Vector2dF scroll_delta(0, 15);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen)
          .get());

  EXPECT_EQ(OuterViewportScrollLayer()->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->ScrollEnd();

  EXPECT_FLOAT_EQ(
      scroll_delta.y(),
      top_controls_height_ -
          host_impl_->browser_controls_manager()->ContentTopOffset());

  scroll_delta = gfx::Vector2dF(0, 50);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen)
          .get());

  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(OuterViewportScrollLayer()->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  host_impl_->ScrollEnd();

  // Position the viewports such that the inner viewport will be scrolled.
  gfx::Vector2dF inner_viewport_offset(0, 25);
  SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2dF());
  SetScrollOffsetDelta(InnerViewportScrollLayer(), inner_viewport_offset);

  scroll_delta = gfx::Vector2dF(0, -65);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen)
          .get());

  EXPECT_EQ(top_controls_height_,
            host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_FLOAT_EQ(
      inner_viewport_offset.y() + (scroll_delta.y() + top_controls_height_),
      ScrollDelta(InnerViewportScrollLayer()).y());

  host_impl_->ScrollEnd();
}

TEST_F(LayerTreeHostImplBrowserControlsTest,
       ScrollNonScrollableRootWithBrowserControls) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 50),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);

  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 50));
  host_impl_->browser_controls_manager()->ScrollEnd();
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  // Now that browser controls have moved, expect the clip to resize.
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());

  host_impl_->ScrollEnd();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, -25),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);

  float scroll_increment_y = -25;
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(
      gfx::Vector2dF(0, scroll_increment_y));
  EXPECT_FLOAT_EQ(-scroll_increment_y,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  // Now that browser controls have moved, expect the clip to resize.
  EXPECT_EQ(gfx::Vector2dF(0, 25),
            property_trees->inner_viewport_container_bounds_delta());

  host_impl_->browser_controls_manager()->ScrollBy(
      gfx::Vector2dF(0, scroll_increment_y));
  host_impl_->browser_controls_manager()->ScrollEnd();
  EXPECT_FLOAT_EQ(-2 * scroll_increment_y,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  // Now that browser controls have moved, expect the clip to resize.
  EXPECT_EQ(gfx::Vector2dF(),
            property_trees->inner_viewport_container_bounds_delta());

  host_impl_->ScrollEnd();

  // Verify the layer is once-again non-scrollable.
  EXPECT_EQ(gfx::ScrollOffset(), MaxScrollOffset(InnerViewportScrollLayer()));

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
}

// Tests that activating a pending tree while there's a bounds_delta on the
// viewport layers from browser controls doesn't cause a scroll jump. This bug
// was occurring because the UpdateViewportContainerSizes was being called
// before the property trees were updated with the bounds_delta.
// crbug.com/597266.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       ViewportBoundsDeltaOnTreeActivation) {
  const gfx::Size inner_viewport_size(1000, 1000);
  const gfx::Size outer_viewport_size(1000, 1000);
  const gfx::Size content_size(2000, 2000);

  // Initialization
  {
    SetupBrowserControlsAndScrollLayerWithVirtualViewport(
        inner_viewport_size, outer_viewport_size, content_size);
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 1);

    // Start off with the browser controls hidden on both main and impl.
    host_impl_->active_tree()->SetBrowserControlsParams(
        {top_controls_height_, 0, 0, 0, false, false});
    host_impl_->active_tree()->PushBrowserControlsFromMainThread(0, 0);

    CreatePendingTree();
    SetupBrowserControlsAndScrollLayerWithVirtualViewport(
        host_impl_->pending_tree(), inner_viewport_size, outer_viewport_size,
        content_size);
    host_impl_->pending_tree()->SetBrowserControlsParams(
        {top_controls_height_, 0, 0, 0, false, false});
    UpdateDrawProperties(host_impl_->pending_tree());

    // Fully scroll the viewport.
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(75, 75), gfx::Vector2dF(0, 2000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 2000),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();
  }

  LayerImpl* outer_scroll = OuterViewportScrollLayer();

  ASSERT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  ASSERT_EQ(1000, MaxScrollOffset(outer_scroll).y());
  ASSERT_EQ(1000, CurrentScrollOffset(outer_scroll).y());

  // Kick off an animation to show the browser controls.
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, true);
  base::TimeTicks start_time = base::TimeTicks::Now();
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // The first animation frame will not produce any delta, it will establish
  // the animation.
  {
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);
    float delta =
        host_impl_->active_tree()->top_controls_shown_ratio()->Delta();
    ASSERT_EQ(delta, 0);
  }

  // Pump an animation frame to put some delta in the browser controls.
  {
    begin_frame_args.frame_time =
        start_time + base::TimeDelta::FromMilliseconds(50);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }

  // Pull the browser controls delta and get it back to the pending tree so that
  // when we go to activate the pending tree we cause a change to browser
  // controls.
  {
    float delta =
        host_impl_->active_tree()->top_controls_shown_ratio()->Delta();
    ASSERT_GT(delta, 0);
    ASSERT_LT(delta, 1);
    host_impl_->active_tree()
        ->top_controls_shown_ratio()
        ->PullDeltaForMainThread();
    host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(
        delta);
  }

  // 200 is the kShowHideMaxDurationMs value from browser_controls_manager.cc so
  // the browser controls should be fully animated in this frame.
  {
    begin_frame_args.frame_time =
        start_time + base::TimeDelta::FromMilliseconds(200);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    ASSERT_EQ(50, host_impl_->browser_controls_manager()->ContentTopOffset());
    ASSERT_EQ(1050, MaxScrollOffset(outer_scroll).y());
    // NEAR because clip layer bounds are truncated in MaxScrollOffset so we
    // lose some precision in the intermediate animation steps.
    ASSERT_NEAR(1050, CurrentScrollOffset(outer_scroll).y(), 1);
  }

  // Activate the pending tree which should have the same scroll value as the
  // active tree.
  {
    host_impl_->pending_tree()
        ->property_trees()
        ->scroll_tree.SetScrollOffsetDeltaForTesting(outer_scroll->element_id(),
                                                     gfx::Vector2dF(0, 1050));
    host_impl_->ActivateSyncTree();

    // Make sure we don't accidentally clamp the outer offset based on a bounds
    // delta that hasn't yet been updated.
    EXPECT_NEAR(1050, CurrentScrollOffset(outer_scroll).y(), 1);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollNonCompositedRoot) {
  // Test the configuration where a non-composited root layer is embedded in a
  // scrollable outer layer.
  gfx::Size surface_size(10, 10);
  gfx::Size contents_size(20, 20);

  SetupViewportLayersNoScrolls(surface_size);

  LayerImpl* scroll_container_layer = AddContentLayer();
  CreateEffectNode(scroll_container_layer).render_surface_reason =
      RenderSurfaceReason::kTest;

  LayerImpl* scroll_layer =
      AddScrollableLayer(scroll_container_layer, surface_size, contents_size);

  LayerImpl* content_layer = AddLayer();
  content_layer->SetDrawsContent(true);
  content_layer->SetBounds(contents_size);
  CopyProperties(scroll_layer, content_layer);

  DrawFrame();

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(5, 5), gfx::Vector2dF(0, 10),
                                   ui::ScrollInputType::kWheel)
                            .get(),
                        ui::ScrollInputType::kWheel)
          .thread);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollChildCallsCommitAndRedraw) {
  gfx::Size surface_size(10, 10);
  gfx::Size contents_size(20, 20);

  SetupViewportLayersNoScrolls(surface_size);

  LayerImpl* content_root = AddContentLayer();
  CreateEffectNode(content_root).render_surface_reason =
      RenderSurfaceReason::kTest;
  AddScrollableLayer(content_root, surface_size, contents_size);

  DrawFrame();

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(5, 5), gfx::Vector2dF(0, 10),
                                   ui::ScrollInputType::kWheel)
                            .get(),
                        ui::ScrollInputType::kWheel)
          .thread);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kWheel)
                               .get());
  host_impl_->ScrollEnd();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollMissesChild) {
  gfx::Size viewport_size(5, 5);
  gfx::Size surface_size(10, 10);
  LayerImpl* root = SetupDefaultRootLayer(surface_size);
  AddScrollableLayer(root, viewport_size, surface_size);
  DrawFrame();

  // Scroll event is ignored because the input coordinate is outside the layer
  // boundaries.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(15, 5), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);

  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollMissesBackfacingChild) {
  gfx::Size viewport_size(5, 5);
  gfx::Size surface_size(10, 10);
  LayerImpl* root = SetupDefaultRootLayer(viewport_size);
  LayerImpl* child = AddScrollableLayer(root, viewport_size, surface_size);

  gfx::Transform matrix;
  matrix.RotateAboutXAxis(180.0);

  GetTransformNode(child)->local = matrix;
  CreateEffectNode(child).double_sided = false;
  DrawFrame();

  // Scroll event is ignored because the scrollable layer is not facing the
  // viewer and there is nothing scrollable behind it.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(5, 5), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);

  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollBlockedByContentLayer) {
  gfx::Size scroll_container_size(5, 5);
  gfx::Size surface_size(10, 10);
  LayerImpl* root = SetupDefaultRootLayer(surface_size);

  // Note: we can use the same clip layer for both since both calls to
  // AddScrollableLayer() use the same surface size.
  LayerImpl* scroll_layer =
      AddScrollableLayer(root, scroll_container_size, surface_size);
  LayerImpl* content_layer =
      AddScrollableLayer(scroll_layer, scroll_container_size, surface_size);
  GetScrollNode(content_layer)->main_thread_scrolling_reasons =
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  DrawFrame();

  // Scrolling fails because the content layer is asking to be scrolled on the
  // main thread.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(5, 5), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollRootAndChangePageScaleOnMainThread) {
  gfx::Size inner_viewport_size(20, 20);
  gfx::Size outer_viewport_size(40, 40);
  gfx::Size content_size(80, 80);
  SetupViewportLayers(host_impl_->active_tree(), inner_viewport_size,
                      outer_viewport_size, content_size);
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);
  DrawFrame();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::ScrollOffset expected_scroll_delta(scroll_delta);
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  gfx::ScrollOffset expected_max_scroll = MaxScrollOffset(outer_scroll);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();

  // Set new page scale from main thread.
  float page_scale = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(page_scale, 1, 2);

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), inner_scroll->element_id(),
                                 expected_scroll_delta));

  // The scroll range should also have been updated.
  EXPECT_EQ(expected_max_scroll, MaxScrollOffset(outer_scroll));

  // The page scale delta remains constant because the impl thread did not
  // scale.
  EXPECT_EQ(1, host_impl_->active_tree()->page_scale_delta());
}

TEST_F(LayerTreeHostImplTest, ScrollRootAndChangePageScaleOnImplThread) {
  gfx::Size inner_viewport_size(20, 20);
  gfx::Size outer_viewport_size(40, 40);
  gfx::Size content_size(80, 80);
  SetupViewportLayers(host_impl_->active_tree(), inner_viewport_size,
                      outer_viewport_size, content_size);
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);
  DrawFrame();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::ScrollOffset expected_scroll_delta(scroll_delta);
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  gfx::ScrollOffset expected_max_scroll = MaxScrollOffset(outer_scroll);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();

  // Set new page scale on impl thread by pinching.
  float page_scale = 2;
  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(page_scale, gfx::Point());
  host_impl_->PinchGestureEnd(gfx::Point(), true);
  host_impl_->ScrollEnd();

  DrawOneFrame();

  // The scroll delta is not scaled because the main thread did not scale.
  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), inner_scroll->element_id(),
                                 expected_scroll_delta));

  // The scroll range should also have been updated.
  EXPECT_EQ(expected_max_scroll, MaxScrollOffset(outer_scroll));

  // The page scale delta should match the new scale on the impl side.
  EXPECT_EQ(page_scale, host_impl_->active_tree()->current_page_scale_factor());
}

TEST_F(LayerTreeHostImplTest, PageScaleDeltaAppliedToRootScrollLayerOnly) {
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);
  gfx::Size viewport_size(5, 5);
  gfx::Size surface_size(10, 10);
  float default_page_scale = 1;
  gfx::Transform default_page_scale_matrix;
  default_page_scale_matrix.Scale(default_page_scale, default_page_scale);

  float new_page_scale = 2;
  gfx::Transform new_page_scale_matrix;
  new_page_scale_matrix.Scale(new_page_scale, new_page_scale);

  SetupViewportLayersInnerScrolls(viewport_size, surface_size);
  LayerImpl* root = root_layer();
  auto* inner_scroll = InnerViewportScrollLayer();
  auto* outer_scroll = OuterViewportScrollLayer();

  // Create a normal scrollable root layer and another scrollable child layer.
  LayerImpl* scrollable_child_clip = AddLayer();
  CopyProperties(inner_scroll, scrollable_child_clip);
  AddScrollableLayer(scrollable_child_clip, viewport_size, surface_size);
  UpdateDrawProperties(host_impl_->active_tree());

  // Set new page scale on impl thread by pinching.
  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(new_page_scale, gfx::Point());
  host_impl_->PinchGestureEnd(gfx::Point(), true);
  host_impl_->ScrollEnd();
  DrawOneFrame();

  // Make sure all the layers are drawn with the page scale delta applied, i.e.,
  // the page scale delta on the root layer is applied hierarchically.
  DrawFrame();

  EXPECT_EQ(1, root->DrawTransform().matrix().getDouble(0, 0));
  EXPECT_EQ(1, root->DrawTransform().matrix().getDouble(1, 1));
  EXPECT_EQ(new_page_scale,
            inner_scroll->DrawTransform().matrix().getDouble(0, 0));
  EXPECT_EQ(new_page_scale,
            inner_scroll->DrawTransform().matrix().getDouble(1, 1));
  EXPECT_EQ(new_page_scale,
            outer_scroll->DrawTransform().matrix().getDouble(0, 0));
  EXPECT_EQ(new_page_scale,
            outer_scroll->DrawTransform().matrix().getDouble(1, 1));
}

TEST_F(LayerTreeHostImplTest, ScrollChildAndChangePageScaleOnMainThread) {
  SetupViewportLayers(host_impl_->active_tree(), gfx::Size(15, 15),
                      gfx::Size(30, 30), gfx::Size(50, 50));
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  DrawFrame();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::ScrollOffset expected_scroll_delta(scroll_delta);
  gfx::ScrollOffset expected_max_scroll(MaxScrollOffset(outer_scroll));
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();

  float page_scale = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(page_scale, 1,
                                                         page_scale);
  DrawOneFrame();

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), inner_scroll->element_id(),
                                 expected_scroll_delta));

  // The scroll range should not have changed.
  EXPECT_EQ(MaxScrollOffset(outer_scroll), expected_max_scroll);

  // The page scale delta remains constant because the impl thread did not
  // scale.
  EXPECT_EQ(1, host_impl_->active_tree()->page_scale_delta());
}

TEST_F(LayerTreeHostImplTest, ScrollChildBeyondLimit) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // parent layer isn't scrolled.
  gfx::Size surface_size(10, 10);
  gfx::Size content_size(20, 20);

  SetupViewportLayersNoScrolls(surface_size);
  LayerImpl* top = AddContentLayer();
  CreateEffectNode(top).render_surface_reason = RenderSurfaceReason::kTest;
  LayerImpl* child_layer = AddScrollableLayer(top, surface_size, content_size);
  LayerImpl* grand_child_layer =
      AddScrollableLayer(child_layer, surface_size, content_size);

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          grand_child_layer->element_id(), gfx::ScrollOffset(0, 5));
  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                                     gfx::ScrollOffset(3, 0));

  DrawFrame();
  {
    gfx::Vector2d scroll_delta(-8, -7);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                           ui::ScrollInputType::kWheel)
                                    .get(),
                                ui::ScrollInputType::kWheel)
                  .thread);
    host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel)
            .get());
    host_impl_->ScrollEnd();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // The grand child should have scrolled up to its limit.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   grand_child_layer->element_id(),
                                   gfx::ScrollOffset(0, -5)));

    // The child should not have scrolled.
    ExpectNone(*scroll_info.get(), child_layer->element_id());
  }
}

TEST_F(LayerTreeHostImplTimelinesTest, ScrollAnimatedLatchToChild) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // parent layer isn't scrolled.
  gfx::Size surface_size(100, 100);
  gfx::Size content_size(150, 150);

  SetupViewportLayersNoScrolls(surface_size);
  LayerImpl* top = AddContentLayer();
  CreateEffectNode(top).render_surface_reason = RenderSurfaceReason::kTest;
  LayerImpl* child_layer = AddScrollableLayer(top, surface_size, content_size);
  LayerImpl* grand_child_layer =
      AddScrollableLayer(child_layer, surface_size, content_size);

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          grand_child_layer->element_id(), gfx::ScrollOffset(0, 30));
  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                                     gfx::ScrollOffset(0, 50));

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(10);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, -100),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, -100)).get());

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  // Should have started scrolling.
  EXPECT_NE(gfx::ScrollOffset(0, 30), CurrentScrollOffset(grand_child_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_EQ(gfx::ScrollOffset(0, 0), CurrentScrollOffset(grand_child_layer));
  EXPECT_EQ(gfx::ScrollOffset(0, 50), CurrentScrollOffset(child_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Second ScrollAnimated should remain latched to the grand_child_layer.
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, -100)).get());

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(450);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_EQ(gfx::ScrollOffset(0, 0), CurrentScrollOffset(grand_child_layer));
  EXPECT_EQ(gfx::ScrollOffset(0, 50), CurrentScrollOffset(child_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutBubbling) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // the scroll doesn't bubble up to the parent layer.
  gfx::Size surface_size(20, 20);
  gfx::Size viewport_size(10, 10);
  SetupViewportLayersNoScrolls(surface_size);
  LayerImpl* child_layer = AddScrollableLayer(InnerViewportScrollLayer(),
                                              viewport_size, surface_size);
  LayerImpl* grand_child_layer =
      AddScrollableLayer(child_layer, viewport_size, surface_size);
  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          grand_child_layer->element_id(), gfx::ScrollOffset(0, 2));
  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                                     gfx::ScrollOffset(0, 3));

  DrawFrame();
  {
    gfx::Vector2d scroll_delta(0, -10);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // The grand child should have scrolled up to its limit.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   grand_child_layer->element_id(),
                                   gfx::ScrollOffset(0, -2)));

    // The child should not have scrolled.
    ExpectNone(*scroll_info.get(), child_layer->element_id());

    // The next time we scroll we should only scroll the parent.
    scroll_delta = gfx::Vector2d(0, -3);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              child_layer->scroll_tree_index());
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              child_layer->scroll_tree_index());
    host_impl_->ScrollEnd();

    scroll_info = host_impl_->ProcessScrollDeltas();

    // The child should have scrolled up to its limit.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   child_layer->element_id(),
                                   gfx::ScrollOffset(0, -3)));

    // The grand child should not have scrolled.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   grand_child_layer->element_id(),
                                   gfx::ScrollOffset(0, -2)));

    // After scrolling the parent, another scroll on the opposite direction
    // should still scroll the child.
    scroll_delta = gfx::Vector2d(0, 7);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    host_impl_->ScrollEnd();

    scroll_info = host_impl_->ProcessScrollDeltas();

    // The grand child should have scrolled.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   grand_child_layer->element_id(),
                                   gfx::ScrollOffset(0, 5)));

    // The child should not have scrolled.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   child_layer->element_id(),
                                   gfx::ScrollOffset(0, -3)));

    // Scrolling should be adjusted from viewport space.
    host_impl_->active_tree()->PushPageScaleFromMainThread(2, 2, 2);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(2);

    scroll_delta = gfx::Vector2d(0, -2);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(1, 1), scroll_delta,
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_EQ(grand_child_layer->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    scroll_info = host_impl_->ProcessScrollDeltas();

    // Should have scrolled by half the amount in layer space (5 - 2/2)
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   grand_child_layer->element_id(),
                                   gfx::ScrollOffset(0, 4)));
  }
}

// Ensure that layers who's scroll parent is the InnerViewportScrollNode are
// still able to scroll on the compositor.
TEST_F(LayerTreeHostImplTest, ChildrenOfInnerScrollNodeCanScrollOnThread) {
  gfx::Size viewport_size(10, 10);
  gfx::Size content_size(20, 20);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  // Simulate adding a "fixed" layer to the tree.
  {
    LayerImpl* fixed_layer = AddLayer();
    fixed_layer->SetBounds(viewport_size);
    fixed_layer->SetDrawsContent(true);
    CopyProperties(InnerViewportScrollLayer(), fixed_layer);
  }

  host_impl_->active_tree()->DidBecomeActive();
  DrawFrame();
  {
    gfx::ScrollOffset scroll_delta(0, 4);
    // Scrolling should be able to happen on the compositor thread here.
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(BeginState(gfx::Point(5, 5),
                                     gfx::ScrollOffsetToVector2dF(scroll_delta),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel)
            .thread);
    host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), gfx::ScrollOffsetToVector2dF(scroll_delta),
                    ui::ScrollInputType::kWheel)
            .get());
    host_impl_->ScrollEnd();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // The outer viewport should have scrolled.
    ASSERT_EQ(scroll_info->scrolls.size(), 1u);
    EXPECT_TRUE(ScrollInfoContains(
        *scroll_info.get(), host_impl_->OuterViewportScrollNode()->element_id,
        scroll_delta));
  }
}

TEST_F(LayerTreeHostImplTest, ScrollEventBubbling) {
  // When we try to scroll a non-scrollable child layer, the scroll delta
  // should be applied to one of its ancestors if possible.
  gfx::Size viewport_size(10, 10);
  gfx::Size content_size(20, 20);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  // Add a scroller whose scroll bounds and scroll container bounds are equal.
  // Since the max_scroll_offset is 0, scrolls will bubble.
  LayerImpl* scroll_child_clip = AddContentLayer();
  AddScrollableLayer(scroll_child_clip, gfx::Size(10, 10), gfx::Size(10, 10));

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();
  DrawFrame();
  {
    gfx::ScrollOffset scroll_delta(0, 4);
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(BeginState(gfx::Point(5, 5),
                                     gfx::ScrollOffsetToVector2dF(scroll_delta),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel)
            .thread);
    host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), gfx::ScrollOffsetToVector2dF(scroll_delta),
                    ui::ScrollInputType::kWheel)
            .get());
    host_impl_->ScrollEnd();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // Only the root scroll should have scrolled.
    ASSERT_EQ(scroll_info->scrolls.size(), 1u);
    EXPECT_TRUE(ScrollInfoContains(
        *scroll_info.get(), host_impl_->OuterViewportScrollNode()->element_id,
        scroll_delta));
  }
}

TEST_F(LayerTreeHostImplTest, ScrollBeforeRedraw) {
  gfx::Size surface_size(10, 10);
  SetupViewportLayersNoScrolls(surface_size);
  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  // Draw one frame and then immediately rebuild the layer tree to mimic a tree
  // synchronization.
  DrawFrame();

  ClearLayersAndPropertyTrees(host_impl_->active_tree());
  SetupViewportLayersNoScrolls(surface_size);

  // Scrolling should still work even though we did not draw yet.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(5, 5), gfx::Vector2dF(0, 10),
                                   ui::ScrollInputType::kWheel)
                            .get(),
                        ui::ScrollInputType::kWheel)
          .thread);
}

TEST_F(LayerTreeHostImplTest, ScrollAxisAlignedRotatedLayer) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  scroll_layer->SetDrawsContent(true);

  // Rotate the root layer 90 degrees counter-clockwise about its center.
  gfx::Transform rotate_transform;
  rotate_transform.Rotate(-90.0);
  // Set external transform.
  host_impl_->OnDraw(rotate_transform, gfx::Rect(0, 0, 50, 50), false, false);
  DrawFrame();

  // Scroll to the right in screen coordinates with a gesture.
  gfx::Vector2d gesture_scroll_delta(10, 0);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gesture_scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gesture_scroll_delta,
                                       ui::ScrollInputType::kTouchscreen)
                               .get());
  host_impl_->ScrollEnd();

  // The layer should have scrolled down in its local coordinates.
  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(
      ScrollInfoContains(*scroll_info.get(), scroll_layer->element_id(),
                         gfx::ScrollOffset(0, gesture_scroll_delta.x())));

  // Reset and scroll down with the wheel.
  SetScrollOffsetDelta(scroll_layer, gfx::Vector2dF());
  gfx::ScrollOffset wheel_scroll_delta(0, 10);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(
                    BeginState(gfx::Point(),
                               gfx::ScrollOffsetToVector2dF(wheel_scroll_delta),
                               ui::ScrollInputType::kWheel)
                        .get(),
                    ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(),
                  gfx::ScrollOffsetToVector2dF(wheel_scroll_delta),
                  ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();

  // The layer should have scrolled down in its local coordinates.
  scroll_info = host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), scroll_layer->element_id(),
                                 wheel_scroll_delta));
}

TEST_F(LayerTreeHostImplTest, ScrollNonAxisAlignedRotatedLayer) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  float child_layer_angle = -20;

  // Create a child layer that is rotated to a non-axis-aligned angle.
  // Only allow vertical scrolling.
  gfx::Size content_size = scroll_layer->bounds();
  gfx::Size scroll_container_bounds(content_size.width(),
                                    content_size.height() / 2);
  LayerImpl* clip_layer = AddLayer();
  clip_layer->SetBounds(scroll_container_bounds);
  CopyProperties(scroll_layer, clip_layer);
  gfx::Transform rotate_transform;
  rotate_transform.Translate(-50.0, -50.0);
  rotate_transform.Rotate(child_layer_angle);
  rotate_transform.Translate(50.0, 50.0);
  auto& clip_layer_transform_node = CreateTransformNode(clip_layer);
  // The rotation depends on the layer's transform origin, and the child layer
  // is a different size than the clip, so make sure the clip layer's origin
  // lines up over the child.
  clip_layer_transform_node.origin = gfx::Point3F(
      clip_layer->bounds().width() * 0.5f, clip_layer->bounds().height(), 0);
  clip_layer_transform_node.local = rotate_transform;

  LayerImpl* child =
      AddScrollableLayer(clip_layer, scroll_container_bounds, content_size);

  ElementId child_scroll_id = child->element_id();

  DrawFrame();
  {
    // Scroll down in screen coordinates with a gesture.
    gfx::Vector2d gesture_scroll_delta(0, 10);
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(BeginState(gfx::Point(1, 1), gesture_scroll_delta,
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen)
            .thread);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gesture_scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    // The child layer should have scrolled down in its local coordinates an
    // amount proportional to the angle between it and the input scroll delta.
    gfx::ScrollOffset expected_scroll_delta(
        0, std::floor(gesture_scroll_delta.y() *
                      std::cos(gfx::DegToRad(child_layer_angle))));
    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), child_scroll_id,
                                   expected_scroll_delta));

    // The root scroll layer should not have scrolled, because the input delta
    // was close to the layer's axis of movement.
    EXPECT_EQ(scroll_info->scrolls.size(), 1u);
  }
  {
    // Now reset and scroll the same amount horizontally.
    SetScrollOffsetDelta(child, gfx::Vector2dF());
    gfx::Vector2d gesture_scroll_delta(10, 0);
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(BeginState(gfx::Point(1, 1), gesture_scroll_delta,
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen)
            .thread);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gesture_scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    // The child layer should have scrolled down in its local coordinates an
    // amount proportional to the angle between it and the input scroll delta.
    gfx::ScrollOffset expected_scroll_delta(
        0, std::floor(-gesture_scroll_delta.x() *
                      std::sin(gfx::DegToRad(child_layer_angle))));
    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), child_scroll_id,
                                   expected_scroll_delta));

    // The root scroll layer shouldn't have scrolled.
    ExpectNone(*scroll_info.get(), scroll_layer->element_id());
  }
}

TEST_F(LayerTreeHostImplTest, ScrollPerspectiveTransformedLayer) {
  // When scrolling an element with perspective, the distance scrolled
  // depends on the point at which the scroll begins.
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();

  // Create a child layer that is rotated on its x axis, with perspective.
  LayerImpl* clip_layer = AddLayer();
  clip_layer->SetBounds(gfx::Size(50, 50));
  CopyProperties(scroll_layer, clip_layer);
  gfx::Transform perspective_transform;
  perspective_transform.Translate(-50.0, -50.0);
  perspective_transform.ApplyPerspectiveDepth(20);
  perspective_transform.RotateAboutXAxis(45);
  perspective_transform.Translate(50.0, 50.0);
  auto& clip_layer_transform_node = CreateTransformNode(clip_layer);
  // The transform depends on the layer's transform origin, and the child layer
  // is a different size than the clip, so make sure the clip layer's origin
  // lines up over the child.
  clip_layer_transform_node.origin = gfx::Point3F(
      clip_layer->bounds().width(), clip_layer->bounds().height(), 0);
  clip_layer_transform_node.local = perspective_transform;

  LayerImpl* child = AddScrollableLayer(clip_layer, clip_layer->bounds(),
                                        scroll_layer->bounds());

  UpdateDrawProperties(host_impl_->active_tree());

  std::unique_ptr<ScrollAndScaleSet> scroll_info;

  gfx::ScrollOffset gesture_scroll_deltas[4];
  gesture_scroll_deltas[0] = gfx::ScrollOffset(4, 10);
  gesture_scroll_deltas[1] = gfx::ScrollOffset(4, 10);
  gesture_scroll_deltas[2] = gfx::ScrollOffset(10, 0);
  gesture_scroll_deltas[3] = gfx::ScrollOffset(10, 0);

  gfx::ScrollOffset expected_scroll_deltas[4];
  // Perspective affects the vertical delta by a different
  // amount depending on the vertical position of the |viewport_point|.
  expected_scroll_deltas[0] = gfx::ScrollOffset(2, 9);
  expected_scroll_deltas[1] = gfx::ScrollOffset(1, 4);
  // Deltas which start with the same vertical position of the
  // |viewport_point| are subject to identical perspective effects.
  expected_scroll_deltas[2] = gfx::ScrollOffset(5, 0);
  expected_scroll_deltas[3] = gfx::ScrollOffset(5, 0);

  gfx::Point viewport_point(1, 1);

  // Scroll in screen coordinates with a gesture. Each scroll starts
  // where the previous scroll ended, but the scroll position is reset
  // for each scroll.
  for (int i = 0; i < 4; ++i) {
    SetScrollOffsetDelta(child, gfx::Vector2dF());
    DrawFrame();
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(viewport_point,
                                           gfx::ScrollOffsetToVector2dF(
                                               gesture_scroll_deltas[i]),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    host_impl_->ScrollUpdate(
        UpdateState(viewport_point,
                    gfx::ScrollOffsetToVector2dF(gesture_scroll_deltas[i]),
                    ui::ScrollInputType::kTouchscreen)
            .get());
    viewport_point +=
        gfx::ScrollOffsetToFlooredVector2d(gesture_scroll_deltas[i]);
    host_impl_->ScrollEnd();

    scroll_info = host_impl_->ProcessScrollDeltas();
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), child->element_id(),
                                   expected_scroll_deltas[i]));

    // The root scroll layer should not have scrolled, because the input delta
    // was close to the layer's axis of movement.
    EXPECT_EQ(scroll_info->scrolls.size(), 1u);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollScaledLayer) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();

  // Scale the layer to twice its normal size.
  int scale = 2;
  gfx::Transform scale_transform;
  scale_transform.Scale(scale, scale);
  // Set external transform above root.
  host_impl_->OnDraw(scale_transform, gfx::Rect(0, 0, 50, 50), false, false);
  DrawFrame();

  // Scroll down in screen coordinates with a gesture.
  gfx::Vector2d scroll_delta(0, 10);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen)
          .get());
  host_impl_->ScrollEnd();

  // The layer should have scrolled down in its local coordinates, but half the
  // amount.
  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(
      ScrollInfoContains(*scroll_info.get(), scroll_layer->element_id(),
                         gfx::ScrollOffset(0, scroll_delta.y() / scale)));

  // Reset and scroll down with the wheel.
  SetScrollOffsetDelta(scroll_layer, gfx::Vector2dF());
  gfx::ScrollOffset wheel_scroll_delta(0, 10);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(
                    BeginState(gfx::Point(),
                               gfx::ScrollOffsetToVector2dF(wheel_scroll_delta),
                               ui::ScrollInputType::kWheel)
                        .get(),
                    ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(),
                  gfx::ScrollOffsetToVector2dF(wheel_scroll_delta),
                  ui::ScrollInputType::kWheel)
          .get());
  host_impl_->ScrollEnd();

  // It should apply the scale factor to the scroll delta for the wheel event.
  scroll_info = host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), scroll_layer->element_id(),
                                 wheel_scroll_delta));
}

TEST_F(LayerTreeHostImplTest, ScrollViewportRounding) {
  int width = 332;
  int height = 20;
  int scale = 3;
  gfx::Size container_bounds = gfx::Size(width * scale - 1, height * scale);
  SetupViewportLayersInnerScrolls(container_bounds, gfx::Size(width, height));
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->active_tree()->SetDeviceScaleFactor(scale);
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);

  LayerImpl* inner_viewport_scroll_layer = InnerViewportScrollLayer();
  EXPECT_EQ(gfx::ScrollOffset(0, 0),
            MaxScrollOffset(inner_viewport_scroll_layer));
}

TEST_F(LayerTreeHostImplTest, RootLayerScrollOffsetDelegation) {
  TestInputHandlerClient scroll_watcher;
  SetupViewportLayersInnerScrolls(gfx::Size(10, 20), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->BindToClient(&scroll_watcher);

  gfx::Vector2dF initial_scroll_delta(10, 10);
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset());
  SetScrollOffsetDelta(scroll_layer, initial_scroll_delta);

  EXPECT_EQ(gfx::ScrollOffset(), scroll_watcher.last_set_scroll_offset());

  // Requesting an update results in the current scroll offset being set.
  host_impl_->RequestUpdateForSynchronousInputHandler();
  EXPECT_EQ(gfx::ScrollOffset(initial_scroll_delta),
            scroll_watcher.last_set_scroll_offset());

  // Setting the delegate results in the scrollable_size, max_scroll_offset,
  // page_scale_factor and {min|max}_page_scale_factor being set.
  EXPECT_EQ(gfx::SizeF(100, 100), scroll_watcher.scrollable_size());
  EXPECT_EQ(gfx::ScrollOffset(90, 80), scroll_watcher.max_scroll_offset());
  EXPECT_EQ(1, scroll_watcher.page_scale_factor());
  EXPECT_EQ(1, scroll_watcher.min_page_scale_factor());
  EXPECT_EQ(1, scroll_watcher.max_page_scale_factor());

  // Put a page scale on the tree.
  host_impl_->active_tree()->PushPageScaleFromMainThread(2, 0.5f, 4);
  EXPECT_EQ(1, scroll_watcher.page_scale_factor());
  EXPECT_EQ(1, scroll_watcher.min_page_scale_factor());
  EXPECT_EQ(1, scroll_watcher.max_page_scale_factor());
  // Activation will update the delegate.
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(2, scroll_watcher.page_scale_factor());
  EXPECT_EQ(.5f, scroll_watcher.min_page_scale_factor());
  EXPECT_EQ(4, scroll_watcher.max_page_scale_factor());

  // Animating page scale can change the root offset, so it should update the
  // delegate. Also resets the page scale to 1 for the rest of the test.
  host_impl_->LayerTreeHostImpl::StartPageScaleAnimation(
      gfx::Vector2d(0, 0), false, 1, base::TimeDelta());
  host_impl_->Animate();
  EXPECT_EQ(1, scroll_watcher.page_scale_factor());
  EXPECT_EQ(.5f, scroll_watcher.min_page_scale_factor());
  EXPECT_EQ(4, scroll_watcher.max_page_scale_factor());

  // The pinch gesture doesn't put the delegate into a state where the scroll
  // offset is outside of the scroll range.  (this is verified by DCHECKs in the
  // delegate).
  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, gfx::Point());
  host_impl_->PinchGestureUpdate(.5f, gfx::Point());
  host_impl_->PinchGestureEnd(gfx::Point(), true);
  host_impl_->ScrollEnd();

  // Scrolling should be relative to the offset as given by the delegate.
  gfx::Vector2dF scroll_delta(0, 10);
  gfx::ScrollOffset current_offset(7, 8);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->SetSynchronousInputHandlerRootScrollOffset(current_offset);

  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen)
          .get());
  EXPECT_EQ(ScrollOffsetWithDelta(current_offset, scroll_delta),
            scroll_watcher.last_set_scroll_offset());

  current_offset = gfx::ScrollOffset(42, 41);
  host_impl_->SetSynchronousInputHandlerRootScrollOffset(current_offset);
  host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen)
          .get());
  EXPECT_EQ(current_offset + gfx::ScrollOffset(scroll_delta),
            scroll_watcher.last_set_scroll_offset());
  host_impl_->ScrollEnd();
  host_impl_->SetSynchronousInputHandlerRootScrollOffset(gfx::ScrollOffset());

  // Forces a full tree synchronization and ensures that the scroll delegate
  // sees the correct size of the new tree.
  gfx::Size new_viewport_size(21, 12);
  gfx::Size new_content_size(42, 24);
  CreatePendingTree();
  host_impl_->pending_tree()->PushPageScaleFromMainThread(1, 1, 1);
  SetupViewportLayers(host_impl_->pending_tree(), new_viewport_size,
                      new_content_size, new_content_size);
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(gfx::SizeF(new_content_size), scroll_watcher.scrollable_size());

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

void CheckLayerScrollDelta(LayerImpl* layer, gfx::Vector2dF scroll_delta) {
  const gfx::Transform target_space_transform =
      layer->draw_properties().target_space_transform;
  EXPECT_TRUE(target_space_transform.IsScaleOrTranslation());
  gfx::Point translated_point;
  target_space_transform.TransformPoint(&translated_point);
  gfx::Point expected_point = gfx::Point() - ToRoundedVector2d(scroll_delta);
  EXPECT_EQ(expected_point.ToString(), translated_point.ToString());
}

TEST_F(LayerTreeHostImplTest,
       ExternalRootLayerScrollOffsetDelegationReflectedInNextDraw) {
  SetupViewportLayersInnerScrolls(gfx::Size(10, 20), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  scroll_layer->SetDrawsContent(true);

  // Draw first frame to clear any pending draws and check scroll.
  DrawFrame();
  CheckLayerScrollDelta(scroll_layer, gfx::Vector2dF(0, 0));
  EXPECT_FALSE(host_impl_->active_tree()->needs_update_draw_properties());

  // Set external scroll delta on delegate and notify LayerTreeHost.
  gfx::ScrollOffset scroll_offset(10, 10);
  host_impl_->SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
  CheckLayerScrollDelta(scroll_layer, gfx::Vector2dF(0, 0));
  EXPECT_TRUE(host_impl_->active_tree()->needs_update_draw_properties());

  // Check scroll delta reflected in layer.
  TestFrameData frame;
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  host_impl_->WillBeginImplFrame(args);
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);
  EXPECT_FALSE(frame.has_no_damage);
  CheckLayerScrollDelta(scroll_layer,
                        gfx::ScrollOffsetToVector2dF(scroll_offset));
}

// Ensure the viewport correctly handles the user_scrollable bits. That is, if
// the outer viewport disables user scrolling, we should still be able to
// scroll the inner viewport.
TEST_F(LayerTreeHostImplTest, ViewportUserScrollable) {
  gfx::Size viewport_size(100, 100);
  gfx::Size content_size(200, 200);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  auto* outer_scroll = OuterViewportScrollLayer();
  auto* inner_scroll = InnerViewportScrollLayer();

  ScrollTree& scroll_tree =
      host_impl_->active_tree()->property_trees()->scroll_tree;
  ElementId inner_element_id = inner_scroll->element_id();
  ElementId outer_element_id = outer_scroll->element_id();

  DrawFrame();

  // "Zoom in" so that the inner viewport is scrollable.
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, page_scale_factor, page_scale_factor);

  // Disable scrolling the outer viewport horizontally. The inner viewport
  // should still be allowed to scroll.
  GetScrollNode(outer_scroll)->user_scrollable_vertical = true;
  GetScrollNode(outer_scroll)->user_scrollable_horizontal = false;

  gfx::Vector2dF scroll_delta(30 * page_scale_factor, 0);
  {
    auto begin_state = BeginState(gfx::Point(), scroll_delta,
                                  ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(begin_state.get(), ui::ScrollInputType::kTouchscreen)
            .thread);

    // Try scrolling right, the inner viewport should be allowed to scroll.
    auto update_state = UpdateState(gfx::Point(), scroll_delta,
                                    ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(update_state.get());

    EXPECT_VECTOR_EQ(gfx::ScrollOffset(30, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));

    // Continue scrolling. The inner viewport should scroll until its extent,
    // however, the outer viewport should not accept any scroll.
    update_state = UpdateState(gfx::Point(), scroll_delta,
                               ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(update_state.get());
    update_state = UpdateState(gfx::Point(), scroll_delta,
                               ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(update_state.get());
    update_state = UpdateState(gfx::Point(), scroll_delta,
                               ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(update_state.get());

    EXPECT_VECTOR_EQ(gfx::ScrollOffset(50, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));

    host_impl_->ScrollEnd();
  }

  // Reset. Try the same test above but using animated scrolls.
  SetScrollOffset(outer_scroll, gfx::ScrollOffset(0, 0));
  SetScrollOffset(inner_scroll, gfx::ScrollOffset(0, 0));

  {
    auto begin_state =
        BeginState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel);
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_->ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel)
            .thread);

    // Try scrolling right, the inner viewport should be allowed to scroll.
    auto update_state = AnimatedUpdateState(gfx::Point(), scroll_delta);
    host_impl_->ScrollUpdate(update_state.get());

    base::TimeTicks cur_time =
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
    viz::BeginFrameArgs begin_frame_args =
        viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

#define ANIMATE(time_ms)                                  \
  cur_time += base::TimeDelta::FromMilliseconds(time_ms); \
  begin_frame_args.frame_time = (cur_time);               \
  begin_frame_args.frame_id.sequence_number++;            \
  host_impl_->WillBeginImplFrame(begin_frame_args);       \
  host_impl_->Animate();                                  \
  host_impl_->UpdateAnimationState(true);                 \
  host_impl_->DidFinishImplFrame(begin_frame_args);

    // The animation is setup in the first frame so tick twice to actually
    // animate it.
    ANIMATE(0);
    ANIMATE(200);

    EXPECT_VECTOR_EQ(gfx::ScrollOffset(30, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));

    // Continue scrolling. The inner viewport should scroll until its extent,
    // however, the outer viewport should not accept any scroll.
    update_state = AnimatedUpdateState(gfx::Point(), scroll_delta);
    host_impl_->ScrollUpdate(update_state.get());
    ANIMATE(10);
    ANIMATE(200);

    EXPECT_VECTOR_EQ(gfx::ScrollOffset(50, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));

    // Continue scrolling. the outer viewport should still not scroll.
    update_state = AnimatedUpdateState(gfx::Point(), scroll_delta);
    host_impl_->ScrollUpdate(update_state.get());
    ANIMATE(10);
    ANIMATE(200);

    EXPECT_VECTOR_EQ(gfx::ScrollOffset(50, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));

    // Fully scroll the inner viewport. We'll now try to start an animation on
    // the outer viewport in the vertical direction, which is scrollable. We'll
    // then try to update the curve to scroll horizontally. Ensure that doesn't
    // allow any horizontal scroll.
    SetScrollOffset(inner_scroll, gfx::ScrollOffset(50, 50));
    update_state = AnimatedUpdateState(gfx::Point(), gfx::Vector2dF(0, 100));
    host_impl_->ScrollUpdate(update_state.get());
    ANIMATE(16);
    ANIMATE(64);

    // We don't care about the exact offset, we just want to make sure the
    // scroll is in progress but not finished.
    ASSERT_LT(0, scroll_tree.current_scroll_offset(outer_element_id).y());
    ASSERT_GT(50, scroll_tree.current_scroll_offset(outer_element_id).y());
    ASSERT_EQ(0, scroll_tree.current_scroll_offset(outer_element_id).x());
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(50, 50),
                     scroll_tree.current_scroll_offset(inner_element_id));

    // Now when we scroll we should do so by updating the ongoing animation
    // curve. Ensure this doesn't allow any horizontal scrolling.
    update_state = AnimatedUpdateState(gfx::Point(), gfx::Vector2dF(100, 100));
    host_impl_->ScrollUpdate(update_state.get());
    ANIMATE(200);

    EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 100),
                     scroll_tree.current_scroll_offset(outer_element_id));
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(50, 50),
                     scroll_tree.current_scroll_offset(inner_element_id));

#undef ANIMATE

    host_impl_->ScrollEnd();
  }
}

// Ensure that the SetSynchronousInputHandlerRootScrollOffset method used by
// the WebView API correctly respects the user_scrollable bits on both of the
// inner and outer viewport scroll nodes.
TEST_F(LayerTreeHostImplTest, SetRootScrollOffsetUserScrollable) {
  gfx::Size viewport_size(100, 100);
  gfx::Size content_size(200, 200);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  auto* outer_scroll = OuterViewportScrollLayer();
  auto* inner_scroll = InnerViewportScrollLayer();

  ScrollTree& scroll_tree =
      host_impl_->active_tree()->property_trees()->scroll_tree;
  ElementId inner_element_id = inner_scroll->element_id();
  ElementId outer_element_id = outer_scroll->element_id();

  DrawFrame();

  // Ensure that the scroll offset is interpreted as a content offset so it
  // should be unaffected by the page scale factor. See
  // https://crbug.com/973771.
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, page_scale_factor, page_scale_factor);

  // Disable scrolling the inner viewport. Only the outer should scroll.
  {
    ASSERT_FALSE(did_request_redraw_);
    GetScrollNode(inner_scroll)->user_scrollable_vertical = false;
    GetScrollNode(inner_scroll)->user_scrollable_horizontal = false;

    gfx::ScrollOffset scroll_offset(25, 30);
    host_impl_->SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_VECTOR_EQ(scroll_offset,
                     scroll_tree.current_scroll_offset(outer_element_id));
    EXPECT_TRUE(did_request_redraw_);

    // Reset
    did_request_redraw_ = false;
    GetScrollNode(inner_scroll)->user_scrollable_vertical = true;
    GetScrollNode(inner_scroll)->user_scrollable_horizontal = true;
    SetScrollOffset(outer_scroll, gfx::ScrollOffset(0, 0));
  }

  // Disable scrolling the outer viewport. The inner should scroll to its
  // extent but there should be no bubbling over to the outer viewport.
  {
    ASSERT_FALSE(did_request_redraw_);
    GetScrollNode(outer_scroll)->user_scrollable_vertical = false;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = false;

    gfx::ScrollOffset scroll_offset(120, 140);
    host_impl_->SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(50, 50),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(),
                     scroll_tree.current_scroll_offset(outer_element_id));
    EXPECT_TRUE(did_request_redraw_);

    // Reset
    did_request_redraw_ = false;
    GetScrollNode(outer_scroll)->user_scrollable_vertical = true;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = true;
    SetScrollOffset(inner_scroll, gfx::ScrollOffset(0, 0));
  }

  // Disable both viewports. No scrolling should take place, no redraw should
  // be requested.
  {
    ASSERT_FALSE(did_request_redraw_);
    GetScrollNode(inner_scroll)->user_scrollable_vertical = false;
    GetScrollNode(inner_scroll)->user_scrollable_horizontal = false;
    GetScrollNode(outer_scroll)->user_scrollable_vertical = false;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = false;

    gfx::ScrollOffset scroll_offset(60, 70);
    host_impl_->SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(),
                     scroll_tree.current_scroll_offset(outer_element_id));
    EXPECT_FALSE(did_request_redraw_);

    // Reset
    GetScrollNode(inner_scroll)->user_scrollable_vertical = true;
    GetScrollNode(inner_scroll)->user_scrollable_horizontal = true;
    GetScrollNode(outer_scroll)->user_scrollable_vertical = true;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = true;
  }

  // If the inner is at its extent but the outer cannot scroll, we shouldn't
  // request a redraw.
  {
    ASSERT_FALSE(did_request_redraw_);
    GetScrollNode(outer_scroll)->user_scrollable_vertical = false;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = false;
    SetScrollOffset(inner_scroll, gfx::ScrollOffset(50, 50));

    gfx::ScrollOffset scroll_offset(60, 70);
    host_impl_->SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(50, 50),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_VECTOR_EQ(gfx::ScrollOffset(),
                     scroll_tree.current_scroll_offset(outer_element_id));
    EXPECT_FALSE(did_request_redraw_);

    // Reset
    GetScrollNode(outer_scroll)->user_scrollable_vertical = true;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = true;
  }
}

// The SetSynchronousInputHandlerRootScrollOffset API can be called while there
// is no inner viewport set. This test passes if we don't crash.
TEST_F(LayerTreeHostImplTest, SetRootScrollOffsetNoViewportCrash) {
  auto* inner_scroll = InnerViewportScrollLayer();
  ASSERT_FALSE(inner_scroll);
  gfx::ScrollOffset scroll_offset(25, 30);
  host_impl_->SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
}

TEST_F(LayerTreeHostImplTest, OverscrollRoot) {
  InputHandlerScrollResult scroll_result;
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);
  DrawFrame();
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());

  // In-bounds scrolling does not affect overscroll.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());

  // Overscroll events are reflected immediately.
  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 50),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, 10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  // In-bounds scrolling resets accumulated overscroll for the scrolled axes.
  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -50),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, 0), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -10),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(10, 0),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 0), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-15, 0),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(-5, 0), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(-5, -10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 60),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(-5, 10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(10, -60),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  // Overscroll accumulates within the scope of ScrollBegin/ScrollEnd as long
  // as no scroll occurs.
  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -20),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -20), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -30), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -20),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -20), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -50), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  // Overscroll resets on valid scroll.
  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 0), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, 0), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -20),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  host_impl_->ScrollEnd();
}

TEST_F(LayerTreeHostImplTest, OverscrollChildWithoutBubbling) {
  // Scroll child layers beyond their maximum scroll range and make sure root
  // overscroll does not accumulate.
  InputHandlerScrollResult scroll_result;
  gfx::Size scroll_container_size(5, 5);
  gfx::Size surface_size(10, 10);
  SetupViewportLayersNoScrolls(surface_size);
  LayerImpl* root = AddScrollableLayer(OuterViewportScrollLayer(),
                                       scroll_container_size, surface_size);
  LayerImpl* child_layer =
      AddScrollableLayer(root, scroll_container_size, surface_size);
  LayerImpl* grand_child_layer =
      AddScrollableLayer(child_layer, scroll_container_size, surface_size);

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                                     gfx::ScrollOffset(0, 3));
  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          grand_child_layer->element_id(), gfx::ScrollOffset(0, 2));

  DrawFrame();
  {
    gfx::Vector2d scroll_delta(0, -10);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    scroll_result =
        host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                             ui::ScrollInputType::kTouchscreen)
                                     .get());
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd();

    // The next time we scroll we should only scroll the parent, but overscroll
    // should still not reach the root layer.
    scroll_delta = gfx::Vector2d(0, -30);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              child_layer->scroll_tree_index());
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd();

    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    scroll_result =
        host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                             ui::ScrollInputType::kTouchscreen)
                                     .get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              child_layer->scroll_tree_index());
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd();

    // After scrolling the parent, another scroll on the opposite direction
    // should scroll the child.
    scroll_delta = gfx::Vector2d(0, 70);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    scroll_result =
        host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                             ui::ScrollInputType::kTouchscreen)
                                     .get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd();
  }
}

TEST_F(LayerTreeHostImplTest, OverscrollChildEventBubbling) {
  // When we try to scroll a non-scrollable child layer, the scroll delta
  // should be applied to one of its ancestors if possible. Overscroll should
  // be reflected only when it has bubbled up to the root scrolling layer.
  InputHandlerScrollResult scroll_result;
  SetupViewportLayersInnerScrolls(gfx::Size(10, 10), gfx::Size(20, 20));
  DrawFrame();
  {
    gfx::Vector2d scroll_delta(0, 8);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                           ui::ScrollInputType::kWheel)
                                    .get(),
                                ui::ScrollInputType::kWheel)
                  .thread);
    scroll_result = host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel)
            .get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    scroll_result = host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel)
            .get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_TRUE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(0, 6), host_impl_->accumulated_root_overscroll());
    scroll_result = host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel)
            .get());
    EXPECT_FALSE(scroll_result.did_scroll);
    EXPECT_TRUE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(0, 14), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd();
  }
}

TEST_F(LayerTreeHostImplTest, OverscrollAlways) {
  InputHandlerScrollResult scroll_result;
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  SetupViewportLayersNoScrolls(gfx::Size(50, 50));
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);
  DrawFrame();
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());

  // Even though the layer can't scroll the overscroll still happens.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                           ui::ScrollInputType::kWheel)
                                   .get());
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 10), host_impl_->accumulated_root_overscroll());
}

TEST_F(LayerTreeHostImplTest, NoOverscrollWhenNotAtEdge) {
  InputHandlerScrollResult scroll_result;
  gfx::Size viewport_size(100, 100);
  gfx::Size content_size(200, 200);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  DrawFrame();
  {
    // Edge glow effect should be applicable only upon reaching Edges
    // of the content. unnecessary glow effect calls shouldn't be
    // called while scrolling up without reaching the edge of the content.
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(0, 100),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel)
            .thread);
    scroll_result = host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), gfx::Vector2dF(0, 100),
                    ui::ScrollInputType::kWheel)
            .get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF().ToString(),
              host_impl_->accumulated_root_overscroll().ToString());
    scroll_result = host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), gfx::Vector2dF(0, -2.30f),
                    ui::ScrollInputType::kWheel)
            .get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF().ToString(),
              host_impl_->accumulated_root_overscroll().ToString());
    host_impl_->ScrollEnd();
    // unusedrootDelta should be subtracted from applied delta so that
    // unwanted glow effect calls are not called.
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(0, 20),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen)
            .thread);
    scroll_result = host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), gfx::Vector2dF(0, 20),
                    ui::ScrollInputType::kTouchscreen)
            .get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_TRUE(scroll_result.did_overscroll_root);
    EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0.000000f, 17.699997f),
                        host_impl_->accumulated_root_overscroll());

    scroll_result = host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), gfx::Vector2dF(0.02f, -0.01f),
                    ui::ScrollInputType::kTouchscreen)
            .get());
    EXPECT_FALSE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0.000000f, 17.699997f),
                        host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd();
    // TestCase to check  kEpsilon, which prevents minute values to trigger
    // gloweffect without reaching edge.
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(
                      BeginState(gfx::Point(0, 0), gfx::Vector2dF(-0.12f, 0.1f),
                                 ui::ScrollInputType::kWheel)
                          .get(),
                      ui::ScrollInputType::kWheel)
                  .thread);
    scroll_result = host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), gfx::Vector2dF(-0.12f, 0.1f),
                    ui::ScrollInputType::kWheel)
            .get());
    EXPECT_FALSE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF().ToString(),
              host_impl_->accumulated_root_overscroll().ToString());
    host_impl_->ScrollEnd();
  }
}

TEST_F(LayerTreeHostImplTest, NoOverscrollOnNonViewportLayers) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);

  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* content_layer = AddContentLayer();
  // Initialization: Add a nested scrolling layer, simulating a scrolling div.
  LayerImpl* scroll_layer =
      AddScrollableLayer(content_layer, content_size, gfx::Size(400, 400));

  InputHandlerScrollResult scroll_result;
  DrawFrame();

  // Start a scroll gesture, ensure it's scrolling the subscroller.
  {
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(100, 100),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(100, 100),
                     CurrentScrollOffset(scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
  }

  // Continue the scroll. Ensure that scrolling beyond the child's extent
  // doesn't consume the delta but it isn't counted as overscroll.
  {
    InputHandlerScrollResult result = host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(120, 140),
                    ui::ScrollInputType::kTouchscreen)
            .get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(200, 200),
                     CurrentScrollOffset(scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
    EXPECT_FALSE(result.did_overscroll_root);
  }

  // Continue the scroll. Ensure that scrolling beyond the child's extent
  // doesn't consume the delta but it isn't counted as overscroll.
  {
    InputHandlerScrollResult result = host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(20, 40),
                    ui::ScrollInputType::kTouchscreen)
            .get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(200, 200),
                     CurrentScrollOffset(scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
    EXPECT_FALSE(result.did_overscroll_root);
  }

  host_impl_->ScrollEnd();
}

TEST_F(LayerTreeHostImplTest, OverscrollOnMainThread) {
  InputHandlerScrollResult scroll_result;
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  const gfx::Size viewport_size(50, 50);
  SetupViewportLayersNoScrolls(viewport_size);

  GetScrollNode(InnerViewportScrollLayer())->main_thread_scrolling_reasons =
      MainThreadScrollingReason::kThreadedScrollingDisabled;
  GetScrollNode(OuterViewportScrollLayer())->main_thread_scrolling_reasons =
      MainThreadScrollingReason::kThreadedScrollingDisabled;

  DrawFrame();

  // Overscroll initiated outside layers will be handled by the main thread.
  EXPECT_EQ(nullptr, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                         gfx::PointF(0, 60)));
  EXPECT_EQ(
      InputHandler::SCROLL_ON_MAIN_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(0, 60), gfx::Vector2dF(0, 10),
                                   ui::ScrollInputType::kWheel)
                            .get(),
                        ui::ScrollInputType::kWheel)
          .thread);

  // Overscroll initiated inside layers will be handled by the main thread.
  EXPECT_NE(nullptr, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                         gfx::PointF(0, 0)));
  EXPECT_EQ(
      InputHandler::SCROLL_ON_MAIN_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(0, 10),
                                   ui::ScrollInputType::kWheel)
                            .get(),
                        ui::ScrollInputType::kWheel)
          .thread);
}

// Test that scrolling the inner viewport directly works, as can happen when the
// scroll chains up to it from an sibling of the outer viewport.
TEST_F(LayerTreeHostImplTest, ScrollFromOuterViewportSibling) {
  const gfx::Size viewport_size(100, 100);

  SetupViewportLayersNoScrolls(viewport_size);
  host_impl_->active_tree()->SetBrowserControlsParams(
      {10, 0, 0, 0, false, false});
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1.f, 1.f);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();

  // Create a scrolling layer that's parented directly to the inner viewport.
  // This will test that scrolls that chain up to the inner viewport without
  // passing through the outer viewport still scroll correctly and affect
  // browser controls.
  LayerImpl* scroll_layer = AddScrollableLayer(
      inner_scroll_layer, viewport_size, gfx::Size(400, 400));

  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Fully scroll the child.
  {
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(1000, 1000),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(300, 300),
                     CurrentScrollOffset(scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(inner_scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));
  }

  // Scrolling on the child now should chain up directly to the inner viewport.
  // Scrolling it should cause browser controls to hide. The outer viewport
  // should not be affected.
  {
    gfx::Vector2d scroll_delta(0, 10);
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0), scroll_delta,
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    EXPECT_EQ(0, host_impl_->active_tree()->CurrentTopControlsShownRatio());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(inner_scroll_layer));

    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 10),
                     CurrentScrollOffset(inner_scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));
    host_impl_->ScrollEnd();
  }
}

// Test that scrolls chain correctly when a child scroller on the page (e.g. a
// scrolling div) is set as the outer viewport. This happens in the
// rootScroller proposal.
TEST_F(LayerTreeHostImplTest, ScrollChainingWithReplacedOuterViewport) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();

  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  LayerImpl* content_layer = AddContentLayer();

  // Initialization: Add two nested scrolling layers, simulating a scrolling div
  // with another scrolling div inside it. Set the outer "div" to be the outer
  // viewport.
  LayerImpl* scroll_layer =
      AddScrollableLayer(content_layer, content_size, gfx::Size(400, 400));
  GetScrollNode(scroll_layer)->scrolls_outer_viewport = true;
  LayerImpl* child_scroll_layer = AddScrollableLayer(
      scroll_layer, gfx::Size(300, 300), gfx::Size(500, 500));

  auto viewport_property_ids = layer_tree_impl->ViewportPropertyIdsForTesting();
  viewport_property_ids.outer_scroll = scroll_layer->scroll_tree_index();
  layer_tree_impl->SetViewportPropertyIds(viewport_property_ids);
  UpdateDrawProperties(layer_tree_impl);

  // Scroll should target the nested scrolling layer in the content and then
  // chain to the parent scrolling layer which is now set as the outer
  // viewport. The original outer viewport layer shouldn't get any scroll here.
  {
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(200, 200),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(200, 200),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(gfx::Vector2dF(200, 200),
                     CurrentScrollOffset(child_scroll_layer));

    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(200, 200),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(200, 200),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));

    EXPECT_VECTOR_EQ(gfx::Vector2dF(200, 200),
                     CurrentScrollOffset(scroll_layer));
  }

  // Now that the nested scrolling layers are fully scrolled, further scrolls
  // would normally chain up to the "outer viewport" but since we've set the
  // scrolling content as the outer viewport, it should stop chaining there.
  {
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(100, 100),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));
  }

  // Zoom into the page by a 2X factor so that the inner viewport becomes
  // scrollable.
  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Reset the parent scrolling layer (i.e. the current outer viewport) so that
  // we can ensure viewport scrolling works correctly.
  scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset(0, 0));

  // Scrolling the content layer should now scroll the inner viewport first,
  // and then chain up to the current outer viewport (i.e. the parent scroll
  // layer).
  {
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(100, 100),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                     CurrentScrollOffset(inner_scroll_layer));

    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(100, 100),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50), CurrentScrollOffset(scroll_layer));
  }
}

// Test that scrolls chain correctly when a child scroller on the page (e.g. a
// scrolling div) is set as the outer viewport but scrolls start from a layer
// that's not a descendant of the outer viewport. This happens in the
// rootScroller proposal.
TEST_F(LayerTreeHostImplTest, RootScrollerScrollNonDescendant) {
  const gfx::Size content_size(300, 300);
  const gfx::Size viewport_size(300, 300);

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();

  SetupViewportLayersInnerScrolls(viewport_size, content_size);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  LayerImpl* content_layer = AddContentLayer();

  // Initialization: Add a scrolling layer, simulating an ordinary DIV, to be
  // set as the outer viewport. Add a sibling scrolling layer that isn't a child
  // of the outer viewport scroll layer.
  LayerImpl* outer_scroll_layer =
      AddScrollableLayer(content_layer, content_size, gfx::Size(1200, 1200));
  LayerImpl* sibling_scroll_layer = AddScrollableLayer(
      content_layer, gfx::Size(600, 600), gfx::Size(1200, 1200));

  GetScrollNode(InnerViewportScrollLayer())
      ->prevent_viewport_scrolling_from_inner = true;
  GetScrollNode(OuterViewportScrollLayer())->scrolls_outer_viewport = false;
  GetScrollNode(outer_scroll_layer)->scrolls_outer_viewport = true;
  auto viewport_property_ids = layer_tree_impl->ViewportPropertyIdsForTesting();
  viewport_property_ids.outer_scroll = outer_scroll_layer->scroll_tree_index();
  layer_tree_impl->SetViewportPropertyIds(viewport_property_ids);

  ASSERT_EQ(outer_scroll_layer,
            layer_tree_impl->OuterViewportScrollLayerForTesting());

  // Scrolls should target the non-descendant scroller. Chaining should not
  // propagate to the outer viewport scroll layer.
  {
    // This should fully scroll the layer.
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(1000, 1000),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(gfx::Vector2dF(600, 600),
                     CurrentScrollOffset(sibling_scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));

    // Scrolling now should chain up but, since the outer viewport is a sibling
    // rather than an ancestor, we shouldn't chain to it.
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(1000, 1000),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(gfx::Vector2dF(600, 600),
                     CurrentScrollOffset(sibling_scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));
  }

  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 1;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);

  gfx::Vector2dF viewport_size_vec(viewport_size.width(),
                                   viewport_size.height());

  // Reset the scroll offset.
  sibling_scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset());

  // Now pinch-zoom in. Anchoring should cause scrolling only on the inner
  // viewport layer.
  {
    // Pinch in to the middle of the screen. The inner viewport should scroll
    // to keep the gesture anchored but not the outer or the sibling scroller.
    page_scale_factor = 2;
    gfx::Point anchor(viewport_size.width() / 2, viewport_size.height() / 2);
    host_impl_->ScrollBegin(
        BeginState(anchor, gfx::Vector2dF(), ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_factor, anchor);
    host_impl_->PinchGestureEnd(anchor, true);

    EXPECT_VECTOR_EQ(gfx::Vector2dF(anchor.x() / 2, anchor.y() / 2),
                     CurrentScrollOffset(inner_scroll_layer));

    host_impl_->ScrollUpdate(UpdateState(anchor, viewport_size_vec,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());

    EXPECT_VECTOR_EQ(ScaleVector2d(viewport_size_vec, 1 / page_scale_factor),
                     CurrentScrollOffset(inner_scroll_layer));
    // TODO(bokan): This doesn't yet work but we'll probably want to fix this
    // at some point.
    // EXPECT_VECTOR_EQ(
    //     gfx::Vector2dF(),
    //     CurrentScrollOffset(outer_scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     CurrentScrollOffset(sibling_scroll_layer));

    host_impl_->ScrollEnd();
  }

  // Reset the scroll offsets
  sibling_scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset());
  inner_scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset());
  outer_scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset());

  // Scrolls over the sibling while pinched in should scroll the sibling first,
  // but then chain up to the inner viewport so that the user can still pan
  // around. The outer viewport should be unaffected.
  {
    // This should fully scroll the sibling but, because we latch to the
    // scroller, it shouldn't chain up to the inner viewport yet.
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(2000, 2000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(2000, 2000),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(gfx::Vector2dF(600, 600),
                     CurrentScrollOffset(sibling_scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(inner_scroll_layer));

    // Scrolling now should chain up to the inner viewport.
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(2000, 2000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(2000, 2000),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(ScaleVector2d(viewport_size_vec, 1 / page_scale_factor),
                     CurrentScrollOffset(inner_scroll_layer));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));

    // No more scrolling should be possible.
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(2000, 2000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(2000, 2000),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));
  }
}

TEST_F(LayerTreeHostImplTest, OverscrollOnImplThread) {
  InputHandlerScrollResult scroll_result;
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  const gfx::Size content_size(50, 50);
  SetupViewportLayersNoScrolls(content_size);

  // By default, no main thread scrolling reasons should exist.
  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  ScrollNode* scroll_node = GetScrollNode(scroll_layer);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            scroll_node->main_thread_scrolling_reasons);

  DrawFrame();

  // Overscroll initiated outside layers will be handled by the impl thread.
  EXPECT_EQ(nullptr, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                         gfx::PointF(0, 60)));
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(0, 60), gfx::Vector2dF(0, 10),
                                   ui::ScrollInputType::kWheel)
                            .get(),
                        ui::ScrollInputType::kWheel)
          .thread);

  host_impl_->ScrollEnd();

  // Overscroll initiated inside layers will be handled by the impl thread.
  EXPECT_NE(nullptr, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                         gfx::PointF(0, 0)));
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(0, 10),
                                   ui::ScrollInputType::kWheel)
                            .get(),
                        ui::ScrollInputType::kWheel)
          .thread);
}

class BlendStateCheckLayer : public LayerImpl {
 public:
  static std::unique_ptr<BlendStateCheckLayer> Create(
      LayerTreeImpl* tree_impl,
      int id,
      viz::ClientResourceProvider* resource_provider) {
    return base::WrapUnique(
        new BlendStateCheckLayer(tree_impl, id, resource_provider));
  }

  BlendStateCheckLayer(LayerTreeImpl* tree_impl,
                       int id,
                       viz::ClientResourceProvider* resource_provider)
      : LayerImpl(tree_impl, id),
        resource_provider_(resource_provider),
        blend_(false),
        has_render_surface_(false),
        comparison_layer_(nullptr),
        quads_appended_(false),
        quad_rect_(5, 5, 5, 5),
        quad_visible_rect_(5, 5, 5, 5) {
    resource_id_ = resource_provider_->ImportResource(
        viz::TransferableResource::MakeSoftware(
            viz::SharedBitmap::GenerateId(), gfx::Size(1, 1), viz::RGBA_8888),
        viz::SingleReleaseCallback::Create(base::DoNothing()));
    SetBounds(gfx::Size(10, 10));
    SetDrawsContent(true);
  }

  void ReleaseResources() override {
    resource_provider_->RemoveImportedResource(resource_id_);
  }

  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    quads_appended_ = true;

    gfx::Rect opaque_rect;
    if (contents_opaque())
      opaque_rect = quad_rect_;
    else
      opaque_rect = opaque_content_rect_;
    gfx::Rect visible_quad_rect = quad_visible_rect_;
    bool needs_blending = !opaque_rect.Contains(visible_quad_rect);

    viz::SharedQuadState* shared_quad_state =
        render_pass->CreateAndAppendSharedQuadState();
    PopulateSharedQuadState(shared_quad_state, contents_opaque());

    auto* test_blending_draw_quad =
        render_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
    test_blending_draw_quad->SetNew(shared_quad_state, quad_rect_,
                                    visible_quad_rect, needs_blending,
                                    resource_id_, gfx::RectF(0, 0, 1, 1),
                                    gfx::Size(1, 1), false, false, false);

    EXPECT_EQ(blend_, test_blending_draw_quad->ShouldDrawWithBlending());
    EXPECT_EQ(has_render_surface_,
              GetRenderSurface(this) != GetRenderSurface(comparison_layer_));
  }

  void SetExpectation(bool blend,
                      bool has_render_surface,
                      LayerImpl* comparison_layer) {
    blend_ = blend;
    has_render_surface_ = has_render_surface;
    comparison_layer_ = comparison_layer;
    quads_appended_ = false;
  }

  bool quads_appended() const { return quads_appended_; }

  void SetQuadRect(const gfx::Rect& rect) { quad_rect_ = rect; }
  void SetQuadVisibleRect(const gfx::Rect& rect) { quad_visible_rect_ = rect; }
  void SetOpaqueContentRect(const gfx::Rect& rect) {
    opaque_content_rect_ = rect;
  }

 private:
  viz::ClientResourceProvider* resource_provider_;
  bool blend_;
  bool has_render_surface_;
  LayerImpl* comparison_layer_;
  bool quads_appended_;
  gfx::Rect quad_rect_;
  gfx::Rect opaque_content_rect_;
  gfx::Rect quad_visible_rect_;
  viz::ResourceId resource_id_;
};

TEST_F(LayerTreeHostImplTest, BlendingOffWhenDrawingOpaqueLayers) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(10, 10));
  root->SetDrawsContent(false);

  auto* layer1 = AddLayer<BlendStateCheckLayer>(
      host_impl_->active_tree(), host_impl_->resource_provider());
  CopyProperties(root, layer1);
  CreateTransformNode(layer1).post_translation = gfx::Vector2dF(2, 2);
  CreateEffectNode(layer1);

  // Opaque layer, drawn without blending.
  layer1->SetContentsOpaque(true);
  layer1->SetExpectation(false, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());

  // Layer with translucent content and painting, so drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetExpectation(true, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());

  // Layer with translucent opacity, drawn with blending.
  layer1->SetContentsOpaque(true);
  SetOpacity(layer1, 0.5f);
  layer1->SetExpectation(true, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());

  // Layer with translucent opacity and painting, drawn with blending.
  layer1->SetContentsOpaque(true);
  SetOpacity(layer1, 0.5f);
  layer1->SetExpectation(true, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());

  auto* layer2 = AddLayer<BlendStateCheckLayer>(
      host_impl_->active_tree(), host_impl_->resource_provider());
  CopyProperties(layer1, layer2);
  CreateTransformNode(layer2).post_translation = gfx::Vector2dF(4, 4);
  CreateEffectNode(layer2);

  // 2 opaque layers, drawn without blending.
  layer1->SetContentsOpaque(true);
  SetOpacity(layer1, 1);
  layer1->SetExpectation(false, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetContentsOpaque(true);
  SetOpacity(layer2, 1);
  layer2->SetExpectation(false, false, root);
  layer2->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());

  // Parent layer with translucent content, drawn with blending.
  // Child layer with opaque content, drawn without blending.
  layer1->SetContentsOpaque(false);
  layer1->SetExpectation(true, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetExpectation(false, false, root);
  layer2->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());

  // Parent layer with translucent content but opaque painting, drawn without
  // blending.
  // Child layer with opaque content, drawn without blending.
  layer1->SetContentsOpaque(true);
  layer1->SetExpectation(false, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetExpectation(false, false, root);
  layer2->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());

  // Parent layer with translucent opacity and opaque content. Since it has a
  // drawing child, it's drawn to a render surface which carries the opacity,
  // so it's itself drawn without blending.
  // Child layer with opaque content, drawn without blending (parent surface
  // carries the inherited opacity).
  layer1->SetContentsOpaque(true);
  SetOpacity(layer1, 0.5f);
  GetEffectNode(layer1)->render_surface_reason = RenderSurfaceReason::kTest;
  layer1->SetExpectation(false, true, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetExpectation(false, false, layer1);
  layer2->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  GetEffectNode(layer1)->render_surface_reason = RenderSurfaceReason::kNone;

  // Draw again, but with child non-opaque, to make sure
  // layer1 not culled.
  layer1->SetContentsOpaque(true);
  SetOpacity(layer1, 1);
  layer1->SetExpectation(false, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetContentsOpaque(true);
  SetOpacity(layer2, 0.5f);
  layer2->SetExpectation(true, false, layer1);
  layer2->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());

  // A second way of making the child non-opaque.
  layer1->SetContentsOpaque(true);
  SetOpacity(layer1, 1);
  layer1->SetExpectation(false, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetContentsOpaque(false);
  SetOpacity(layer2, 1);
  layer2->SetExpectation(true, false, root);
  layer2->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());

  // And when the layer says its not opaque but is painted opaque, it is not
  // blended.
  layer1->SetContentsOpaque(true);
  SetOpacity(layer1, 1);
  layer1->SetExpectation(false, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetContentsOpaque(true);
  SetOpacity(layer2, 1);
  layer2->SetExpectation(false, false, root);
  layer2->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());

  // Layer with partially opaque contents, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());

  // Layer with partially opaque contents partially culled, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 5, 2));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());

  // Layer with partially opaque contents culled, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(7, 5, 3, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());

  // Layer with partially opaque contents and translucent contents culled, drawn
  // without blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(false, false, root);
  layer1->UnionUpdateRect(gfx::Rect(layer1->bounds()));
  DrawFrame();
  EXPECT_TRUE(layer1->quads_appended());
}

static bool MayContainVideoBitSetOnFrameData(LayerTreeHostImpl* host_impl) {
  host_impl->active_tree()->set_needs_update_draw_properties();
  TestFrameData frame;
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  host_impl->WillBeginImplFrame(args);
  EXPECT_EQ(DRAW_SUCCESS, host_impl->PrepareToDraw(&frame));
  host_impl->DrawLayers(&frame);
  host_impl->DidDrawAllLayers(frame);
  host_impl->DidFinishImplFrame(args);
  return frame.may_contain_video;
}

TEST_F(LayerTreeHostImplTest, MayContainVideo) {
  gfx::Size big_size(1000, 1000);
  auto* root =
      SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(), big_size);
  auto* video_layer = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  video_layer->set_may_contain_video(true);
  CopyProperties(root, video_layer);
  UpdateDrawProperties(host_impl_->active_tree());
  EXPECT_TRUE(MayContainVideoBitSetOnFrameData(host_impl_.get()));

  // Test with the video layer occluded.
  auto* large_layer = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  large_layer->SetBounds(big_size);
  large_layer->SetContentsOpaque(true);
  CopyProperties(root, large_layer);
  UpdateDrawProperties(host_impl_->active_tree());
  EXPECT_FALSE(MayContainVideoBitSetOnFrameData(host_impl_.get()));

  {
    // Remove the large layer.
    OwnedLayerImplList layers =
        host_impl_->active_tree()->DetachLayersKeepingRootLayerForTesting();
    ASSERT_EQ(video_layer, layers[1].get());
    host_impl_->active_tree()->AddLayer(std::move(layers[1]));
  }
  UpdateDrawProperties(host_impl_->active_tree());
  EXPECT_TRUE(MayContainVideoBitSetOnFrameData(host_impl_.get()));

  // Move the video layer so it goes beyond the root.
  video_layer->SetOffsetToTransformParent(gfx::Vector2dF(100, 100));
  UpdateDrawProperties(host_impl_->active_tree());
  EXPECT_FALSE(MayContainVideoBitSetOnFrameData(host_impl_.get()));

  video_layer->SetOffsetToTransformParent(gfx::Vector2dF(0, 0));
  video_layer->NoteLayerPropertyChanged();
  UpdateDrawProperties(host_impl_->active_tree());
  EXPECT_TRUE(MayContainVideoBitSetOnFrameData(host_impl_.get()));
}

class LayerTreeHostImplViewportCoveredTest : public LayerTreeHostImplTest {
 protected:
  LayerTreeHostImplViewportCoveredTest()
      : gutter_quad_material_(viz::DrawQuad::Material::kSolidColor),
        child_(nullptr),
        did_activate_pending_tree_(false) {}

  std::unique_ptr<LayerTreeFrameSink> CreateFakeLayerTreeFrameSink(
      bool software) {
    if (software)
      return FakeLayerTreeFrameSink::CreateSoftware();
    return FakeLayerTreeFrameSink::Create3d();
  }

  void SetupActiveTreeLayers() {
    host_impl_->active_tree()->set_background_color(SK_ColorGRAY);
    LayerImpl* root = SetupDefaultRootLayer(viewport_size_);
    child_ = AddLayer<BlendStateCheckLayer>(host_impl_->active_tree(),
                                            host_impl_->resource_provider());
    child_->SetExpectation(false, false, root);
    child_->SetContentsOpaque(true);
    CopyProperties(root, child_);
    UpdateDrawProperties(host_impl_->active_tree());
  }

  void SetLayerGeometry(const gfx::Rect& layer_rect) {
    child_->SetBounds(layer_rect.size());
    child_->SetQuadRect(gfx::Rect(layer_rect.size()));
    child_->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));
    child_->SetOffsetToTransformParent(
        gfx::Vector2dF(layer_rect.OffsetFromOrigin()));
  }

  // Expect no gutter rects.
  void TestLayerCoversFullViewport() {
    SetLayerGeometry(gfx::Rect(viewport_size_));

    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    ASSERT_EQ(1u, frame.render_passes.size());

    EXPECT_EQ(0u, CountGutterQuads(frame.render_passes[0]->quad_list));
    EXPECT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(frame.render_passes[0]->quad_list);

    VerifyQuadsExactlyCoverViewport(frame.render_passes[0]->quad_list);
    host_impl_->DidDrawAllLayers(frame);
  }

  // Expect fullscreen gutter rect.
  void SetUpEmptylayer() { SetLayerGeometry(gfx::Rect()); }

  void VerifyEmptyLayerRenderPasses(const viz::RenderPassList& render_passes) {
    ASSERT_EQ(1u, render_passes.size());

    EXPECT_EQ(1u, CountGutterQuads(render_passes[0]->quad_list));
    EXPECT_EQ(1u, render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(render_passes[0]->quad_list);

    VerifyQuadsExactlyCoverViewport(render_passes[0]->quad_list);
  }

  void TestEmptyLayer() {
    SetUpEmptylayer();
    DrawFrame();
  }

  void TestEmptyLayerWithOnDraw() {
    SetUpEmptylayer();
    gfx::Transform identity;
    gfx::Rect viewport(viewport_size_);
    bool resourceless_software_draw = true;
    host_impl_->OnDraw(identity, viewport, resourceless_software_draw, false);
    VerifyEmptyLayerRenderPasses(last_on_draw_render_passes_);
  }

  // Expect four surrounding gutter rects.
  void SetUpLayerInMiddleOfViewport() {
    SetLayerGeometry(gfx::Rect(500, 500, 200, 200));
  }

  void VerifyLayerInMiddleOfViewport(const viz::RenderPassList& render_passes) {
    ASSERT_EQ(1u, render_passes.size());

    EXPECT_EQ(4u, CountGutterQuads(render_passes[0]->quad_list));
    EXPECT_EQ(5u, render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(render_passes[0]->quad_list);

    VerifyQuadsExactlyCoverViewport(render_passes[0]->quad_list);
  }

  void TestLayerInMiddleOfViewport() {
    SetUpLayerInMiddleOfViewport();
    DrawFrame();
  }

  void TestLayerInMiddleOfViewportWithOnDraw() {
    SetUpLayerInMiddleOfViewport();
    gfx::Transform identity;
    gfx::Rect viewport(viewport_size_);
    bool resourceless_software_draw = true;
    host_impl_->OnDraw(identity, viewport, resourceless_software_draw, false);
    VerifyLayerInMiddleOfViewport(last_on_draw_render_passes_);
  }

  // Expect no gutter rects.
  void SetUpLayerIsLargerThanViewport() {
    SetLayerGeometry(
        gfx::Rect(viewport_size_.width() + 10, viewport_size_.height() + 10));
  }

  void VerifyLayerIsLargerThanViewport(
      const viz::RenderPassList& render_passes) {
    ASSERT_EQ(1u, render_passes.size());

    EXPECT_EQ(0u, CountGutterQuads(render_passes[0]->quad_list));
    EXPECT_EQ(1u, render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(render_passes[0]->quad_list);
  }

  void TestLayerIsLargerThanViewport() {
    SetUpLayerIsLargerThanViewport();
    DrawFrame();
  }

  void TestLayerIsLargerThanViewportWithOnDraw() {
    SetUpLayerIsLargerThanViewport();
    gfx::Transform identity;
    gfx::Rect viewport(viewport_size_);
    bool resourceless_software_draw = true;
    host_impl_->OnDraw(identity, viewport, resourceless_software_draw, false);
    VerifyLayerIsLargerThanViewport(last_on_draw_render_passes_);
  }

  void DidActivateSyncTree() override {
    LayerTreeHostImplTest::DidActivateSyncTree();
    did_activate_pending_tree_ = true;
  }

  void set_gutter_quad_material(viz::DrawQuad::Material material) {
    gutter_quad_material_ = material;
  }
  void set_gutter_texture_size(const gfx::Size& gutter_texture_size) {
    gutter_texture_size_ = gutter_texture_size;
  }

 protected:
  size_t CountGutterQuads(const viz::QuadList& quad_list) {
    size_t num_gutter_quads = 0;
    for (auto* quad : quad_list) {
      num_gutter_quads += (quad->material == gutter_quad_material_) ? 1 : 0;
    }
    return num_gutter_quads;
  }

  void VerifyQuadsExactlyCoverViewport(const viz::QuadList& quad_list) {
    VerifyQuadsExactlyCoverRect(quad_list,
                                gfx::Rect(DipSizeToPixelSize(viewport_size_)));
  }

  // Make sure that the texture coordinates match their expectations.
  void ValidateTextureDrawQuads(const viz::QuadList& quad_list) {
    for (auto* quad : quad_list) {
      if (quad->material != viz::DrawQuad::Material::kTextureContent)
        continue;
      const viz::TextureDrawQuad* texture_quad =
          viz::TextureDrawQuad::MaterialCast(quad);
      gfx::SizeF gutter_texture_size_pixels =
          gfx::ScaleSize(gfx::SizeF(gutter_texture_size_),
                         host_impl_->active_tree()->device_scale_factor());
      EXPECT_EQ(texture_quad->uv_top_left.x(),
                texture_quad->rect.x() / gutter_texture_size_pixels.width());
      EXPECT_EQ(texture_quad->uv_top_left.y(),
                texture_quad->rect.y() / gutter_texture_size_pixels.height());
      EXPECT_EQ(
          texture_quad->uv_bottom_right.x(),
          texture_quad->rect.right() / gutter_texture_size_pixels.width());
      EXPECT_EQ(
          texture_quad->uv_bottom_right.y(),
          texture_quad->rect.bottom() / gutter_texture_size_pixels.height());
    }
  }

  viz::DrawQuad::Material gutter_quad_material_;
  gfx::Size gutter_texture_size_;
  gfx::Size viewport_size_;
  BlendStateCheckLayer* child_;
  bool did_activate_pending_tree_;
};

TEST_F(LayerTreeHostImplViewportCoveredTest, ViewportCovered) {
  viewport_size_ = gfx::Size(1000, 1000);

  bool software = false;
  CreateHostImpl(DefaultSettings(), CreateFakeLayerTreeFrameSink(software));
  SetupActiveTreeLayers();
  EXPECT_SCOPED(TestLayerCoversFullViewport());
  EXPECT_SCOPED(TestEmptyLayer());
  EXPECT_SCOPED(TestLayerInMiddleOfViewport());
  EXPECT_SCOPED(TestLayerIsLargerThanViewport());
}

TEST_F(LayerTreeHostImplViewportCoveredTest, ViewportCoveredScaled) {
  viewport_size_ = gfx::Size(1000, 1000);

  bool software = false;
  CreateHostImpl(DefaultSettings(), CreateFakeLayerTreeFrameSink(software));

  host_impl_->active_tree()->SetDeviceScaleFactor(2);
  SetupActiveTreeLayers();
  EXPECT_SCOPED(TestLayerCoversFullViewport());
  EXPECT_SCOPED(TestEmptyLayer());
  EXPECT_SCOPED(TestLayerInMiddleOfViewport());
  EXPECT_SCOPED(TestLayerIsLargerThanViewport());
}

TEST_F(LayerTreeHostImplViewportCoveredTest, ActiveTreeGrowViewportInvalid) {
  viewport_size_ = gfx::Size(1000, 1000);

  bool software = true;
  CreateHostImpl(DefaultSettings(), CreateFakeLayerTreeFrameSink(software));

  // Pending tree to force active_tree size invalid. Not used otherwise.
  CreatePendingTree();

  SetupActiveTreeLayers();
  EXPECT_SCOPED(TestEmptyLayerWithOnDraw());
  EXPECT_SCOPED(TestLayerInMiddleOfViewportWithOnDraw());
  EXPECT_SCOPED(TestLayerIsLargerThanViewportWithOnDraw());
}

TEST_F(LayerTreeHostImplViewportCoveredTest, ActiveTreeShrinkViewportInvalid) {
  viewport_size_ = gfx::Size(1000, 1000);

  bool software = true;
  CreateHostImpl(DefaultSettings(), CreateFakeLayerTreeFrameSink(software));

  // Set larger viewport and activate it to active tree.
  CreatePendingTree();
  gfx::Size larger_viewport(viewport_size_.width() + 100,
                            viewport_size_.height() + 100);
  host_impl_->active_tree()->SetDeviceViewportRect(
      gfx::Rect(DipSizeToPixelSize(larger_viewport)));
  host_impl_->ActivateSyncTree();
  EXPECT_TRUE(did_activate_pending_tree_);

  // Shrink pending tree viewport without activating.
  CreatePendingTree();
  host_impl_->active_tree()->SetDeviceViewportRect(
      gfx::Rect(DipSizeToPixelSize(viewport_size_)));

  SetupActiveTreeLayers();
  EXPECT_SCOPED(TestEmptyLayerWithOnDraw());
  EXPECT_SCOPED(TestLayerInMiddleOfViewportWithOnDraw());
  EXPECT_SCOPED(TestLayerIsLargerThanViewportWithOnDraw());
}

class FakeDrawableLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return base::WrapUnique(new FakeDrawableLayerImpl(tree_impl, id));
  }

 protected:
  FakeDrawableLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id) {}
};

// Make sure damage tracking propagates all the way to the viz::CompositorFrame
// submitted to the LayerTreeFrameSink, where it should request to swap only
// the sub-buffer that is damaged.
TEST_F(LayerTreeHostImplTest, PartialSwapReceivesDamageRect) {
  auto gl_owned = std::make_unique<viz::TestGLES2Interface>();
  gl_owned->set_have_post_sub_buffer(true);
  scoped_refptr<viz::TestContextProvider> context_provider(
      viz::TestContextProvider::Create(std::move(gl_owned)));
  context_provider->BindToCurrentThread();

  std::unique_ptr<FakeLayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d(context_provider));
  FakeLayerTreeFrameSink* fake_layer_tree_frame_sink =
      layer_tree_frame_sink.get();

  // This test creates its own LayerTreeHostImpl, so
  // that we can force partial swap enabled.
  LayerTreeSettings settings = DefaultSettings();
  std::unique_ptr<LayerTreeHostImpl> layer_tree_host_impl =
      LayerTreeHostImpl::Create(
          settings, this, &task_runner_provider_, &stats_instrumentation_,
          &task_graph_runner_,
          AnimationHost::CreateForTesting(ThreadInstance::IMPL), 0, nullptr,
          nullptr);
  layer_tree_host_impl->SetVisible(true);
  layer_tree_host_impl->InitializeFrameSink(layer_tree_frame_sink.get());

  LayerImpl* root = SetupRootLayer<LayerImpl>(
      layer_tree_host_impl->active_tree(), gfx::Size(500, 500));
  LayerImpl* child = AddLayer<LayerImpl>(layer_tree_host_impl->active_tree());
  child->SetBounds(gfx::Size(14, 15));
  child->SetDrawsContent(true);
  CopyProperties(root, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(12, 13));
  layer_tree_host_impl->active_tree()->SetLocalSurfaceIdAllocationFromParent(
      viz::LocalSurfaceIdAllocation(
          viz::LocalSurfaceId(1, base::UnguessableToken::Deserialize(2u, 3u)),
          base::TimeTicks::Now()));
  UpdateDrawProperties(layer_tree_host_impl->active_tree());

  TestFrameData frame;

  // First frame, the entire screen should get swapped.
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  layer_tree_host_impl->WillBeginImplFrame(args);
  EXPECT_EQ(DRAW_SUCCESS, layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame);
  layer_tree_host_impl->DidDrawAllLayers(frame);
  layer_tree_host_impl->DidFinishImplFrame(args);
  gfx::Rect expected_swap_rect(500, 500);
  EXPECT_EQ(expected_swap_rect.ToString(),
            fake_layer_tree_frame_sink->last_swap_rect().ToString());

  // Second frame, only the damaged area should get swapped. Damage should be
  // the union of old and new child rects: gfx::Rect(26, 28).
  child->SetOffsetToTransformParent(gfx::Vector2dF());
  child->NoteLayerPropertyChanged();
  args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  layer_tree_host_impl->WillBeginImplFrame(args);
  EXPECT_EQ(DRAW_SUCCESS, layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame);
  layer_tree_host_impl->DidDrawAllLayers(frame);
  layer_tree_host_impl->DidFinishImplFrame(args);

  expected_swap_rect = gfx::Rect(26, 28);
  EXPECT_EQ(expected_swap_rect.ToString(),
            fake_layer_tree_frame_sink->last_swap_rect().ToString());

  layer_tree_host_impl->active_tree()->SetDeviceViewportRect(gfx::Rect(10, 10));
  // This will damage everything.
  root->SetBackgroundColor(SK_ColorBLACK);
  args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  layer_tree_host_impl->WillBeginImplFrame(args);
  EXPECT_EQ(DRAW_SUCCESS, layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame);
  layer_tree_host_impl->DidDrawAllLayers(frame);
  layer_tree_host_impl->DidFinishImplFrame(args);

  expected_swap_rect = gfx::Rect(10, 10);
  EXPECT_EQ(expected_swap_rect.ToString(),
            fake_layer_tree_frame_sink->last_swap_rect().ToString());

  layer_tree_host_impl->ReleaseLayerTreeFrameSink();
}

TEST_F(LayerTreeHostImplTest, RootLayerDoesntCreateExtraSurface) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(10, 10));
  LayerImpl* child = AddLayer();
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  root->SetBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  CopyProperties(root, child);

  UpdateDrawProperties(host_impl_->active_tree());

  TestFrameData frame;

  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  EXPECT_EQ(1u, frame.render_surface_list->size());
  EXPECT_EQ(1u, frame.render_passes.size());
  host_impl_->DidDrawAllLayers(frame);
}

class FakeLayerWithQuads : public LayerImpl {
 public:
  static std::unique_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return base::WrapUnique(new FakeLayerWithQuads(tree_impl, id));
  }

  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    viz::SharedQuadState* shared_quad_state =
        render_pass->CreateAndAppendSharedQuadState();
    PopulateSharedQuadState(shared_quad_state, contents_opaque());

    SkColor gray = SkColorSetRGB(100, 100, 100);
    gfx::Rect quad_rect(bounds());
    gfx::Rect visible_quad_rect(quad_rect);
    auto* my_quad =
        render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    my_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, gray,
                    false);
  }

 private:
  FakeLayerWithQuads(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id) {}
};

TEST_F(LayerTreeHostImplTest, LayersFreeTextures) {
  scoped_refptr<viz::TestContextProvider> context_provider =
      viz::TestContextProvider::Create();
  viz::TestSharedImageInterface* sii = context_provider->SharedImageInterface();
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d(context_provider));
  CreateHostImpl(DefaultSettings(), std::move(layer_tree_frame_sink));

  LayerImpl* root_layer = SetupDefaultRootLayer(gfx::Size(10, 10));

  scoped_refptr<VideoFrame> softwareFrame = media::VideoFrame::CreateColorFrame(
      gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(softwareFrame);
  auto* video_layer = AddLayer<VideoLayerImpl>(
      host_impl_->active_tree(), &provider, media::VIDEO_ROTATION_0);
  video_layer->SetBounds(gfx::Size(10, 10));
  video_layer->SetDrawsContent(true);
  CopyProperties(root_layer, video_layer);

  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_EQ(0u, sii->shared_image_count());

  DrawFrame();

  EXPECT_GT(sii->shared_image_count(), 0u);

  // Kill the layer tree.
  ClearLayersAndPropertyTrees(host_impl_->active_tree());
  // There should be no textures left in use after.
  EXPECT_EQ(0u, sii->shared_image_count());
}

TEST_F(LayerTreeHostImplTest, HasTransparentBackground) {
  SetupDefaultRootLayer(gfx::Size(10, 10));
  host_impl_->active_tree()->set_background_color(SK_ColorWHITE);
  UpdateDrawProperties(host_impl_->active_tree());

  // Verify one quad is drawn when transparent background set is not set.
  TestFrameData frame;
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  host_impl_->WillBeginImplFrame(args);
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  {
    const auto& root_pass = frame.render_passes.back();
    ASSERT_EQ(1u, root_pass->quad_list.size());
    EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
              root_pass->quad_list.front()->material);
  }
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);

  // Cause damage so we would draw something if possible.
  host_impl_->SetFullViewportDamage();

  // Verify no quads are drawn when transparent background is set.
  host_impl_->active_tree()->set_background_color(SK_ColorTRANSPARENT);
  host_impl_->SetFullViewportDamage();
  args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  host_impl_->WillBeginImplFrame(args);
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  {
    const auto& root_pass = frame.render_passes.back();
    ASSERT_EQ(0u, root_pass->quad_list.size());
  }
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);

  // Cause damage so we would draw something if possible.
  host_impl_->SetFullViewportDamage();

  // Verify no quads are drawn when semi-transparent background is set.
  host_impl_->active_tree()->set_background_color(SkColorSetARGB(5, 255, 0, 0));
  host_impl_->SetFullViewportDamage();
  host_impl_->WillBeginImplFrame(viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1)));
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  {
    const auto& root_pass = frame.render_passes.back();
    ASSERT_EQ(0u, root_pass->quad_list.size());
  }
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);
}

class LayerTreeHostImplTestDrawAndTestDamage : public LayerTreeHostImplTest {
 protected:
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }

  void DrawFrameAndTestDamage(const gfx::Rect& expected_damage,
                              const LayerImpl* child) {
    bool expect_to_draw = !expected_damage.IsEmpty();

    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

    if (!expect_to_draw) {
      // With no damage, we don't draw, and no quads are created.
      ASSERT_EQ(0u, frame.render_passes.size());
    } else {
      ASSERT_EQ(1u, frame.render_passes.size());

      // Verify the damage rect for the root render pass.
      const viz::RenderPass* root_render_pass =
          frame.render_passes.back().get();
      EXPECT_EQ(expected_damage, root_render_pass->damage_rect);

      // Verify the root and child layers' quads are generated and not being
      // culled.
      ASSERT_EQ(2u, root_render_pass->quad_list.size());

      gfx::Rect expected_child_visible_rect(child->bounds());
      EXPECT_EQ(expected_child_visible_rect,
                root_render_pass->quad_list.front()->visible_rect);

      LayerImpl* root = root_layer();
      gfx::Rect expected_root_visible_rect(root->bounds());
      EXPECT_EQ(expected_root_visible_rect,
                root_render_pass->quad_list.ElementAt(1)->visible_rect);
    }

    EXPECT_EQ(expect_to_draw, host_impl_->DrawLayers(&frame));
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }
};

TEST_F(LayerTreeHostImplTestDrawAndTestDamage, FrameIncludesDamageRect) {
  auto* root = SetupRootLayer<SolidColorLayerImpl>(host_impl_->active_tree(),
                                                   gfx::Size(10, 10));
  root->SetDrawsContent(true);
  root->SetBackgroundColor(SK_ColorRED);

  // Child layer is in the bottom right corner.
  auto* child = AddLayer<SolidColorLayerImpl>(host_impl_->active_tree());
  child->SetBounds(gfx::Size(1, 1));
  child->SetDrawsContent(true);
  child->SetBackgroundColor(SK_ColorRED);
  CopyProperties(root, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(9, 9));

  UpdateDrawProperties(host_impl_->active_tree());

  // Draw a frame. In the first frame, the entire viewport should be damaged.
  gfx::Rect full_frame_damage(
      host_impl_->active_tree()->GetDeviceViewport().size());
  DrawFrameAndTestDamage(full_frame_damage, child);

  // The second frame has damage that doesn't touch the child layer. Its quads
  // should still be generated.
  gfx::Rect small_damage = gfx::Rect(0, 0, 1, 1);
  root->UnionUpdateRect(small_damage);
  DrawFrameAndTestDamage(small_damage, child);

  // The third frame should have no damage, so no quads should be generated.
  gfx::Rect no_damage;
  DrawFrameAndTestDamage(no_damage, child);
}

class GLRendererWithSetupQuadForAntialiasing : public viz::GLRenderer {
 public:
  using viz::GLRenderer::ShouldAntialiasQuad;
};

TEST_F(LayerTreeHostImplTest, FarAwayQuadsDontNeedAA) {
  // Due to precision issues (especially on Android), sometimes far
  // away quads can end up thinking they need AA.
  float device_scale_factor = 4 / 3;
  gfx::Size root_size(2000, 1000);
  CreatePendingTree();
  host_impl_->pending_tree()->SetDeviceScaleFactor(device_scale_factor);
  host_impl_->pending_tree()->PushPageScaleFromMainThread(1, 1 / 16, 16);

  auto* root = SetupRootLayer<LayerImpl>(host_impl_->pending_tree(), root_size);
  root->SetNeedsPushProperties();

  gfx::Size content_layer_bounds(100001, 100);
  auto* scrolling_layer =
      AddScrollableLayer(root, content_layer_bounds, gfx::Size());
  scrolling_layer->SetNeedsPushProperties();

  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(content_layer_bounds));

  auto* content_layer =
      AddLayer<FakePictureLayerImpl>(host_impl_->pending_tree(), raster_source);
  CopyProperties(scrolling_layer, content_layer);
  content_layer->SetBounds(content_layer_bounds);
  content_layer->SetDrawsContent(true);
  content_layer->SetNeedsPushProperties();

  UpdateDrawProperties(host_impl_->pending_tree());

  gfx::ScrollOffset scroll_offset(100000, 0);
  scrolling_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          scrolling_layer->element_id(), scroll_offset);
  host_impl_->ActivateSyncTree();

  UpdateDrawProperties(host_impl_->active_tree());
  ASSERT_EQ(1u, host_impl_->active_tree()->GetRenderSurfaceList().size());

  TestFrameData frame;
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  host_impl_->WillBeginImplFrame(args);
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(1u, frame.render_passes.size());
  ASSERT_LE(1u, frame.render_passes[0]->quad_list.size());
  const viz::DrawQuad* quad = frame.render_passes[0]->quad_list.front();

  bool clipped = false, force_aa = false;
  gfx::QuadF device_layer_quad = MathUtil::MapQuad(
      quad->shared_quad_state->quad_to_target_transform,
      gfx::QuadF(gfx::RectF(quad->shared_quad_state->visible_quad_layer_rect)),
      &clipped);
  EXPECT_FALSE(clipped);
  bool antialiased =
      GLRendererWithSetupQuadForAntialiasing::ShouldAntialiasQuad(
          device_layer_quad, clipped, force_aa);
  EXPECT_FALSE(antialiased);

  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);
}

class CompositorFrameMetadataTest : public LayerTreeHostImplTest {
 public:
  CompositorFrameMetadataTest() = default;

  void DidReceiveCompositorFrameAckOnImplThread() override { acks_received_++; }

  int acks_received_ = 0;
};

TEST_F(CompositorFrameMetadataTest, CompositorFrameAckCountsAsSwapComplete) {
  SetupRootLayer<FakeLayerWithQuads>(host_impl_->active_tree(),
                                     gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());
  DrawFrame();
  host_impl_->ReclaimResources(std::vector<viz::ReturnedResource>());
  host_impl_->DidReceiveCompositorFrameAck();
  EXPECT_EQ(acks_received_, 1);
}

class CountingSoftwareDevice : public viz::SoftwareOutputDevice {
 public:
  CountingSoftwareDevice() : frames_began_(0), frames_ended_(0) {}

  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override {
    ++frames_began_;
    return viz::SoftwareOutputDevice::BeginPaint(damage_rect);
  }
  void EndPaint() override {
    viz::SoftwareOutputDevice::EndPaint();
    ++frames_ended_;
  }

  int frames_began_, frames_ended_;
};

TEST_F(LayerTreeHostImplTest,
       ForcedDrawToSoftwareDeviceSkipsUnsupportedLayers) {
  set_reduce_memory_result(false);
  EXPECT_TRUE(CreateHostImpl(DefaultSettings(),
                             FakeLayerTreeFrameSink::CreateSoftware()));

  const gfx::Transform external_transform;
  const gfx::Rect external_viewport;
  const bool resourceless_software_draw = true;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  // SolidColorLayerImpl will be drawn.
  auto* root = SetupRootLayer<SolidColorLayerImpl>(host_impl_->active_tree(),
                                                   gfx::Size(10, 10));
  root->SetDrawsContent(true);

  // VideoLayerImpl will not be drawn.
  FakeVideoFrameProvider provider;
  LayerImpl* video_layer = AddLayer<VideoLayerImpl>(
      host_impl_->active_tree(), &provider, media::VIDEO_ROTATION_0);
  video_layer->SetBounds(gfx::Size(10, 10));
  video_layer->SetDrawsContent(true);
  CopyProperties(root, video_layer);
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);

  EXPECT_EQ(1u, last_on_draw_frame_->will_draw_layers.size());
  EXPECT_EQ(host_impl_->active_tree()->root_layer(),
            last_on_draw_frame_->will_draw_layers[0]);
}

// Checks that we use the memory limits provided.
TEST_F(LayerTreeHostImplTest, MemoryLimits) {
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;

  const size_t kGpuByteLimit = 1234321;
  const size_t kGpuResourceLimit = 2345432;
  const gpu::MemoryAllocation::PriorityCutoff kGpuCutoff =
      gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING;

  const TileMemoryLimitPolicy kGpuTileCutoff =
      ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(kGpuCutoff);
  const TileMemoryLimitPolicy kNothingTileCutoff =
      ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(
          gpu::MemoryAllocation::CUTOFF_ALLOW_NOTHING);
  EXPECT_NE(kGpuTileCutoff, kNothingTileCutoff);

  LayerTreeSettings settings = DefaultSettings();
  settings.memory_policy =
      ManagedMemoryPolicy(kGpuByteLimit, kGpuCutoff, kGpuResourceLimit);
  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &task_runner_provider_, &stats_instrumentation_,
      &task_graph_runner_,
      AnimationHost::CreateForTesting(ThreadInstance::IMPL), 0, nullptr,
      nullptr);

  // Gpu compositing.
  layer_tree_frame_sink_ = FakeLayerTreeFrameSink::Create3d();
  host_impl_->SetVisible(true);
  host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(kGpuByteLimit, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
    EXPECT_EQ(kGpuTileCutoff, state.memory_limit_policy);
  }

  // Not visible, drops to 0.
  host_impl_->SetVisible(false);
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(0u, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
    EXPECT_EQ(kNothingTileCutoff, state.memory_limit_policy);
  }

  // Visible, is the gpu limit again.
  host_impl_->SetVisible(true);
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(kGpuByteLimit, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
  }

  // Software compositing.
  host_impl_->ReleaseLayerTreeFrameSink();
  layer_tree_frame_sink_ = FakeLayerTreeFrameSink::CreateSoftware();
  host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(kGpuByteLimit, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
    EXPECT_EQ(kGpuTileCutoff, state.memory_limit_policy);
  }

  // Not visible, drops to 0.
  host_impl_->SetVisible(false);
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(0u, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
    EXPECT_EQ(kNothingTileCutoff, state.memory_limit_policy);
  }

  // Visible, is the software limit again.
  host_impl_->SetVisible(true);
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(kGpuByteLimit, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
    EXPECT_EQ(kGpuTileCutoff, state.memory_limit_policy);
  }
}

namespace {
void ExpectFullDamageAndDraw(LayerTreeHostImpl* host_impl) {
  gfx::Rect full_frame_damage(
      host_impl->active_tree()->GetDeviceViewport().size());
  TestFrameData frame;
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  host_impl->WillBeginImplFrame(args);
  EXPECT_EQ(DRAW_SUCCESS, host_impl->PrepareToDraw(&frame));
  ASSERT_EQ(1u, frame.render_passes.size());
  const viz::RenderPass* root_render_pass = frame.render_passes.back().get();
  EXPECT_EQ(full_frame_damage, root_render_pass->damage_rect);
  EXPECT_TRUE(host_impl->DrawLayers(&frame));
  host_impl->DidDrawAllLayers(frame);
  host_impl->DidFinishImplFrame(args);
}
}  // namespace

TEST_F(LayerTreeHostImplTestDrawAndTestDamage,
       RequireHighResAndRedrawWhenVisible) {
  ASSERT_TRUE(host_impl_->active_tree());

  LayerImpl* root = SetupRootLayer<SolidColorLayerImpl>(
      host_impl_->active_tree(), gfx::Size(10, 10));
  root->SetBackgroundColor(SK_ColorRED);
  UpdateDrawProperties(host_impl_->active_tree());

  // RequiresHighResToDraw is set when new output surface is used.
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());

  // Expect full frame damage for first frame.
  EXPECT_SCOPED(ExpectFullDamageAndDraw(host_impl_.get()));

  host_impl_->ResetRequiresHighResToDraw();

  host_impl_->SetVisible(false);
  EXPECT_FALSE(host_impl_->RequiresHighResToDraw());
  host_impl_->SetVisible(true);
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());
  host_impl_->SetVisible(false);
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());

  host_impl_->ResetRequiresHighResToDraw();

  EXPECT_FALSE(host_impl_->RequiresHighResToDraw());
  did_request_redraw_ = false;
  host_impl_->SetVisible(true);
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());
  // Expect redraw and full frame damage when becoming visible.
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_SCOPED(ExpectFullDamageAndDraw(host_impl_.get()));
}

class LayerTreeHostImplTestPrepareTiles : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    fake_host_impl_ = new FakeLayerTreeHostImpl(
        LayerTreeSettings(), &task_runner_provider_, &task_graph_runner_);
    host_impl_.reset(fake_host_impl_);
    layer_tree_frame_sink_ = CreateLayerTreeFrameSink();
    host_impl_->SetVisible(true);
    host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
    host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(10, 10));
  }

  FakeLayerTreeHostImpl* fake_host_impl_;
};

TEST_F(LayerTreeHostImplTestPrepareTiles, PrepareTilesWhenInvisible) {
  EXPECT_TRUE(fake_host_impl_->prepare_tiles_needed());
  host_impl_->SetVisible(false);
  EXPECT_FALSE(fake_host_impl_->prepare_tiles_needed());
  host_impl_->SetVisible(true);
  EXPECT_TRUE(fake_host_impl_->prepare_tiles_needed());
}

TEST_F(LayerTreeHostImplTest, UIResourceManagement) {
  auto test_context_provider = viz::TestContextProvider::Create();
  viz::TestSharedImageInterface* sii =
      test_context_provider->SharedImageInterface();
  CreateHostImpl(DefaultSettings(), FakeLayerTreeFrameSink::Create3d(
                                        std::move(test_context_provider)));

  EXPECT_EQ(0u, sii->shared_image_count());

  UIResourceId ui_resource_id = 1;
  bool is_opaque = false;
  UIResourceBitmap bitmap(gfx::Size(1, 1), is_opaque);
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, sii->shared_image_count());
  viz::ResourceId id1 = host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(0u, id1);

  // Multiple requests with the same id is allowed.  The previous texture is
  // deleted.
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, sii->shared_image_count());
  viz::ResourceId id2 = host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(0u, id2);
  EXPECT_NE(id1, id2);

  // Deleting invalid UIResourceId is allowed and does not change state.
  host_impl_->DeleteUIResource(-1);
  EXPECT_EQ(1u, sii->shared_image_count());

  // Should return zero for invalid UIResourceId.  Number of textures should
  // not change.
  EXPECT_EQ(0u, host_impl_->ResourceIdForUIResource(-1));
  EXPECT_EQ(1u, sii->shared_image_count());

  host_impl_->DeleteUIResource(ui_resource_id);
  EXPECT_EQ(0u, host_impl_->ResourceIdForUIResource(ui_resource_id));
  EXPECT_EQ(0u, sii->shared_image_count());

  // Should not change state for multiple deletion on one UIResourceId
  host_impl_->DeleteUIResource(ui_resource_id);
  EXPECT_EQ(0u, sii->shared_image_count());
}

TEST_F(LayerTreeHostImplTest, CreateETC1UIResource) {
  auto test_context_provider = viz::TestContextProvider::Create();
  viz::TestSharedImageInterface* sii =
      test_context_provider->SharedImageInterface();
  CreateHostImpl(DefaultSettings(), FakeLayerTreeFrameSink::Create3d(
                                        std::move(test_context_provider)));

  EXPECT_EQ(0u, sii->shared_image_count());

  gfx::Size size(4, 4);
  // SkImageInfo has no support for ETC1.  The |info| below contains the right
  // total pixel size for the bitmap but not the right height and width.  The
  // correct width/height are passed directly to UIResourceBitmap.
  SkImageInfo info =
      SkImageInfo::Make(4, 2, kAlpha_8_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkPixelRef> pixel_ref(SkMallocPixelRef::MakeAllocate(info, 0));
  pixel_ref->setImmutable();
  UIResourceBitmap bitmap(std::move(pixel_ref), size);
  UIResourceId ui_resource_id = 1;
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, sii->shared_image_count());
  viz::ResourceId id1 = host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(0u, id1);
}

TEST_F(LayerTreeHostImplTest, ObeyMSAACaps) {
  LayerTreeSettings msaaSettings = DefaultSettings();
  msaaSettings.gpu_rasterization_msaa_sample_count = 4;

  // gpu raster, msaa on
  {
    bool msaa_is_slow = false;
    EXPECT_TRUE(CreateHostImpl(
        msaaSettings,
        FakeLayerTreeFrameSink::Create3dForGpuRasterization(
            msaaSettings.gpu_rasterization_msaa_sample_count, msaa_is_slow)));
    EXPECT_TRUE(host_impl_->can_use_msaa());
  }

  // gpu raster, msaa off
  {
    bool msaa_is_slow = true;
    EXPECT_TRUE(CreateHostImpl(
        msaaSettings,
        FakeLayerTreeFrameSink::Create3dForGpuRasterization(
            msaaSettings.gpu_rasterization_msaa_sample_count, msaa_is_slow)));
    EXPECT_FALSE(host_impl_->can_use_msaa());
  }

  // oop raster, msaa on
  {
    bool msaa_is_slow = false;
    EXPECT_TRUE(CreateHostImpl(
        msaaSettings,
        FakeLayerTreeFrameSink::Create3dForOopRasterization(
            msaaSettings.gpu_rasterization_msaa_sample_count, msaa_is_slow)));
    EXPECT_TRUE(host_impl_->can_use_msaa());
  }

  // oop raster, msaa off
  {
    bool msaa_is_slow = true;
    EXPECT_TRUE(CreateHostImpl(
        msaaSettings,
        FakeLayerTreeFrameSink::Create3dForOopRasterization(
            msaaSettings.gpu_rasterization_msaa_sample_count, msaa_is_slow)));
    EXPECT_FALSE(host_impl_->can_use_msaa());
  }
}

class FrameSinkClient : public TestLayerTreeFrameSinkClient {
 public:
  explicit FrameSinkClient(
      scoped_refptr<viz::ContextProvider> display_context_provider)
      : display_context_provider_(std::move(display_context_provider)) {}

  std::unique_ptr<viz::SkiaOutputSurface> CreateDisplaySkiaOutputSurface()
      override {
    return viz::FakeSkiaOutputSurface::Create3d(
        std::move(display_context_provider_));
  }

  std::unique_ptr<viz::OutputSurface> CreateDisplayOutputSurface(
      scoped_refptr<viz::ContextProvider> compositor_context_provider)
      override {
    return viz::FakeOutputSurface::Create3d(
        std::move(display_context_provider_));
  }

  void DisplayReceivedLocalSurfaceId(
      const viz::LocalSurfaceId& local_surface_id) override {}
  void DisplayReceivedCompositorFrame(
      const viz::CompositorFrame& frame) override {}
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              viz::RenderPassList* render_passes) override {}
  void DisplayDidDrawAndSwap() override {}

 private:
  scoped_refptr<viz::ContextProvider> display_context_provider_;
};

enum RendererType { RENDERER_GL, RENDERER_SKIA };

class LayerTreeHostImplTestWithRenderer
    : public LayerTreeHostImplTest,
      public ::testing::WithParamInterface<RendererType> {
 protected:
  RendererType renderer_type() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostImplTestWithRenderer,
                         ::testing::Values(RENDERER_GL, RENDERER_SKIA));

TEST_P(LayerTreeHostImplTestWithRenderer, ShutdownReleasesContext) {
  scoped_refptr<viz::TestContextProvider> context_provider =
      viz::TestContextProvider::Create();
  FrameSinkClient test_client(context_provider);

  constexpr bool synchronous_composite = true;
  constexpr bool disable_display_vsync = false;
  constexpr double refresh_rate = 60.0;
  viz::RendererSettings renderer_settings = viz::RendererSettings();
  renderer_settings.use_skia_renderer = renderer_type() == RENDERER_SKIA;
  auto layer_tree_frame_sink = std::make_unique<TestLayerTreeFrameSink>(
      context_provider, viz::TestContextProvider::CreateWorker(), nullptr,
      renderer_settings, base::ThreadTaskRunnerHandle::Get().get(),
      synchronous_composite, disable_display_vsync, refresh_rate);
  layer_tree_frame_sink->SetClient(&test_client);

  CreateHostImpl(DefaultSettings(), std::move(layer_tree_frame_sink));

  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(10, 10));
  struct Helper {
    std::unique_ptr<viz::CopyOutputResult> unprocessed_result;
    void OnResult(std::unique_ptr<viz::CopyOutputResult> result) {
      unprocessed_result = std::move(result);
    }
  } helper;

  GetEffectNode(root)->has_copy_request = true;
  GetPropertyTrees(root)->effect_tree.AddCopyRequest(
      root->effect_tree_index(),
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
          base::BindOnce(&Helper::OnResult, base::Unretained(&helper))));
  DrawFrame();

  auto* sii = context_provider->SharedImageInterface();
  // The CopyOutputResult has a ref on the viz::ContextProvider and a shared
  // image allocated.
  EXPECT_TRUE(helper.unprocessed_result);
  EXPECT_FALSE(context_provider->HasOneRef());
  EXPECT_EQ(1u, sii->shared_image_count());

  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;

  // The texture release callback that was given to the CopyOutputResult has
  // been canceled, and the shared image deleted.
  EXPECT_TRUE(context_provider->HasOneRef());
  EXPECT_EQ(0u, sii->shared_image_count());

  // When resetting the CopyOutputResult, it will run its texture release
  // callback. This should not cause a crash, etc.
  helper.unprocessed_result.reset();
}

TEST_F(LayerTreeHostImplTest, ScrollUnknownNotOnAncestorChain) {
  // If we ray cast a scroller that is not on the first layer's ancestor chain,
  // we should return SCROLL_UNKNOWN.
  gfx::Size viewport_size(50, 50);
  gfx::Size content_size(100, 100);
  SetupViewportLayersInnerScrolls(viewport_size, content_size);

  LayerImpl* occluder_layer = AddLayer();
  occluder_layer->SetDrawsContent(true);
  occluder_layer->SetHitTestable(true);
  occluder_layer->SetBounds(content_size);

  // The parent of the occluder is *above* the scroller.
  CopyProperties(root_layer(), occluder_layer);
  occluder_layer->SetTransformTreeIndex(
      host_impl_->active_tree()->PageScaleTransformNode()->id);

  DrawFrame();

  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollUnknownScrollAncestorMismatch) {
  // If we ray cast a scroller this is on the first layer's ancestor chain, but
  // is not the first scroller we encounter when walking up from the layer, we
  // should also return SCROLL_UNKNOWN.
  gfx::Size viewport_size(50, 50);
  gfx::Size content_size(100, 100);
  SetupViewportLayersInnerScrolls(viewport_size, content_size);

  LayerImpl* scroll_layer = InnerViewportScrollLayer();

  LayerImpl* child_scroll_clip = AddLayer();
  CopyProperties(scroll_layer, child_scroll_clip);

  LayerImpl* child_scroll =
      AddScrollableLayer(child_scroll_clip, viewport_size, content_size);
  child_scroll->SetOffsetToTransformParent(gfx::Vector2dF(10, 10));

  LayerImpl* occluder_layer = AddLayer();
  occluder_layer->SetDrawsContent(true);
  occluder_layer->SetHitTestable(true);
  occluder_layer->SetBounds(content_size);
  CopyProperties(child_scroll, occluder_layer);
  occluder_layer->SetOffsetToTransformParent(gfx::Vector2dF(-10, -10));

  DrawFrame();

  InputHandler::ScrollStatus status =
      host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel);
  EXPECT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollInvisibleScroller) {
  gfx::Size viewport_size(50, 50);
  gfx::Size content_size(100, 100);
  SetupViewportLayersInnerScrolls(viewport_size, content_size);

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  LayerImpl* child_scroll =
      AddScrollableLayer(scroll_layer, viewport_size, content_size);
  child_scroll->SetDrawsContent(false);

  DrawFrame();

  // We should have scrolled |child_scroll| even though it does not move
  // any layer that is a drawn RSLL member.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);

  EXPECT_EQ(child_scroll->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
}

// Make sure LatencyInfo carried by LatencyInfoSwapPromise are passed
// in viz::CompositorFrameMetadata.
TEST_F(LayerTreeHostImplTest, LatencyInfoPassedToCompositorFrameMetadata) {
  LayerTreeSettings settings = DefaultSettings();
  settings.commit_to_active_tree = false;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  SetupRootLayer<SolidColorLayerImpl>(host_impl_->active_tree(),
                                      gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());

  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(5);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
  std::unique_ptr<SwapPromise> swap_promise(
      new LatencyInfoSwapPromise(latency_info));
  host_impl_->active_tree()->QueuePinnedSwapPromise(std::move(swap_promise));

  host_impl_->SetFullViewportDamage();
  host_impl_->SetNeedsRedraw();
  DrawFrame();

  const auto& metadata_latency_after =
      fake_layer_tree_frame_sink->last_sent_frame()->metadata.latency_info;
  EXPECT_EQ(1u, metadata_latency_after.size());
  EXPECT_TRUE(metadata_latency_after[0].FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
}

#if defined(OS_ANDROID)
TEST_F(LayerTreeHostImplTest, SelectionBoundsPassedToCompositorFrameMetadata) {
  LayerImpl* root = SetupRootLayer<SolidColorLayerImpl>(
      host_impl_->active_tree(), gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

  // Plumb the layer-local selection bounds.
  gfx::Point selection_start(5, 0);
  gfx::Point selection_end(5, 5);
  LayerSelection selection;
  selection.start.type = gfx::SelectionBound::CENTER;
  selection.start.layer_id = root->id();
  selection.start.edge_end = selection_end;
  selection.start.edge_start = selection_start;
  selection.end = selection.start;
  host_impl_->active_tree()->RegisterSelection(selection);

  host_impl_->SetNeedsRedraw();
  RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();

  // Ensure the selection bounds have propagated to the frame metadata.
  const viz::Selection<gfx::SelectionBound>& selection_after =
      metadata.selection;
  EXPECT_EQ(selection.start.type, selection_after.start.type());
  EXPECT_EQ(selection.end.type, selection_after.end.type());
  EXPECT_EQ(gfx::PointF(selection_end), selection_after.start.edge_end());
  EXPECT_EQ(gfx::PointF(selection_start), selection_after.start.edge_start());
  EXPECT_TRUE(selection_after.start.visible());
  EXPECT_TRUE(selection_after.end.visible());
}

TEST_F(LayerTreeHostImplTest, HiddenSelectionBoundsStayHidden) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(10, 10));

  UpdateDrawProperties(host_impl_->active_tree());

  // Plumb the layer-local selection bounds.
  gfx::Point selection_start(5, 0);
  gfx::Point selection_end(5, 5);
  LayerSelection selection;

  // Mark the start as hidden.
  selection.start.hidden = true;

  selection.start.type = gfx::SelectionBound::CENTER;
  selection.start.layer_id = root->id();
  selection.start.edge_end = selection_end;
  selection.start.edge_start = selection_start;
  selection.end = selection.start;
  host_impl_->active_tree()->RegisterSelection(selection);

  host_impl_->SetNeedsRedraw();
  RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();

  // Ensure the selection bounds have propagated to the frame metadata.
  const viz::Selection<gfx::SelectionBound>& selection_after =
      metadata.selection;
  EXPECT_EQ(selection.start.type, selection_after.start.type());
  EXPECT_EQ(selection.end.type, selection_after.end.type());
  EXPECT_EQ(gfx::PointF(selection_end), selection_after.start.edge_end());
  EXPECT_EQ(gfx::PointF(selection_start), selection_after.start.edge_start());
  EXPECT_FALSE(selection_after.start.visible());
  EXPECT_FALSE(selection_after.end.visible());
}
#endif  // defined(OS_ANDROID)

class SimpleSwapPromiseMonitor : public SwapPromiseMonitor {
 public:
  SimpleSwapPromiseMonitor(LayerTreeHost* layer_tree_host,
                           LayerTreeHostImpl* layer_tree_host_impl,
                           int* set_needs_commit_count,
                           int* set_needs_redraw_count)
      : SwapPromiseMonitor(
            (layer_tree_host ? layer_tree_host->GetSwapPromiseManager()
                             : nullptr),
            layer_tree_host_impl),
        set_needs_commit_count_(set_needs_commit_count),
        set_needs_redraw_count_(set_needs_redraw_count) {}

  ~SimpleSwapPromiseMonitor() override = default;

  void OnSetNeedsCommitOnMain() override { (*set_needs_commit_count_)++; }

  void OnSetNeedsRedrawOnImpl() override { (*set_needs_redraw_count_)++; }

 private:
  int* set_needs_commit_count_;
  int* set_needs_redraw_count_;
};

TEST_F(LayerTreeHostImplTest, SimpleSwapPromiseMonitor) {
  int set_needs_commit_count = 0;
  int set_needs_redraw_count = 0;

  {
    std::unique_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
        new SimpleSwapPromiseMonitor(nullptr, host_impl_.get(),
                                     &set_needs_commit_count,
                                     &set_needs_redraw_count));
    host_impl_->SetNeedsRedraw();
    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(1, set_needs_redraw_count);
  }

  // Now the monitor is destroyed, SetNeedsRedraw() is no longer being
  // monitored.
  host_impl_->SetNeedsRedraw();
  EXPECT_EQ(0, set_needs_commit_count);
  EXPECT_EQ(1, set_needs_redraw_count);

  {
    std::unique_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
        new SimpleSwapPromiseMonitor(nullptr, host_impl_.get(),
                                     &set_needs_commit_count,
                                     &set_needs_redraw_count));
    // Redraw with damage.
    host_impl_->SetFullViewportDamage();
    host_impl_->SetNeedsRedraw();
    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(2, set_needs_redraw_count);
  }

  {
    std::unique_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
        new SimpleSwapPromiseMonitor(nullptr, host_impl_.get(),
                                     &set_needs_commit_count,
                                     &set_needs_redraw_count));
    // Redraw without damage.
    host_impl_->SetNeedsRedraw();
    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(3, set_needs_redraw_count);
  }

  set_needs_commit_count = 0;
  set_needs_redraw_count = 0;

  {
    std::unique_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
        new SimpleSwapPromiseMonitor(nullptr, host_impl_.get(),
                                     &set_needs_commit_count,
                                     &set_needs_redraw_count));
    SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

    // Scrolling normally should not trigger any forwarding.
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_TRUE(
        host_impl_
            ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kTouchscreen)
                               .get())
            .did_scroll);
    host_impl_->ScrollEnd();

    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(1, set_needs_redraw_count);

    // Scrolling with a scroll handler should defer the swap to the main
    // thread.
    host_impl_->active_tree()->set_have_scroll_event_handlers(true);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_TRUE(
        host_impl_
            ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kTouchscreen)
                               .get())
            .did_scroll);
    host_impl_->ScrollEnd();

    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(2, set_needs_redraw_count);
  }
}

class LayerTreeHostImplWithBrowserControlsTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    LayerTreeSettings settings = DefaultSettings();
    settings.commit_to_active_tree = false;
    CreateHostImpl(settings, CreateLayerTreeFrameSink());
    host_impl_->active_tree()->SetBrowserControlsParams(
        {top_controls_height_, 0, 0, 0, false, false});
    host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1.f, 1.f);
  }

 protected:
  static const int top_controls_height_;
};

const int LayerTreeHostImplWithBrowserControlsTest::top_controls_height_ = 50;

TEST_F(LayerTreeHostImplWithBrowserControlsTest, NoIdleAnimations) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset(0, 10));
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_request_redraw_);
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsHeightIsCommitted) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  EXPECT_FALSE(did_request_redraw_);
  CreatePendingTree();
  host_impl_->sync_tree()->SetBrowserControlsParams(
      {100, 0, 0, 0, false, false});
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(100, host_impl_->browser_controls_manager()->TopControlsHeight());
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsStayFullyVisibleOnHeightChange) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());

  CreatePendingTree();
  host_impl_->sync_tree()->SetBrowserControlsParams({0, 0, 0, 0, false, false});
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());

  CreatePendingTree();
  host_impl_->sync_tree()->SetBrowserControlsParams(
      {50, 0, 0, 0, false, false});
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsAnimationScheduling) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset(0, 10));
  host_impl_->DidChangeBrowserControlsPosition();
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       ScrollHandledByBrowserControls) {
  InputHandlerScrollResult result;
  SetupViewportLayersInnerScrolls(gfx::Size(50, 100), gfx::Size(100, 200));
  auto* scroll_layer = InnerViewportScrollLayer();
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false);
  DrawFrame();

  // First, scroll just the browser controls and verify that the scroll
  // succeeds.
  const float residue = 10;
  float offset = top_controls_height_ - residue;
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, offset),
                                   ui::ScrollInputType::kTouchscreen)
                            .get(),
                        ui::ScrollInputType::kTouchscreen)
          .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  result = host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                  ui::ScrollInputType::kTouchscreen)
          .get());
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_TRUE(result.did_scroll);
  EXPECT_FLOAT_EQ(-offset,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  // Scroll across the boundary
  const float content_scroll = 20;
  offset = residue + content_scroll;
  result = host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                  ui::ScrollInputType::kTouchscreen)
          .get());
  EXPECT_TRUE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_EQ(-top_controls_height_,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF(0, content_scroll).ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  // Now scroll back to the top of the content
  offset = -content_scroll;
  result = host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                  ui::ScrollInputType::kTouchscreen)
          .get());
  EXPECT_TRUE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_EQ(-top_controls_height_,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  // And scroll the browser controls completely into view
  offset = -top_controls_height_;
  result = host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                  ui::ScrollInputType::kTouchscreen)
          .get());
  EXPECT_TRUE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  // And attempt to scroll past the end
  result = host_impl_->ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                  ui::ScrollInputType::kTouchscreen)
          .get());
  EXPECT_FALSE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, -50));
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  host_impl_->ScrollEnd();
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       WheelUnhandledByBrowserControls) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 100), gfx::Size(100, 200));
  host_impl_->active_tree()->SetBrowserControlsParams(
      {top_controls_height_, 0, 0, 0, false, true});
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false);
  DrawFrame();

  LayerImpl* viewport_layer = InnerViewportScrollLayer();

  const float delta = top_controls_height_;
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, delta),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(viewport_layer));

  // Wheel scrolls should not affect the browser controls, and should pass
  // directly through to the viewport.
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, delta),
                                     ui::ScrollInputType::kWheel)
                             .get())
          .did_scroll);
  EXPECT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, delta),
                   CurrentScrollOffset(viewport_layer));

  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, delta),
                                     ui::ScrollInputType::kWheel)
                             .get())
          .did_scroll);
  EXPECT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, delta * 2),
                   CurrentScrollOffset(viewport_layer));
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsAnimationAtOrigin) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 100), gfx::Size(100, 200));
  auto* scroll_layer = InnerViewportScrollLayer();
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false);
  DrawFrame();

  const float residue = 35;
  float offset = top_controls_height_ - residue;
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, offset),
                                   ui::ScrollInputType::kTouchscreen)
                            .get(),
                        ui::ScrollInputType::kTouchscreen)
          .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  // Scroll the browser controls partially.
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_FLOAT_EQ(-offset,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  did_request_commit_ = false;

  // End the scroll while the controls are still offset from their limit.
  host_impl_->ScrollEnd();
  ASSERT_TRUE(host_impl_->browser_controls_manager()->HasAnimation());
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);

  // The browser controls should properly animate until finished, despite the
  // scroll offset being at the origin.
  viz::BeginFrameArgs begin_frame_args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 1, base::TimeTicks::Now());
  while (did_request_next_frame_) {
    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;

    float old_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    begin_frame_args.frame_time += base::TimeDelta::FromMilliseconds(5);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_EQ(gfx::Vector2dF().ToString(),
              CurrentScrollOffset(scroll_layer).ToString());

    float new_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    // No commit is needed as the controls are animating the content offset,
    // not the scroll offset.
    EXPECT_FALSE(did_request_commit_);

    if (new_offset != old_offset)
      EXPECT_TRUE(did_request_redraw_);

    if (new_offset != 0) {
      EXPECT_TRUE(host_impl_->browser_controls_manager()->HasAnimation());
      EXPECT_TRUE(did_request_next_frame_);
    }
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }
  EXPECT_FALSE(host_impl_->browser_controls_manager()->HasAnimation());
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsAnimationAfterScroll) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 100), gfx::Size(100, 200));
  auto* scroll_layer = InnerViewportScrollLayer();
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false);
  float initial_scroll_offset = 50;
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          scroll_layer->element_id(),
          gfx::ScrollOffset(0, initial_scroll_offset));
  DrawFrame();

  const float residue = 15;
  float offset = top_controls_height_ - residue;
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, offset),
                                   ui::ScrollInputType::kTouchscreen)
                            .get(),
                        ui::ScrollInputType::kTouchscreen)
          .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF(0, initial_scroll_offset).ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  // Scroll the browser controls partially.
  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_FLOAT_EQ(-offset,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF(0, initial_scroll_offset).ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  did_request_commit_ = false;

  // End the scroll while the controls are still offset from the limit.
  host_impl_->ScrollEnd();
  ASSERT_TRUE(host_impl_->browser_controls_manager()->HasAnimation());
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);

  // Animate the browser controls to the limit.
  viz::BeginFrameArgs begin_frame_args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 1, base::TimeTicks::Now());
  while (did_request_next_frame_) {
    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;

    float old_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    begin_frame_args.frame_time += base::TimeDelta::FromMilliseconds(5);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();

    float new_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    if (new_offset != old_offset) {
      EXPECT_TRUE(did_request_redraw_);
      EXPECT_TRUE(did_request_commit_);
    }
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }
  EXPECT_FALSE(host_impl_->browser_controls_manager()->HasAnimation());
  EXPECT_EQ(-top_controls_height_,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsScrollDeltaInOverScroll) {
  // Verifies that the overscroll delta should not have accumulated in
  // the browser controls if we do a hide and show without releasing finger.
  SetupViewportLayersInnerScrolls(gfx::Size(50, 100), gfx::Size(100, 200));
  auto* scroll_layer = InnerViewportScrollLayer();
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false);
  DrawFrame();

  float offset = 50;
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, offset),
                                   ui::ScrollInputType::kTouchscreen)
                            .get(),
                        ui::ScrollInputType::kTouchscreen)
          .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());

  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_EQ(-offset,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_EQ(gfx::Vector2dF(0, offset).ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);

  // Should have fully scrolled
  EXPECT_EQ(gfx::Vector2dF(0, MaxScrollOffset(scroll_layer).y()).ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  float overscrollamount = 10;

  // Overscroll the content
  EXPECT_FALSE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(),
                                     gfx::Vector2d(0, overscrollamount),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_EQ(gfx::Vector2dF(0, 2 * offset).ToString(),
            CurrentScrollOffset(scroll_layer).ToString());
  EXPECT_EQ(gfx::Vector2dF(0, overscrollamount).ToString(),
            host_impl_->accumulated_root_overscroll().ToString());

  EXPECT_TRUE(host_impl_
                  ->ScrollUpdate(UpdateState(gfx::Point(),
                                             gfx::Vector2d(0, -2 * offset),
                                             ui::ScrollInputType::kTouchscreen)
                                     .get())
                  .did_scroll);
  EXPECT_EQ(gfx::Vector2dF(0, 0).ToString(),
            CurrentScrollOffset(scroll_layer).ToString());
  EXPECT_EQ(-offset,
            host_impl_->browser_controls_manager()->ControlsTopOffset());

  EXPECT_TRUE(
      host_impl_
          ->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -offset),
                                     ui::ScrollInputType::kTouchscreen)
                             .get())
          .did_scroll);
  EXPECT_EQ(gfx::Vector2dF(0, 0).ToString(),
            CurrentScrollOffset(scroll_layer).ToString());

  // Browser controls should be fully visible
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());

  host_impl_->ScrollEnd();
}

// Tests that when we set a child scroller (e.g. a scrolling div) as the outer
// viewport, scrolling it controls the browser controls.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       ReplacedOuterViewportScrollsBrowserControls) {
  const gfx::Size scroll_content_size(400, 400);
  const gfx::Size root_layer_size(200, 200);
  const gfx::Size viewport_size(100, 100);

  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      viewport_size, viewport_size, root_layer_size);

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  LayerImpl* outer_scroll = OuterViewportScrollLayer();

  // Initialization: Add a child scrolling layer to the outer scroll layer and
  // set its scroll layer as the outer viewport. This simulates setting a
  // scrolling element as the root scroller on the page.
  LayerImpl* clip_layer = AddLayer();
  clip_layer->SetBounds(root_layer_size);
  CopyProperties(outer_scroll, clip_layer);
  CreateClipNode(clip_layer);
  LayerImpl* scroll_layer =
      AddScrollableLayer(clip_layer, root_layer_size, scroll_content_size);
  GetScrollNode(scroll_layer)->scrolls_outer_viewport = true;

  auto viewport_property_ids = layer_tree_impl->ViewportPropertyIdsForTesting();
  viewport_property_ids.outer_clip = clip_layer->clip_tree_index();
  viewport_property_ids.outer_scroll = scroll_layer->scroll_tree_index();
  layer_tree_impl->SetViewportPropertyIds(viewport_property_ids);
  DrawFrame();

  ASSERT_EQ(1, layer_tree_impl->CurrentTopControlsShownRatio());

  // Scrolling should scroll the child content and the browser controls. The
  // original outer viewport should get no scroll.
  {
    host_impl_->ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(0, 0),
                                         gfx::Vector2dF(100, 100),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(100, 50),
                     CurrentScrollOffset(scroll_layer));
    EXPECT_EQ(0, layer_tree_impl->CurrentTopControlsShownRatio());
  }
}

using LayerTreeHostImplVirtualViewportTest = LayerTreeHostImplTest;

TEST_F(LayerTreeHostImplVirtualViewportTest, RootScrollBothInnerAndOuterLayer) {
  gfx::Size content_size = gfx::Size(100, 160);
  gfx::Size outer_viewport = gfx::Size(50, 80);
  gfx::Size inner_viewport = gfx::Size(25, 40);

  SetupViewportLayers(host_impl_->active_tree(), inner_viewport, outer_viewport,
                      content_size);

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();

  DrawFrame();
  {
    gfx::ScrollOffset inner_expected;
    gfx::ScrollOffset outer_expected;
    EXPECT_EQ(inner_expected, CurrentScrollOffset(inner_scroll));
    EXPECT_EQ(outer_expected, CurrentScrollOffset(outer_scroll));

    gfx::ScrollOffset current_offset(70, 100);

    host_impl_->SetSynchronousInputHandlerRootScrollOffset(current_offset);
    EXPECT_EQ(gfx::ScrollOffset(25, 40), MaxScrollOffset(inner_scroll));
    EXPECT_EQ(gfx::ScrollOffset(50, 80), MaxScrollOffset(outer_scroll));

    // Inner viewport scrolls first. Then the rest is applied to the outer
    // viewport.
    EXPECT_EQ(gfx::ScrollOffset(25, 40), CurrentScrollOffset(inner_scroll));
    EXPECT_EQ(gfx::ScrollOffset(45, 60), CurrentScrollOffset(outer_scroll));
  }
}

TEST_F(LayerTreeHostImplVirtualViewportTest,
       DiagonalScrollBubblesPerfectlyToInner) {
  gfx::Size content_size = gfx::Size(200, 320);
  gfx::Size outer_viewport = gfx::Size(100, 160);
  gfx::Size inner_viewport = gfx::Size(50, 80);

  SetupViewportLayers(host_impl_->active_tree(), inner_viewport, outer_viewport,
                      content_size);

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();

  DrawFrame();
  {
    gfx::Vector2dF inner_expected;
    gfx::Vector2dF outer_expected;
    EXPECT_VECTOR_EQ(inner_expected, CurrentScrollOffset(inner_scroll));
    EXPECT_VECTOR_EQ(outer_expected, CurrentScrollOffset(outer_scroll));

    gfx::Vector2d scroll_delta(inner_viewport.width() / 2,
                               inner_viewport.height() / 2);

    // Make sure the scroll goes to the inner viewport first.
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen)
                  .thread);

    // Scroll near the edge of the outer viewport.
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(), scroll_delta,
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    inner_expected += scroll_delta;

    EXPECT_VECTOR_EQ(inner_expected, CurrentScrollOffset(inner_scroll));
    EXPECT_VECTOR_EQ(outer_expected, CurrentScrollOffset(outer_scroll));

    // Now diagonal scroll across the outer viewport boundary in a single event.
    // The entirety of the scroll should be consumed, as bubbling between inner
    // and outer viewport layers is perfect.
    host_impl_->ScrollUpdate(UpdateState(gfx::Point(),
                                         gfx::ScaleVector2d(scroll_delta, 2),
                                         ui::ScrollInputType::kTouchscreen)
                                 .get());
    outer_expected += scroll_delta;
    inner_expected += scroll_delta;
    host_impl_->ScrollEnd();

    EXPECT_VECTOR_EQ(inner_expected, CurrentScrollOffset(inner_scroll));
    EXPECT_VECTOR_EQ(outer_expected, CurrentScrollOffset(outer_scroll));
  }
}

TEST_F(LayerTreeHostImplVirtualViewportTest,
       ScrollBeginEventThatTargetsViewportLayerSkipsHitTest) {
  gfx::Size content_size = gfx::Size(100, 160);
  gfx::Size outer_viewport = gfx::Size(50, 80);
  gfx::Size inner_viewport = gfx::Size(25, 40);

  SetupViewportLayers(host_impl_->active_tree(), inner_viewport, outer_viewport,
                      content_size);

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* child_scroll =
      AddScrollableLayer(outer_scroll, inner_viewport, outer_viewport);

  UpdateDrawProperties(host_impl_->active_tree());
  DrawFrame();

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->RootScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen)
          .thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode(),
            host_impl_->OuterViewportScrollNode());
  host_impl_->ScrollEnd();
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
            child_scroll->scroll_tree_index());
  host_impl_->ScrollEnd();
}

TEST_F(LayerTreeHostImplVirtualViewportTest,
       NoOverscrollWhenInnerViewportCantScroll) {
  InputHandlerScrollResult scroll_result;
  gfx::Size content_size = gfx::Size(100, 160);
  gfx::Size outer_viewport = gfx::Size(50, 80);
  gfx::Size inner_viewport = gfx::Size(25, 40);
  SetupViewportLayers(host_impl_->active_tree(), inner_viewport, outer_viewport,
                      content_size);
  // Make inner viewport unscrollable.
  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  GetScrollNode(inner_scroll)->user_scrollable_horizontal = false;
  GetScrollNode(inner_scroll)->user_scrollable_vertical = false;

  DrawFrame();

  // Ensure inner viewport doesn't react to scrolls (test it's unscrollable).
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(inner_scroll));
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 100),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);
  scroll_result =
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0, 100),
                                           ui::ScrollInputType::kTouchscreen)
                                   .get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(inner_scroll));

  // When inner viewport is unscrollable, a fling gives zero overscroll.
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
}

class LayerTreeHostImplWithImplicitLimitsTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    LayerTreeSettings settings = DefaultSettings();
    settings.max_memory_for_prepaint_percentage = 50;
    CreateHostImpl(settings, CreateLayerTreeFrameSink());
  }
};

TEST_F(LayerTreeHostImplWithImplicitLimitsTest, ImplicitMemoryLimits) {
  // Set up a memory policy and percentages which could cause
  // 32-bit integer overflows.
  ManagedMemoryPolicy mem_policy(300 * 1024 * 1024);  // 300MB

  // Verify implicit limits are calculated correctly with no overflows
  host_impl_->SetMemoryPolicy(mem_policy);
  EXPECT_EQ(host_impl_->global_tile_state().hard_memory_limit_in_bytes,
            300u * 1024u * 1024u);
  EXPECT_EQ(host_impl_->global_tile_state().soft_memory_limit_in_bytes,
            150u * 1024u * 1024u);
}

TEST_F(LayerTreeHostImplTest, ExternalTransformReflectedInNextDraw) {
  const gfx::Size viewport_size(50, 50);
  const gfx::Size layer_size(100, 100);
  gfx::Transform external_transform;
  const gfx::Rect external_viewport(layer_size);
  const bool resourceless_software_draw = false;
  SetupViewportLayersInnerScrolls(viewport_size, layer_size);
  auto* layer = InnerViewportScrollLayer();
  layer->SetDrawsContent(true);

  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      external_transform, layer->draw_properties().target_space_transform);

  external_transform.Translate(20, 20);
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      external_transform, layer->draw_properties().target_space_transform);
}

TEST_F(LayerTreeHostImplTest, ExternalTransformSetNeedsRedraw) {
  const gfx::Size viewport_size(100, 100);
  SetupDefaultRootLayer(viewport_size);
  UpdateDrawProperties(host_impl_->active_tree());

  const gfx::Transform transform_for_tile_priority;
  const gfx::Transform draw_transform;
  const gfx::Rect viewport_for_tile_priority1(viewport_size);
  const gfx::Rect viewport_for_tile_priority2(50, 50);
  const gfx::Rect draw_viewport(viewport_size);
  bool resourceless_software_draw = false;

  // Clear any damage.
  host_impl_->SetExternalTilePriorityConstraints(viewport_for_tile_priority1,
                                                 transform_for_tile_priority);
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  last_on_draw_frame_.reset();

  // Setting new constraints needs redraw.
  did_request_redraw_ = false;
  host_impl_->SetExternalTilePriorityConstraints(viewport_for_tile_priority2,
                                                 transform_for_tile_priority);
  EXPECT_TRUE(did_request_redraw_);
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_FALSE(last_on_draw_frame_->has_no_damage);
}

TEST_F(LayerTreeHostImplTest, OnMemoryPressure) {
  gfx::Size size(200, 200);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  ResourcePool::InUsePoolResource resource =
      host_impl_->resource_pool()->AcquireResource(size, format, color_space);
  host_impl_->resource_pool()->ReleaseResource(std::move(resource));

  size_t current_memory_usage =
      host_impl_->resource_pool()->GetTotalMemoryUsageForTesting();

  base::MemoryPressureListener::SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();

  size_t memory_usage_after_memory_pressure =
      host_impl_->resource_pool()->GetTotalMemoryUsageForTesting();

  // Memory usage after the memory pressure should be less than previous one.
  EXPECT_LT(memory_usage_after_memory_pressure, current_memory_usage);
}

TEST_F(LayerTreeHostImplTest, OnDrawConstraintSetNeedsRedraw) {
  const gfx::Size viewport_size(100, 100);
  SetupDefaultRootLayer(viewport_size);
  UpdateDrawProperties(host_impl_->active_tree());

  const gfx::Transform draw_transform;
  const gfx::Rect draw_viewport1(viewport_size);
  const gfx::Rect draw_viewport2(50, 50);
  bool resourceless_software_draw = false;

  // Clear any damage.
  host_impl_->OnDraw(draw_transform, draw_viewport1, resourceless_software_draw,
                     false);
  last_on_draw_frame_.reset();

  // Same draw params does not swap.
  did_request_redraw_ = false;
  host_impl_->OnDraw(draw_transform, draw_viewport1, resourceless_software_draw,
                     false);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_TRUE(last_on_draw_frame_->has_no_damage);
  last_on_draw_frame_.reset();

  // Different draw params does swap.
  did_request_redraw_ = false;
  host_impl_->OnDraw(draw_transform, draw_viewport2, resourceless_software_draw,
                     false);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(last_on_draw_frame_->has_no_damage);
}

// This test verifies that the viewport damage rect is the full viewport and not
// just part of the viewport in the presence of an external viewport.
TEST_F(LayerTreeHostImplTest, FullViewportDamageAfterOnDraw) {
  const gfx::Size viewport_size(100, 100);
  SetupDefaultRootLayer(viewport_size);
  UpdateDrawProperties(host_impl_->active_tree());

  const gfx::Transform draw_transform;
  const gfx::Rect draw_viewport(gfx::Point(5, 5), viewport_size);
  bool resourceless_software_draw = false;

  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_EQ(draw_viewport, host_impl_->active_tree()->GetDeviceViewport());

  host_impl_->SetFullViewportDamage();
  EXPECT_EQ(host_impl_->active_tree()->internal_device_viewport(),
            host_impl_->viewport_damage_rect_for_testing());
}

class ResourcelessSoftwareLayerTreeHostImplTest : public LayerTreeHostImplTest {
 protected:
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }
};

TEST_F(ResourcelessSoftwareLayerTreeHostImplTest,
       ResourcelessSoftwareSetNeedsRedraw) {
  const gfx::Size viewport_size(100, 100);
  SetupDefaultRootLayer(viewport_size);
  UpdateDrawProperties(host_impl_->active_tree());

  const gfx::Transform draw_transform;
  const gfx::Rect draw_viewport(viewport_size);
  bool resourceless_software_draw = false;

  // Clear any damage.
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  last_on_draw_frame_.reset();

  // Always swap even if same draw params.
  resourceless_software_draw = true;
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_FALSE(last_on_draw_frame_->has_no_damage);
  last_on_draw_frame_.reset();

  // Next hardware draw has damage.
  resourceless_software_draw = false;
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_FALSE(last_on_draw_frame_->has_no_damage);
}

TEST_F(ResourcelessSoftwareLayerTreeHostImplTest,
       ResourcelessSoftwareDrawSkipsUpdateTiles) {
  const gfx::Size viewport_size(100, 100);

  CreatePendingTree();
  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(viewport_size));
  auto* root = SetupRootLayer<FakePictureLayerImpl>(
      host_impl_->pending_tree(), viewport_size, raster_source);
  root->SetBounds(viewport_size);
  root->SetDrawsContent(true);

  UpdateDrawProperties(host_impl_->pending_tree());
  host_impl_->ActivateSyncTree();

  const gfx::Transform draw_transform;
  const gfx::Rect draw_viewport(viewport_size);
  bool resourceless_software_draw = false;

  // Regular draw causes UpdateTiles.
  did_request_prepare_tiles_ = false;
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_TRUE(did_request_prepare_tiles_);
  host_impl_->PrepareTiles();

  // Resourceless draw skips UpdateTiles.
  const gfx::Rect new_draw_viewport(50, 50);
  resourceless_software_draw = true;
  did_request_prepare_tiles_ = false;
  host_impl_->OnDraw(draw_transform, new_draw_viewport,
                     resourceless_software_draw, false);
  EXPECT_FALSE(did_request_prepare_tiles_);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       ExternalTileConstraintReflectedInPendingTree) {
  EXPECT_FALSE(host_impl_->CommitToActiveTree());
  const gfx::Size layer_size(100, 100);

  // Set up active and pending tree.
  CreatePendingTree();
  SetupRootLayer<LayerImpl>(host_impl_->pending_tree(), layer_size);
  UpdateDrawProperties(host_impl_->pending_tree());
  host_impl_->pending_tree()->root_layer()->SetNeedsPushProperties();

  host_impl_->ActivateSyncTree();
  UpdateDrawProperties(host_impl_->active_tree());

  CreatePendingTree();
  UpdateDrawProperties(host_impl_->pending_tree());
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(host_impl_->pending_tree()->needs_update_draw_properties());
  EXPECT_FALSE(host_impl_->active_tree()->needs_update_draw_properties());

  // Update external constraints should set_needs_update_draw_properties on
  // both trees.
  gfx::Transform external_transform;
  gfx::Rect external_viewport(10, 20);
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);
  EXPECT_TRUE(host_impl_->pending_tree()->needs_update_draw_properties());
  EXPECT_TRUE(host_impl_->active_tree()->needs_update_draw_properties());
}

TEST_F(LayerTreeHostImplTest, ExternalViewportAffectsVisibleRects) {
  const gfx::Size viewport_size(50, 50);
  const gfx::Size layer_size(100, 100);
  SetupViewportLayersInnerScrolls(viewport_size, layer_size);
  LayerImpl* content_layer = AddContentLayer();

  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(90, 90));
  UpdateDrawProperties(host_impl_->active_tree());
  EXPECT_EQ(gfx::Rect(90, 90), content_layer->visible_layer_rect());

  gfx::Transform external_transform;
  gfx::Rect external_viewport(10, 20);
  bool resourceless_software_draw = false;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_EQ(gfx::Rect(10, 20), content_layer->visible_layer_rect());

  // Clear the external viewport.
  external_viewport = gfx::Rect();
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_EQ(gfx::Rect(90, 90), content_layer->visible_layer_rect());
}

TEST_F(LayerTreeHostImplTest, ExternalTransformAffectsVisibleRects) {
  const gfx::Size viewport_size(50, 50);
  const gfx::Size layer_size(100, 100);
  SetupViewportLayersInnerScrolls(viewport_size, layer_size);
  LayerImpl* content_layer = AddContentLayer();

  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(50, 50));
  UpdateDrawProperties(host_impl_->active_tree());
  EXPECT_EQ(gfx::Rect(50, 50), content_layer->visible_layer_rect());

  gfx::Transform external_transform;
  external_transform.Translate(10, 10);
  external_transform.Scale(2, 2);
  gfx::Rect external_viewport;
  bool resourceless_software_draw = false;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  // Visible rects should now be shifted and scaled because of the external
  // transform.
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_EQ(gfx::Rect(20, 20), content_layer->visible_layer_rect());

  // Clear the external transform.
  external_transform = gfx::Transform();
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_EQ(gfx::Rect(50, 50), content_layer->visible_layer_rect());
}

TEST_F(LayerTreeHostImplTest, ExternalTransformAffectsSublayerScaleFactor) {
  const gfx::Size viewport_size(50, 50);
  const gfx::Size layer_size(100, 100);
  SetupViewportLayersInnerScrolls(viewport_size, layer_size);
  LayerImpl* test_layer = AddContentLayer();
  gfx::Transform perspective_transform;
  perspective_transform.ApplyPerspectiveDepth(2);
  CreateTransformNode(test_layer).local = perspective_transform;
  CreateEffectNode(test_layer).render_surface_reason =
      RenderSurfaceReason::kTest;

  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(50, 50));
  UpdateDrawProperties(host_impl_->active_tree());
  EffectNode* node = GetEffectNode(test_layer);
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(1, 1));

  gfx::Transform external_transform;
  external_transform.Translate(10, 10);
  external_transform.Scale(2, 2);
  gfx::Rect external_viewport;
  bool resourceless_software_draw = false;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  // Transform node's sublayer scale should include the device transform scale.
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  node = GetEffectNode(test_layer);
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(2, 2));

  // Clear the external transform.
  external_transform = gfx::Transform();
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  node = GetEffectNode(test_layer);
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(1, 1));
}

#if defined(OS_MACOSX)
// Ensure Mac wheel scrolling causes instant scrolling. This test can be removed
// once https://crbug.com/574283 is fixed.
TEST_F(LayerTreeHostImplTest, MacWheelIsNonAnimated) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scrolling_layer = OuterViewportScrollLayer();

  host_impl_->set_force_smooth_wheel_scrolling_for_testing(false);
  ASSERT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 50),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)).get());

  // Ensure the scroll update happens immediately.
  EXPECT_EQ(CurrentScrollOffset(scrolling_layer).y(), 50);
  host_impl_->ScrollEnd();
}
#endif

TEST_F(LayerTreeHostImplTest, ScrollAnimated) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  {
    // Creating the animation should set 'needs redraw'. This is required
    // for LatencyInfo's to be propagated along with the CompositorFrame
    int set_needs_commit_count = 0;
    int set_needs_redraw_count = 0;
    std::unique_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
        new SimpleSwapPromiseMonitor(nullptr, host_impl_.get(),
                                     &set_needs_commit_count,
                                     &set_needs_redraw_count));
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 50),
                                           ui::ScrollInputType::kWheel)
                                    .get(),
                                ui::ScrollInputType::kWheel)
                  .thread);
    host_impl_->ScrollUpdate(
        AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)).get());

    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_GT(set_needs_redraw_count, 0);
  }

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_NE(gfx::ScrollOffset(), CurrentScrollOffset(scrolling_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(50);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  float y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 1 && y < 49);

  {
    // Updating the animation should set 'needs redraw'. This is required
    // for LatencyInfo's to be propagated along with the CompositorFrame
    int set_needs_commit_count = 0;
    int set_needs_redraw_count = 0;
    std::unique_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
        new SimpleSwapPromiseMonitor(nullptr, host_impl_.get(),
                                     &set_needs_commit_count,
                                     &set_needs_redraw_count));
    // Update target.
    host_impl_->ScrollUpdate(
        AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)).get());
    host_impl_->ScrollEnd();

    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(1, set_needs_redraw_count);
  }

  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 50 && y < 100);
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 100),
                   CurrentScrollOffset(scrolling_layer));
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

// Tests latching behavior, in particular when a ScrollEnd is received but a
// new ScrollBegin is received before the animation from the previous gesture
// stream is finished.
TEST_F(LayerTreeHostImplTest, ScrollAnimatedLatching) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  AddScrollableLayer(OuterViewportScrollLayer(), gfx::Size(10, 10),
                     gfx::Size(20, 20));

  LayerImpl* outer_scroll = OuterViewportScrollLayer();

  DrawFrame();

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Start an animated scroll over the outer viewport.
  {
    gfx::Point position(40, 40);
    auto begin_state =
        BeginState(position, gfx::Vector2d(0, 50), ui::ScrollInputType::kWheel);
    host_impl_->ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel);
    auto update_state = AnimatedUpdateState(position, gfx::Vector2d(0, 50));
    host_impl_->ScrollUpdate(update_state.get());
    EXPECT_EQ(outer_scroll->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);

    base::TimeTicks start_time =
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    EXPECT_NE(gfx::ScrollOffset(), CurrentScrollOffset(outer_scroll));
    EXPECT_TRUE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());
  }

  // End the scroll gesture. We should still be latched to the scroll layer
  // since the animation is still in progress.
  {
    host_impl_->ScrollEnd();
    EXPECT_TRUE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());
    EXPECT_EQ(outer_scroll->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);

    begin_frame_args.frame_time += base::TimeDelta::FromMilliseconds(5);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    EXPECT_TRUE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());
    EXPECT_EQ(outer_scroll->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
  }

  // Start a new animated scroll. We should remain latched to the outer
  // viewport, despite the fact this scroll began over the child scroller,
  // because we don't clear the latch until the animation is finished.
  {
    gfx::Point position(10, 10);
    auto begin_state =
        BeginState(position, gfx::Vector2d(0, 50), ui::ScrollInputType::kWheel);
    host_impl_->ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel);
    EXPECT_EQ(outer_scroll->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
  }

  // Run the animation to the end. Because we received a ScrollBegin, the
  // deferred ScrollEnd from the first gesture should have been cleared. That
  // is, we shouldn't clear the latch when the animation ends because we're
  // currently in an active scroll gesture.
  {
    begin_frame_args.frame_time += base::TimeDelta::FromMilliseconds(200);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    EXPECT_FALSE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());
    EXPECT_EQ(outer_scroll->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
  }

  // A ScrollEnd should now clear the latch.
  {
    host_impl_->ScrollEnd();
    EXPECT_FALSE(host_impl_->CurrentlyScrollingNode());
  }
}

// Test to ensure that animated scrolls correctly account for the page scale
// factor. That is, if you zoom into the page, a wheel scroll should scroll the
// content *less* than before so that it appears to move the same distance when
// zoomed in.
TEST_F(LayerTreeHostImplTest, ScrollAnimatedWhileZoomed) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scrolling_layer = InnerViewportScrollLayer();

  DrawFrame();

  // Zoom in to 2X
  {
    float min_page_scale = 1, max_page_scale = 4;
    float page_scale_factor = 2;
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  }

  // Start an animated scroll then do another animated scroll immediately
  // afterwards. This will ensure we test both the starting animation and
  // animation update code.
  {
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(BeginState(gfx::Point(10, 10), gfx::Vector2d(0, 10),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel)
            .thread);
    host_impl_->ScrollUpdate(
        AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2d(0, 10)).get());
    host_impl_->ScrollUpdate(
        AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2d(0, 20)).get());

    // Scrolling the inner viewport happens through the Viewport class which
    // uses the outer viewport to represent "latched to the viewport".
    EXPECT_EQ(OuterViewportScrollLayer()->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
  }

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Tick a frame to get the animation started.
  {
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);

    EXPECT_NE(0, CurrentScrollOffset(scrolling_layer).y());
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }

  // Tick ahead to the end of the animation. We scrolled 30 viewport pixels but
  // since we're zoomed in to 2x we should have scrolled 15 content pixels.
  {
    begin_frame_args.frame_time =
        start_time + base::TimeDelta::FromMilliseconds(1000);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);

    EXPECT_EQ(15, CurrentScrollOffset(scrolling_layer).y());
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }
}

// Ensures a scroll updating an in progress animation works correctly when the
// inner viewport is animating. Specifically this test makes sure the animation
// update doesn't get confused between the currently scrolling node and the
// currently animating node which are different. See https://crbug.com/1070561.
TEST_F(LayerTreeHostImplTest, ScrollAnimatedUpdateInnerViewport) {
  const gfx::Size content_size(210, 1000);
  const gfx::Size viewport_size(200, 200);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  // Zoom in to 2X and ensure both viewports have scroll extent in every
  // direction.
  {
    float min_page_scale = 1, max_page_scale = 4;
    float page_scale_factor = 2;
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

    SetScrollOffsetDelta(InnerViewportScrollLayer(), gfx::Vector2d(50, 50));
    SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2d(5, 400));
  }

  // Start an animated scroll on the inner viewport.
  {
    auto begin_state = BeginState(gfx::Point(10, 10), gfx::Vector2d(0, 60),
                                  ui::ScrollInputType::kWheel);
    host_impl_->ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel);
    host_impl_->ScrollUpdate(
        AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2d(0, 60)).get());

    // Scrolling the inner viewport happens through the Viewport class which
    // uses the outer viewport to represent "latched to the viewport".
    EXPECT_EQ(OuterViewportScrollLayer()->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);

    // However, the animating scroll node should be the inner viewport.
    ASSERT_TRUE(InnerViewportScrollLayer()->element_id());
    EXPECT_EQ(InnerViewportScrollLayer()->element_id(),
              GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());
  }

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Tick a frame to get the animation started.
  {
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);

    // The inner viewport should have scrolled a bit from its initial position;
    // the outer viewport should be still. Neither should move horizontally.
    EXPECT_LT(50, CurrentScrollOffset(InnerViewportScrollLayer()).y());
    EXPECT_EQ(400, CurrentScrollOffset(OuterViewportScrollLayer()).y());

    EXPECT_EQ(50, CurrentScrollOffset(InnerViewportScrollLayer()).x());
    EXPECT_EQ(5, CurrentScrollOffset(OuterViewportScrollLayer()).x());
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }

  // Do another scroll update which should update the existing animation curve.
  {
    // This scroll will cause the target to be at the max scroll offset. Inner
    // viewport max offset is 100px. Initial offset is 50px. We scrolled 60px +
    // 60px divided by scale factor 2 so 60px total.
    host_impl_->ScrollUpdate(
        AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2d(0, 60)).get());

    // While we're animating the curve, we don't distribute to the outer
    // viewport so this scroll update should be a no-op.
    host_impl_->ScrollUpdate(
        AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2d(0, 60)).get());
  }

  // Tick ahead to the end of the animation. The inner viewport should be
  // scrolled to the maximum offset. The outer viewport should not have
  // scrolled.
  {
    begin_frame_args.frame_time =
        start_time + base::TimeDelta::FromMilliseconds(1000);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);

    EXPECT_EQ(100, CurrentScrollOffset(InnerViewportScrollLayer()).y());
    EXPECT_EQ(400, CurrentScrollOffset(OuterViewportScrollLayer()).y());

    // Ensure no horizontal delta is produced.
    EXPECT_EQ(50, CurrentScrollOffset(InnerViewportScrollLayer()).x());
    EXPECT_EQ(5, CurrentScrollOffset(OuterViewportScrollLayer()).x());
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }
}

// This tests that faded-out Aura scrollbars can't be interacted with.
TEST_F(LayerTreeHostImplTest, FadedOutPaintedOverlayScrollbarHitTest) {
  LayerTreeSettings settings = DefaultSettings();
  settings.compositor_threaded_scrollbar_scrolling = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedOverlayScrollbarLayerImpl>(layer_tree_impl,
                                                               VERTICAL, false);
  SetupScrollbarLayerCommon(scroll_layer, scrollbar);
  scrollbar->SetHitTestable(true);

  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetThumbLength(50);
  scrollbar->SetTrackStart(0);
  scrollbar->SetTrackLength(575);
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  // Set up the scroll node and other state required for scrolling.
  host_impl_->ScrollBegin(BeginState(gfx::Point(350, 18), gfx::Vector2dF(),
                                     ui::ScrollInputType::kScrollbar)
                              .get(),
                          ui::ScrollInputType::kScrollbar);

  TestInputHandlerClient input_handler_client;
  host_impl_->BindToClient(&input_handler_client);

  // PaintedOverlayScrollbarLayerImpl(s) don't have a track, so we test thumb
  // drags instead. Start with 0.8 opacity. Scrolling is expected to occur in
  // this case.
  auto& scrollbar_effect_node = CreateEffectNode(scrollbar);
  scrollbar_effect_node.opacity = 0.8;

  host_impl_->MouseDown(gfx::PointF(350, 18), /*shift_modifier*/ false);
  InputHandlerPointerResult result =
      host_impl_->MouseMoveAt(gfx::Point(350, 28));
  EXPECT_GT(result.scroll_offset.y(), 0u);
  host_impl_->MouseUp(gfx::PointF(350, 28));

  // Scrolling shouldn't occur at opacity = 0.
  scrollbar_effect_node.opacity = 0;

  host_impl_->MouseDown(gfx::PointF(350, 18), /*shift_modifier*/ false);
  result = host_impl_->MouseMoveAt(gfx::Point(350, 28));
  EXPECT_EQ(result.scroll_offset.y(), 0u);
  host_impl_->MouseUp(gfx::PointF(350, 28));

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that a pointerdown followed by pointermove(s) produces
// InputHandlerPointerResult with scroll_offset > 0 even though the GSB might
// have been dispatched *after* the first pointermove was handled by the
// ScrollbarController.
TEST_F(LayerTreeHostImplTest, PointerMoveOutOfSequence) {
  LayerTreeSettings settings = DefaultSettings();
  settings.compositor_threaded_scrollbar_scrolling = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(layer_tree_impl,
                                                        VERTICAL, false, true);
  SetupScrollbarLayerCommon(scroll_layer, scrollbar);
  scrollbar->SetHitTestable(true);

  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  host_impl_->BindToClient(&input_handler_client);

  // PointerDown sets up the state needed to initiate a drag. Although, the
  // resulting GSB won't be dispatched until the next VSync. Hence, the
  // CurrentlyScrollingNode is expected to be null.
  host_impl_->MouseDown(gfx::PointF(350, 18), /*shift_modifier*/ false);
  EXPECT_TRUE(!host_impl_->CurrentlyScrollingNode());

  // PointerMove arrives before the next VSync. This still needs to be handled
  // by the ScrollbarController even though the GSB has not yet been dispatched.
  // Note that this doesn't result in a scroll yet. It only prepares a GSU based
  // on the result that is returned by ScrollbarController::HandlePointerMove.
  InputHandlerPointerResult result =
      host_impl_->MouseMoveAt(gfx::Point(350, 19));
  EXPECT_GT(result.scroll_offset.y(), 0u);

  // GSB gets dispatched at VSync.
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_args.frame_time = base::TimeTicks::Now();
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->ScrollBegin(BeginState(gfx::Point(350, 18), gfx::Vector2dF(),
                                     ui::ScrollInputType::kScrollbar)
                              .get(),
                          ui::ScrollInputType::kScrollbar);
  EXPECT_TRUE(host_impl_->CurrentlyScrollingNode());

  // The PointerMove(s) that follow should be handled and are expected to have a
  // scroll_offset > 0.
  result = host_impl_->MouseMoveAt(gfx::Point(350, 20));
  EXPECT_GT(result.scroll_offset.y(), 0u);

  // End the scroll.
  host_impl_->MouseUp(gfx::PointF(350, 20));
  host_impl_->ScrollEnd();
  EXPECT_TRUE(!host_impl_->CurrentlyScrollingNode());

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// This tests that faded-out Mac scrollbars can't be interacted with.
TEST_F(LayerTreeHostImplTest, FadedOutPaintedScrollbarHitTest) {
  LayerTreeSettings settings = DefaultSettings();
  settings.compositor_threaded_scrollbar_scrolling = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(layer_tree_impl,
                                                        VERTICAL, false, true);
  SetupScrollbarLayerCommon(scroll_layer, scrollbar);
  scrollbar->SetHitTestable(true);

  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  host_impl_->BindToClient(&input_handler_client);

  // MouseDown on the track of a scrollbar with opacity 0 should not produce a
  // scroll.
  scrollbar->set_scrollbar_painted_opacity(0);
  InputHandlerPointerResult result =
      host_impl_->MouseDown(gfx::PointF(350, 100), /*shift_modifier*/ false);
  EXPECT_EQ(result.scroll_offset.y(), 0u);

  // MouseDown on the track of a scrollbar with opacity > 0 should produce a
  // scroll.
  scrollbar->set_scrollbar_painted_opacity(1);
  result =
      host_impl_->MouseDown(gfx::PointF(350, 100), /*shift_modifier*/ false);
  EXPECT_GT(result.scroll_offset.y(), 0u);

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_F(LayerTreeHostImplTest, SingleGSUForScrollbarThumbDragPerFrame) {
  LayerTreeSettings settings = DefaultSettings();
  settings.compositor_threaded_scrollbar_scrolling = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(layer_tree_impl,
                                                        VERTICAL, false, true);
  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(
      gfx::Rect(gfx::Point(345, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(345, 570), gfx::Size(15, 15)));

  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  host_impl_->ScrollBegin(BeginState(gfx::Point(350, 18), gfx::Vector2dF(),
                                     ui::ScrollInputType::kScrollbar)
                              .get(),
                          ui::ScrollInputType::kScrollbar);
  TestInputHandlerClient input_handler_client;
  host_impl_->BindToClient(&input_handler_client);

  // ------------------------- Start frame 0 -------------------------
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(200);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);

  // MouseDown on the thumb should not produce a scroll.
  InputHandlerPointerResult result =
      host_impl_->MouseDown(gfx::PointF(350, 18), /*shift_modifier*/ false);
  EXPECT_EQ(result.scroll_offset.y(), 0u);

  // The first request for a GSU should be processed as expected.
  result = host_impl_->MouseMoveAt(gfx::Point(350, 19));
  EXPECT_GT(result.scroll_offset.y(), 0u);

  // A second request for a GSU within the same frame should be ignored as it
  // will cause the thumb drag to become jittery. The reason this happens is
  // because when the first GSU is processed, it gets queued in the compositor
  // thread event queue. So a second request within the same frame will end up
  // calculating an incorrect delta (as ComputeThumbQuadRect would not have
  // accounted for the delta in the first GSU that was not yet dispatched).
  result = host_impl_->MouseMoveAt(gfx::Point(350, 20));
  EXPECT_EQ(result.scroll_offset.y(), 0u);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // ------------------------- Start frame 1 -------------------------
  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);

  // MouseMove for a new frame gets processed as usual.
  result = host_impl_->MouseMoveAt(gfx::Point(350, 21));
  EXPECT_GT(result.scroll_offset.y(), 0u);

  // MouseUp is not expected to have a delta.
  result = host_impl_->MouseUp(gfx::PointF(350, 21));
  EXPECT_EQ(result.scroll_offset.y(), 0u);

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that an animated scrollbar scroll aborts when a different device (like
// a mousewheel) wants to animate the scroll offset.
TEST_F(LayerTreeHostImplTest, AnimatedScrollYielding) {
  LayerTreeSettings settings = DefaultSettings();
  settings.compositor_threaded_scrollbar_scrolling = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, VERTICAL, /*is_left_side_vertical_scrollbar*/ false,
      /*is_overlay*/ false);

  // TODO(arakeri): crbug.com/1070063 Setting the dimensions for scrollbar parts
  // (like thumb, track etc) should be moved to SetupScrollbarLayer.
  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);
  host_impl_->set_force_smooth_wheel_scrolling_for_testing(true);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(
      gfx::Rect(gfx::Point(345, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(345, 570), gfx::Size(15, 15)));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  host_impl_->BindToClient(&input_handler_client);

  {
    // Set up an animated scrollbar autoscroll.
    host_impl_->scrollbar_controller_for_testing()->HandlePointerDown(
        gfx::PointF(350, 560), /*shift_modifier*/ false);
    auto begin_state = BeginState(gfx::Point(350, 560), gfx::Vector2d(0, 40),
                                  ui::ScrollInputType::kScrollbar);
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(begin_state.get(), ui::ScrollInputType::kScrollbar)
            .thread);
    auto update_state = UpdateState(gfx::Point(350, 560), gfx::Vector2dF(0, 40),
                                    ui::ScrollInputType::kScrollbar);
    update_state->data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;
    host_impl_->ScrollUpdate(update_state.get());

    // Autoscroll animations should be active.
    EXPECT_TRUE(host_impl_->scrollbar_controller_for_testing()
                    ->ScrollbarScrollIsActive());
    EXPECT_TRUE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());
  }

  {
    // While the cc autoscroll is in progress, a mousewheel tick takes place.
    auto begin_state = BeginState(gfx::Point(), gfx::Vector2d(350, 560),
                                  ui::ScrollInputType::kWheel);
    begin_state->data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;

    // InputHandlerProxy calls LayerTreeHostImpl::ScrollEnd to end the
    // autoscroll as it has detected a device change.
    host_impl_->ScrollEnd();

    // This then leads to ScrollbarController::ResetState getting called which
    // clears ScrollbarController::autoscroll_state_,
    // captured_scrollbar_metadata_ etc. That means
    // ScrollbarController::ScrollbarLayer should return null.
    EXPECT_FALSE(
        host_impl_->scrollbar_controller_for_testing()->ScrollbarLayer());

    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_->ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel)
            .thread);

    // Autoscroll animation should be aborted at this point.
    EXPECT_FALSE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());
    host_impl_->ScrollUpdate(
        AnimatedUpdateState(gfx::Point(350, 560), gfx::Vector2d(0, 40)).get());

    // Mousewheel animation should be active.
    EXPECT_TRUE(GetImplAnimationHost()->ImplOnlyScrollAnimatingElement());

    // This should not trigger any DCHECKs and will be a no-op.
    host_impl_->scrollbar_controller_for_testing()->WillBeginImplFrame();
  }

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that changing the scroller length in the middle of a thumb drag doesn't
// cause the scroller to jump.
TEST_F(LayerTreeHostImplTest, ThumbDragScrollerLengthIncrease) {
  LayerTreeSettings settings = DefaultSettings();
  settings.compositor_threaded_scrollbar_scrolling = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, VERTICAL, /*is_left_side_vertical_scrollbar*/ false,
      /*is_overlay*/ false);

  // TODO(arakeri): crbug.com/1070063 Setting the dimensions for scrollbar parts
  // (like thumb, track etc) should be moved to SetupScrollbarLayer.
  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(
      gfx::Rect(gfx::Point(345, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(345, 570), gfx::Size(15, 15)));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  host_impl_->BindToClient(&input_handler_client);

  // ----------------------------- Start frame 0 -----------------------------
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  InputHandlerPointerResult result =
      host_impl_->MouseDown(gfx::PointF(350, 18), /*shift_modifier*/ false);
  EXPECT_EQ(result.scroll_offset.y(), 0);
  EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // ----------------------------- Start frame 1 -----------------------------
  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);

  result = host_impl_->MouseMoveAt(gfx::Point(350, 20));
  EXPECT_EQ(result.scroll_offset.y(), 12);

  // This is intentional. The thumb drags that follow will test the behavior
  // *after* the scroller length expansion.
  scrollbar->SetScrollLayerLength(7000);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // ----------------------------- Start frame 2 -----------------------------
  // The very first mousemove after the scroller length change will result in a
  // zero offset. This is done to ensure that the scroller doesn't jump ahead
  // when the length changes mid thumb drag. So, every time the scroller length
  // changes mid thumb drag, a GSU is lost (in the worst case).
  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(300);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  result = host_impl_->MouseMoveAt(gfx::Point(350, 22));
  EXPECT_EQ(result.scroll_offset.y(), 0);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // ----------------------------- Start frame 3 -----------------------------
  // All subsequent mousemoves then produce deltas which are "displaced" by a
  // certain amount. The previous mousemove (to y = 22) would have calculated
  // the drag_state_->scroller_displacement value as 48 (based on the pointer
  // movement). The current pointermove (to y = 26) calculates the delta as 97.
  // Since delta -= drag_state_->scroller_displacement, the actual delta becomes
  // 97 - 48 which is 49. The math that calculates the deltas (i.e 97 and 48)
  // can be found in ScrollbarController::GetScrollDeltaForDragPosition.
  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(350);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  result = host_impl_->MouseMoveAt(gfx::Point(350, 26));
  EXPECT_EQ(result.scroll_offset.y(), 49);
  host_impl_->MouseUp(gfx::PointF(350, 26));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_F(LayerTreeHostImplTest, MainThreadFallback) {
  LayerTreeSettings settings = DefaultSettings();
  settings.compositor_threaded_scrollbar_scrolling = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(layer_tree_impl,
                                                        VERTICAL, false, true);
  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(
      gfx::Rect(gfx::Point(345, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(345, 570), gfx::Size(15, 15)));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  host_impl_->BindToClient(&input_handler_client);

  // Clicking on the track should produce a scroll on the compositor thread.
  InputHandlerPointerResult compositor_threaded_scrolling_result =
      host_impl_->MouseDown(gfx::PointF(350, 500), /*shift_modifier*/ false);
  host_impl_->MouseUp(gfx::PointF(350, 500));
  EXPECT_EQ(compositor_threaded_scrolling_result.scroll_offset.y(), 525u);
  EXPECT_FALSE(GetScrollNode(scroll_layer)->main_thread_scrolling_reasons);

  // If the scroll_node has a main_thread_scrolling_reason, the
  // InputHandlerPointerResult should return a zero offset. This will cause the
  // main thread to handle the scroll.
  GetScrollNode(scroll_layer)->main_thread_scrolling_reasons =
      MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects;
  compositor_threaded_scrolling_result =
      host_impl_->MouseDown(gfx::PointF(350, 500), /*shift_modifier*/ false);
  host_impl_->MouseUp(gfx::PointF(350, 500));
  EXPECT_EQ(compositor_threaded_scrolling_result.scroll_offset.y(), 0u);

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_F(LayerTreeHostImplTest, SecondScrollAnimatedBeginNotIgnored) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);

  host_impl_->ScrollEnd();

  // The second ScrollBegin should not get ignored.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
}

// Verfify that a smooth scroll animation doesn't jump when UpdateTarget gets
// called before the animation is started.
TEST_F(LayerTreeHostImplTest, AnimatedScrollUpdateTargetBeforeStarting) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(200);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 50),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)).get());
  // This will call ScrollOffsetAnimationCurve::UpdateTarget while the animation
  // created above is in state ANIMATION::WAITING_FOR_TARGET_AVAILABILITY and
  // doesn't have a start time.
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 100)).get());

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  // This is when the animation above gets promoted to STARTING.
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(300);
  begin_frame_args.frame_id.sequence_number++;
  // This is when the animation above gets ticked.
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  // Verify no jump.
  float y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 1 && y < 49);
}

TEST_F(LayerTreeHostImplTest, ScrollAnimatedWithDelay) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Create animation with a 100ms delay.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 100),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 100)).get(),
      base::TimeDelta::FromMilliseconds(100));

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  // First tick, animation is started.
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  EXPECT_NE(gfx::ScrollOffset(), CurrentScrollOffset(scrolling_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Second tick after 50ms, animation should be half way done since the
  // duration due to delay is 100ms. Subtract off the frame interval since we
  // progress a full frame on the first tick.
  base::TimeTicks half_way_time = start_time - begin_frame_args.interval +
                                  base::TimeDelta::FromMilliseconds(50);
  begin_frame_args.frame_time = half_way_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(50, CurrentScrollOffset(scrolling_layer).y());
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Update target.
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 100)).get(),
      base::TimeDelta::FromMilliseconds(150));

  // Third tick after 100ms, should be at the target position since update
  // target was called with a large value of jank.
  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(100);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  EXPECT_LT(100, CurrentScrollOffset(scrolling_layer).y());
}

// Test that a smooth scroll offset animation is aborted when followed by a
// non-smooth scroll offset animation.
TEST_F(LayerTreeHostImplTimelinesTest, ScrollAnimatedAborted) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Perform animated scroll.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 50),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)).get());

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_TRUE(GetImplAnimationHost()->HasAnyAnimationTargetingProperty(
      scrolling_layer->element_id(), TargetProperty::SCROLL_OFFSET));

  EXPECT_NE(gfx::ScrollOffset(), CurrentScrollOffset(scrolling_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(50);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  float y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 1 && y < 49);

  host_impl_->ScrollEnd();

  // Perform instant scroll.
  // Use "precise pixel" granularity to avoid animating.
  auto begin_state = BeginState(gfx::Point(0, y), gfx::Vector2dF(0, 50),
                                ui::ScrollInputType::kWheel);
  begin_state->data()->delta_granularity =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel)
          .thread);
  auto update_state = UpdateState(gfx::Point(0, y), gfx::Vector2d(0, 50),
                                  ui::ScrollInputType::kWheel);
  // Use "precise pixel" granularity to avoid animating.
  update_state->data()->delta_granularity =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  host_impl_->ScrollUpdate(update_state.get());
  host_impl_->ScrollEnd();

  // The instant scroll should have marked the smooth scroll animation as
  // aborted.
  EXPECT_FALSE(GetImplAnimationHost()->HasTickingKeyframeModelForTesting(
      scrolling_layer->element_id()));

  EXPECT_VECTOR2DF_EQ(gfx::ScrollOffset(0, y + 50),
                      CurrentScrollOffset(scrolling_layer));
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

// Evolved from LayerTreeHostImplTest.ScrollAnimated.
TEST_F(LayerTreeHostImplTimelinesTest, ScrollAnimated) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 50),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)).get());

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_NE(gfx::ScrollOffset(), CurrentScrollOffset(scrolling_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(50);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  float y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 1 && y < 49);

  // Update target.
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)).get());
  host_impl_->ScrollEnd();
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 50 && y < 100);
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 100),
                   CurrentScrollOffset(scrolling_layer));
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

// Test that the scroll delta for an animated scroll is distributed correctly
// between the inner and outer viewport.
TEST_F(LayerTreeHostImplTimelinesTest, ImplPinchZoomScrollAnimated) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  UpdateDrawProperties(host_impl_->active_tree());

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();

  // Zoom into the page by a 2X factor
  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Scroll by a small amount, there should be no bubbling to the outer
  // viewport (but scrolling the viewport always sets the outer as the
  // currently scrolling node).
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(250);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(10, 20),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(10, 20)).get());
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 10),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));

  // Scroll by the inner viewport's max scroll extent, the remainder
  // should bubble up to the outer viewport.
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(100, 100)).get());
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(350));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 10),
                   CurrentScrollOffset(outer_scroll_layer));

  // Scroll by the outer viewport's max scroll extent, it should all go to the
  // outer viewport.
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(190, 180)).get());
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(850));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(100, 100),
                   CurrentScrollOffset(outer_scroll_layer));

  // Scroll upwards by the max scroll extent. The inner viewport should animate
  // and the remainder should bubble to the outer viewport.
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(-110, -120)).get());
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1200));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(95, 90),
                   CurrentScrollOffset(outer_scroll_layer));
}

// Test that the correct viewport scroll layer is updated when the target offset
// is updated.
TEST_F(LayerTreeHostImplTimelinesTest, ImplPinchZoomScrollAnimatedUpdate) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();

  // Zoom into the page by a 2X factor
  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Scroll the inner viewport.
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(50);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(90, 90),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(90, 90)).get());
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  // When either the inner or outer node is being scrolled, the outer node is
  // the one that's "latched".
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  float inner_x = CurrentScrollOffset(inner_scroll_layer).x();
  float inner_y = CurrentScrollOffset(inner_scroll_layer).y();
  EXPECT_TRUE(inner_x > 0 && inner_x < 45);
  EXPECT_TRUE(inner_y > 0 && inner_y < 45);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   CurrentScrollOffset(outer_scroll_layer));

  // Update target.
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(50, 50)).get());
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  // Verify that all the delta is applied to the inner viewport and nothing is
  // carried forward.
  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(350));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(outer_scroll_layer));
}

// Test that smooth scroll offset animation doesn't happen for non user
// scrollable layers.
TEST_F(LayerTreeHostImplTimelinesTest, ScrollAnimatedNotUserScrollable) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  GetScrollNode(scrolling_layer)->user_scrollable_vertical = true;
  GetScrollNode(scrolling_layer)->user_scrollable_horizontal = false;

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(50, 50),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(50, 50)).get());

  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_NE(gfx::ScrollOffset(), CurrentScrollOffset(scrolling_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(50);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  // Should not have scrolled horizontally.
  EXPECT_EQ(0, CurrentScrollOffset(scrolling_layer).x());
  float y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 1 && y < 49);

  // Update target.
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(50, 50)).get());
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 50 && y < 100);
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 100),
                   CurrentScrollOffset(scrolling_layer));
  // The CurrentlyScrollingNode shouldn't be cleared until a GestureScrollEnd.
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  host_impl_->ScrollEnd();
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
}

// Test that smooth scrolls clamp correctly when bounds change mid-animation.
TEST_F(LayerTreeHostImplTimelinesTest, ScrollAnimatedChangingBounds) {
  const gfx::Size old_content_size(1000, 1000);
  const gfx::Size new_content_size(750, 750);
  const gfx::Size viewport_size(500, 500);

  SetupViewportLayersOuterScrolls(viewport_size, old_content_size);
  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  LayerImpl* content_layer = AddContentLayer();

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(500, 500),
                                     ui::ScrollInputType::kWheel)
                              .get(),
                          ui::ScrollInputType::kWheel);
  host_impl_->ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(500, 500)).get());

  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  content_layer->SetBounds(new_content_size);
  scrolling_layer->SetBounds(new_content_size);
  GetScrollNode(scrolling_layer)->bounds = new_content_size;

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  EXPECT_EQ(gfx::ScrollOffset(250, 250), CurrentScrollOffset(scrolling_layer));
}

TEST_F(LayerTreeHostImplTest, InvalidLayerNotAddedToRasterQueue) {
  CreatePendingTree();

  Region empty_invalidation;
  scoped_refptr<RasterSource> raster_source_with_tiles(
      FakeRasterSource::CreateFilled(gfx::Size(10, 10)));

  auto* layer = SetupRootLayer<FakePictureLayerImpl>(host_impl_->pending_tree(),
                                                     gfx::Size(10, 10));
  layer->set_gpu_raster_max_texture_size(
      host_impl_->active_tree()->GetDeviceViewport().size());
  layer->SetDrawsContent(true);
  layer->tilings()->AddTiling(gfx::AxisTransform2d(), raster_source_with_tiles);
  layer->UpdateRasterSource(raster_source_with_tiles, &empty_invalidation,
                            nullptr, nullptr);
  layer->tilings()->tiling_at(0)->set_resolution(
      TileResolution::HIGH_RESOLUTION);
  layer->tilings()->tiling_at(0)->CreateAllTilesForTesting();
  layer->tilings()->UpdateTilePriorities(gfx::Rect(gfx::Size(10, 10)), 1, 1.0,
                                         Occlusion(), true);

  layer->set_has_valid_tile_priorities(true);
  std::unique_ptr<RasterTilePriorityQueue> non_empty_raster_priority_queue_all =
      host_impl_->BuildRasterQueue(TreePriority::SAME_PRIORITY_FOR_BOTH_TREES,
                                   RasterTilePriorityQueue::Type::ALL);
  EXPECT_FALSE(non_empty_raster_priority_queue_all->IsEmpty());

  layer->set_has_valid_tile_priorities(false);
  std::unique_ptr<RasterTilePriorityQueue> empty_raster_priority_queue_all =
      host_impl_->BuildRasterQueue(TreePriority::SAME_PRIORITY_FOR_BOTH_TREES,
                                   RasterTilePriorityQueue::Type::ALL);
  EXPECT_TRUE(empty_raster_priority_queue_all->IsEmpty());
}

TEST_F(LayerTreeHostImplTest, DidBecomeActive) {
  CreatePendingTree();
  host_impl_->ActivateSyncTree();
  CreatePendingTree();

  LayerTreeImpl* pending_tree = host_impl_->pending_tree();

  auto* pending_layer =
      SetupRootLayer<FakePictureLayerImpl>(pending_tree, gfx::Size(10, 10));

  EXPECT_EQ(0u, pending_layer->did_become_active_call_count());
  pending_tree->DidBecomeActive();
  EXPECT_EQ(1u, pending_layer->did_become_active_call_count());

  std::unique_ptr<FakePictureLayerImpl> mask_layer =
      FakePictureLayerImpl::Create(pending_tree, next_layer_id_++);
  FakePictureLayerImpl* raw_mask_layer = mask_layer.get();
  SetupMaskProperties(pending_layer, raw_mask_layer);
  pending_tree->AddLayer(std::move(mask_layer));

  EXPECT_EQ(1u, pending_layer->did_become_active_call_count());
  EXPECT_EQ(0u, raw_mask_layer->did_become_active_call_count());
  pending_tree->DidBecomeActive();
  EXPECT_EQ(2u, pending_layer->did_become_active_call_count());
  EXPECT_EQ(1u, raw_mask_layer->did_become_active_call_count());

  EXPECT_EQ(2u, pending_layer->did_become_active_call_count());
  EXPECT_EQ(1u, raw_mask_layer->did_become_active_call_count());
  pending_tree->DidBecomeActive();
  EXPECT_EQ(3u, pending_layer->did_become_active_call_count());
  EXPECT_EQ(2u, raw_mask_layer->did_become_active_call_count());
}

TEST_F(LayerTreeHostImplTest, WheelScrollWithPageScaleFactorOnInnerLayer) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(50, 50));
  DrawFrame();

  EXPECT_EQ(scroll_layer, InnerViewportScrollLayer());

  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 1;

  // The scroll deltas should have the page scale factor applied.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2;
    host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                       ui::ScrollInputType::kTouchscreen)
                                .get(),
                            ui::ScrollInputType::kTouchscreen);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point());
    host_impl_->PinchGestureEnd(gfx::Point(), true);
    host_impl_->ScrollEnd();

    gfx::Vector2dF scroll_delta(0, 5);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                           ui::ScrollInputType::kWheel)
                                    .get(),
                                ui::ScrollInputType::kWheel)
                  .thread);
    EXPECT_VECTOR_EQ(gfx::Vector2dF(), CurrentScrollOffset(scroll_layer));

    host_impl_->ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel)
            .get());
    host_impl_->ScrollEnd();
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 2.5), CurrentScrollOffset(scroll_layer));
  }
}

class LayerTreeHostImplCountingLostSurfaces : public LayerTreeHostImplTest {
 public:
  void DidLoseLayerTreeFrameSinkOnImplThread() override {
    num_lost_surfaces_++;
  }

 protected:
  int num_lost_surfaces_ = 0;
};

// We do not want to reset context recovery state when we get repeated context
// loss notifications via different paths.
TEST_F(LayerTreeHostImplCountingLostSurfaces, TwiceLostSurface) {
  EXPECT_EQ(0, num_lost_surfaces_);
  host_impl_->DidLoseLayerTreeFrameSink();
  EXPECT_EQ(1, num_lost_surfaces_);
  host_impl_->DidLoseLayerTreeFrameSink();
  EXPECT_EQ(1, num_lost_surfaces_);
}

size_t CountRenderPassesWithId(const viz::RenderPassList& list,
                               viz::RenderPassId id) {
  return std::count_if(
      list.begin(), list.end(),
      [id](const std::unique_ptr<viz::RenderPass>& p) { return p->id == id; });
}

TEST_F(LayerTreeHostImplTest, RemoveUnreferencedRenderPass) {
  TestFrameData frame;
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass3 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass2 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass1 = frame.render_passes.back().get();

  pass1->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass2->SetNew(2, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass3->SetNew(3, gfx::Rect(), gfx::Rect(), gfx::Transform());

  // Add a quad to each pass so they aren't empty.
  auto* color_quad = pass1->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::Material::kSolidColor;
  color_quad = pass2->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::Material::kSolidColor;
  color_quad = pass3->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::Material::kSolidColor;

  // pass3 is referenced by pass2.
  auto* rpdq = pass2->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kRenderPass;
  rpdq->render_pass_id = pass3->id;

  // But pass2 is not referenced by pass1. So pass2 and pass3 should be culled.
  FakeLayerTreeHostImpl::RemoveRenderPasses(&frame);
  EXPECT_EQ(1u, frame.render_passes.size());
  EXPECT_EQ(1u, CountRenderPassesWithId(frame.render_passes, 1u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 2u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 3u));
  EXPECT_EQ(1u, frame.render_passes[0]->id);
}

TEST_F(LayerTreeHostImplTest, RemoveEmptyRenderPass) {
  TestFrameData frame;
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass3 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass2 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass1 = frame.render_passes.back().get();

  pass1->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass2->SetNew(2, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass3->SetNew(3, gfx::Rect(), gfx::Rect(), gfx::Transform());

  // pass1 is not empty, but pass2 and pass3 are.
  auto* color_quad = pass1->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::Material::kSolidColor;

  // pass3 is referenced by pass2.
  auto* rpdq = pass2->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kRenderPass;
  rpdq->render_pass_id = pass3->id;

  // pass2 is referenced by pass1.
  rpdq = pass1->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kRenderPass;
  rpdq->render_pass_id = pass2->id;

  // Since pass3 is empty it should be removed. Then pass2 is empty too, and
  // should be removed.
  FakeLayerTreeHostImpl::RemoveRenderPasses(&frame);
  EXPECT_EQ(1u, frame.render_passes.size());
  EXPECT_EQ(1u, CountRenderPassesWithId(frame.render_passes, 1u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 2u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 3u));
  EXPECT_EQ(1u, frame.render_passes[0]->id);
  // The viz::RenderPassDrawQuad should be removed from pass1.
  EXPECT_EQ(1u, pass1->quad_list.size());
  EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
            pass1->quad_list.ElementAt(0)->material);
}

TEST_F(LayerTreeHostImplTest, DoNotRemoveEmptyRootRenderPass) {
  TestFrameData frame;
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass3 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass2 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass1 = frame.render_passes.back().get();

  pass1->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass2->SetNew(2, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass3->SetNew(3, gfx::Rect(), gfx::Rect(), gfx::Transform());

  // pass3 is referenced by pass2.
  auto* rpdq = pass2->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kRenderPass;
  rpdq->render_pass_id = pass3->id;

  // pass2 is referenced by pass1.
  rpdq = pass1->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kRenderPass;
  rpdq->render_pass_id = pass2->id;

  // Since pass3 is empty it should be removed. Then pass2 is empty too, and
  // should be removed. Then pass1 is empty too, but it's the root so it should
  // not be removed.
  FakeLayerTreeHostImpl::RemoveRenderPasses(&frame);
  EXPECT_EQ(1u, frame.render_passes.size());
  EXPECT_EQ(1u, CountRenderPassesWithId(frame.render_passes, 1u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 2u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 3u));
  EXPECT_EQ(1u, frame.render_passes[0]->id);
  // The viz::RenderPassDrawQuad should be removed from pass1.
  EXPECT_EQ(0u, pass1->quad_list.size());
}

class FakeVideoFrameController : public VideoFrameController {
 public:
  void OnBeginFrame(const viz::BeginFrameArgs& args) override {
    begin_frame_args_ = args;
    did_draw_frame_ = false;
  }

  void DidDrawFrame() override { did_draw_frame_ = true; }

  const viz::BeginFrameArgs& begin_frame_args() const {
    return begin_frame_args_;
  }

  bool did_draw_frame() const { return did_draw_frame_; }

 private:
  viz::BeginFrameArgs begin_frame_args_;
  bool did_draw_frame_ = false;
};

TEST_F(LayerTreeHostImplTest, AddVideoFrameControllerInsideFrame) {
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  FakeVideoFrameController controller;

  host_impl_->WillBeginImplFrame(begin_frame_args);
  EXPECT_FALSE(controller.begin_frame_args().IsValid());
  host_impl_->AddVideoFrameController(&controller);
  EXPECT_TRUE(controller.begin_frame_args().IsValid());
  host_impl_->DidFinishImplFrame(begin_frame_args);

  EXPECT_FALSE(controller.did_draw_frame());
  TestFrameData frame;
  host_impl_->DidDrawAllLayers(frame);
  EXPECT_TRUE(controller.did_draw_frame());

  controller.OnBeginFrame(begin_frame_args);
  EXPECT_FALSE(controller.did_draw_frame());
  host_impl_->RemoveVideoFrameController(&controller);
  host_impl_->DidDrawAllLayers(frame);
  EXPECT_FALSE(controller.did_draw_frame());
}

TEST_F(LayerTreeHostImplTest, AddVideoFrameControllerOutsideFrame) {
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  FakeVideoFrameController controller;

  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  EXPECT_FALSE(controller.begin_frame_args().IsValid());
  host_impl_->AddVideoFrameController(&controller);
  EXPECT_FALSE(controller.begin_frame_args().IsValid());

  begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 3);
  EXPECT_FALSE(controller.begin_frame_args().IsValid());
  host_impl_->WillBeginImplFrame(begin_frame_args);
  EXPECT_TRUE(controller.begin_frame_args().IsValid());

  EXPECT_FALSE(controller.did_draw_frame());
  TestFrameData frame;
  host_impl_->DidDrawAllLayers(frame);
  EXPECT_TRUE(controller.did_draw_frame());

  controller.OnBeginFrame(begin_frame_args);
  EXPECT_FALSE(controller.did_draw_frame());
  host_impl_->RemoveVideoFrameController(&controller);
  host_impl_->DidDrawAllLayers(frame);
  EXPECT_FALSE(controller.did_draw_frame());
}

// Tests that SetDeviceScaleFactor correctly impacts GPU rasterization.
TEST_F(LayerTreeHostImplTest, GpuRasterizationStatusDeviceScaleFactor) {
  // Create a host impl with MSAA support (4 samples).
  LayerTreeSettings msaaSettings = DefaultSettings();
  msaaSettings.gpu_rasterization_msaa_sample_count = -1;
  EXPECT_TRUE(CreateHostImpl(
      msaaSettings, FakeLayerTreeFrameSink::Create3dForGpuRasterization(4)));

  // Set initial state, before varying scale factor.
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON, host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  host_impl_->NotifyReadyToActivate();

  // Set device scale factor to 2, which lowers the required MSAA samples from
  // 8 to 4.
  host_impl_->active_tree()->SetDeviceScaleFactor(2.0f);
  host_impl_->CommitComplete();
  EXPECT_TRUE(host_impl_->can_use_msaa());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  host_impl_->NotifyReadyToActivate();

  // Set device scale factor back to 1.
  host_impl_->active_tree()->SetDeviceScaleFactor(1.0f);
  host_impl_->CommitComplete();
  EXPECT_FALSE(host_impl_->can_use_msaa());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  host_impl_->NotifyReadyToActivate();
}

// Tests that explicit MSAA sample count correctly impacts GPU rasterization.
TEST_F(LayerTreeHostImplTest, GpuRasterizationStatusExplicitMSAACount) {
  // Create a host impl with MSAA support and a forced sample count of 4.
  LayerTreeSettings msaaSettings = DefaultSettings();
  msaaSettings.gpu_rasterization_msaa_sample_count = 4;
  EXPECT_TRUE(CreateHostImpl(
      msaaSettings, FakeLayerTreeFrameSink::Create3dForGpuRasterization(
                        msaaSettings.gpu_rasterization_msaa_sample_count)));

  host_impl_->CommitComplete();
  EXPECT_TRUE(host_impl_->can_use_msaa());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
}

class GpuRasterizationDisabledLayerTreeHostImplTest
    : public LayerTreeHostImplTest {
 public:
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }
};

class MsaaIsSlowLayerTreeHostImplTest : public LayerTreeHostImplTest {
 public:
  void CreateHostImplWithCaps(bool msaa_is_slow, bool avoid_stencil_buffers) {
    LayerTreeSettings settings = DefaultSettings();
    settings.gpu_rasterization_msaa_sample_count = 4;
    auto frame_sink =
        FakeLayerTreeFrameSink::Builder()
            .AllContexts(&viz::TestGLES2Interface::SetMaxSamples,
                         settings.gpu_rasterization_msaa_sample_count)
            .AllContexts(&viz::TestGLES2Interface::set_msaa_is_slow,
                         msaa_is_slow)
            .AllContexts(&viz::TestGLES2Interface::set_gpu_rasterization, true)
            .AllContexts(&viz::TestGLES2Interface::set_avoid_stencil_buffers,
                         avoid_stencil_buffers)
            .Build();
    EXPECT_TRUE(CreateHostImpl(settings, std::move(frame_sink)));
  }
};

TEST_F(MsaaIsSlowLayerTreeHostImplTest, GpuRasterizationStatusMsaaIsSlow) {
  // Ensure that without the msaa_is_slow or avoid_stencil_buffers caps
  // we raster slow paths with msaa.
  CreateHostImplWithCaps(false, false);
  host_impl_->CommitComplete();
  EXPECT_TRUE(host_impl_->can_use_msaa());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());

  // Ensure that with either msaa_is_slow or avoid_stencil_buffers caps
  // we don't raster slow paths with msaa (we'll still use GPU raster, though).
  // msaa_is_slow = true, avoid_stencil_buffers = false
  CreateHostImplWithCaps(true, false);
  host_impl_->CommitComplete();
  EXPECT_FALSE(host_impl_->can_use_msaa());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());

  // msaa_is_slow = false, avoid_stencil_buffers = true
  CreateHostImplWithCaps(false, true);
  host_impl_->CommitComplete();
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->can_use_msaa());

  // msaa_is_slow = true, avoid_stencil_buffers = true
  CreateHostImplWithCaps(true, true);
  host_impl_->CommitComplete();
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->can_use_msaa());
}

class MsaaCompatibilityLayerTreeHostImplTest : public LayerTreeHostImplTest {
 public:
  void CreateHostImplWithMultisampleCompatibility(
      bool support_multisample_compatibility) {
    LayerTreeSettings settings = DefaultSettings();
    settings.gpu_rasterization_msaa_sample_count = 4;
    auto frame_sink =
        FakeLayerTreeFrameSink::Builder()
            .AllContexts(&viz::TestGLES2Interface::SetMaxSamples,
                         settings.gpu_rasterization_msaa_sample_count)
            .AllContexts(
                &viz::TestGLES2Interface::set_support_multisample_compatibility,
                support_multisample_compatibility)
            .AllContexts(&viz::TestGLES2Interface::set_gpu_rasterization, true)
            .Build();
    EXPECT_TRUE(CreateHostImpl(settings, std::move(frame_sink)));
  }
};

TEST_F(MsaaCompatibilityLayerTreeHostImplTest,
       GpuRasterizationStatusNonAAPaint) {
  // Always use a recording with slow paths so the toggle is dependent on non aa
  // paint.
  auto recording_source = FakeRecordingSource::CreateRecordingSource(
      gfx::Rect(100, 100), gfx::Size(100, 100));
  recording_source->set_has_slow_paths(true);

  // Ensure that without non-aa paint and without multisample compatibility, we
  // raster slow paths with msaa.
  CreateHostImplWithMultisampleCompatibility(false);
  recording_source->set_has_non_aa_paint(false);
  recording_source->Rerecord();
  EXPECT_TRUE(host_impl_->can_use_msaa());
  EXPECT_GT(host_impl_->GetMSAASampleCountForRaster(
                recording_source->GetDisplayItemList()),
            0);

  // Ensure that without non-aa paint and with multisample compatibility, we
  // raster slow paths with msaa.
  CreateHostImplWithMultisampleCompatibility(true);
  recording_source->set_has_non_aa_paint(false);
  recording_source->Rerecord();
  EXPECT_TRUE(host_impl_->can_use_msaa());
  EXPECT_GT(host_impl_->GetMSAASampleCountForRaster(
                recording_source->GetDisplayItemList()),
            0);

  // Ensure that with non-aa paint and without multisample compatibility, we do
  // not raster slow paths with msaa.
  CreateHostImplWithMultisampleCompatibility(false);
  recording_source->set_has_non_aa_paint(true);
  recording_source->Rerecord();
  EXPECT_TRUE(host_impl_->can_use_msaa());
  EXPECT_EQ(host_impl_->GetMSAASampleCountForRaster(
                recording_source->GetDisplayItemList()),
            0);

  // Ensure that with non-aa paint and with multisample compatibility, we raster
  // slow paths with msaa.
  CreateHostImplWithMultisampleCompatibility(true);
  recording_source->set_has_non_aa_paint(true);
  recording_source->Rerecord();
  EXPECT_TRUE(host_impl_->can_use_msaa());
  EXPECT_GT(host_impl_->GetMSAASampleCountForRaster(
                recording_source->GetDisplayItemList()),
            0);
}

TEST_F(LayerTreeHostImplTest, UpdatePageScaleFactorOnActiveTree) {
  // Check page scale factor update in property trees when an update is made
  // on the active tree.
  CreatePendingTree();
  host_impl_->pending_tree()->PushPageScaleFromMainThread(1, 1, 3);
  SetupViewportLayers(host_impl_->pending_tree(), gfx::Size(50, 50),
                      gfx::Size(100, 100), gfx::Size(100, 100));
  host_impl_->ActivateSyncTree();
  DrawFrame();

  CreatePendingTree();
  host_impl_->active_tree()->SetPageScaleOnActiveTree(2);

  TransformNode* active_tree_node =
      host_impl_->active_tree()->PageScaleTransformNode();
  // SetPageScaleOnActiveTree also updates the factors in property trees.
  EXPECT_TRUE(active_tree_node->local.IsScale2d());
  EXPECT_EQ(gfx::Vector2dF(2, 2), active_tree_node->local.Scale2d());
  EXPECT_EQ(gfx::Point3F(), active_tree_node->origin);
  EXPECT_EQ(2, host_impl_->active_tree()->current_page_scale_factor());

  TransformNode* pending_tree_node =
      host_impl_->pending_tree()->PageScaleTransformNode();
  // Before pending tree updates draw properties, its properties are still
  // based on 1.0 page scale, except for current_page_scale_factor() which is a
  // shared data between the active and pending trees.
  EXPECT_TRUE(pending_tree_node->local.IsIdentity());
  EXPECT_EQ(gfx::Point3F(), pending_tree_node->origin);
  EXPECT_EQ(2, host_impl_->pending_tree()->current_page_scale_factor());
  EXPECT_EQ(1, host_impl_->pending_tree()
                   ->property_trees()
                   ->transform_tree.page_scale_factor());

  host_impl_->pending_tree()->set_needs_update_draw_properties();
  UpdateDrawProperties(host_impl_->pending_tree());
  pending_tree_node = host_impl_->pending_tree()->PageScaleTransformNode();
  EXPECT_TRUE(pending_tree_node->local.IsScale2d());
  EXPECT_EQ(gfx::Vector2dF(2, 2), pending_tree_node->local.Scale2d());
  EXPECT_EQ(gfx::Point3F(), pending_tree_node->origin);

  host_impl_->ActivateSyncTree();
  UpdateDrawProperties(host_impl_->active_tree());
  active_tree_node = host_impl_->active_tree()->PageScaleTransformNode();
  EXPECT_TRUE(active_tree_node->local.IsScale2d());
  EXPECT_EQ(gfx::Vector2dF(2, 2), active_tree_node->local.Scale2d());
  EXPECT_EQ(gfx::Point3F(), active_tree_node->origin);
}

TEST_F(LayerTreeHostImplTest, SubLayerScaleForNodeInSubtreeOfPageScaleLayer) {
  // Checks that the sublayer scale of a transform node in the subtree of the
  // page scale layer is updated without a property tree rebuild.
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 3);
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  LayerImpl* in_subtree_of_page_scale_layer = AddLayer();
  CopyProperties(root_layer(), in_subtree_of_page_scale_layer);
  in_subtree_of_page_scale_layer->SetTransformTreeIndex(
      host_impl_->active_tree()->PageScaleTransformNode()->id);
  CreateEffectNode(in_subtree_of_page_scale_layer).render_surface_reason =
      RenderSurfaceReason::kTest;

  DrawFrame();

  EffectNode* node = GetEffectNode(in_subtree_of_page_scale_layer);
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(1, 1));

  host_impl_->active_tree()->SetPageScaleOnActiveTree(2);

  DrawFrame();

  node = GetEffectNode(in_subtree_of_page_scale_layer);
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(2, 2));
}

// Checks that if we lose a GPU raster enabled LayerTreeFrameSink and replace
// it with a software LayerTreeFrameSink, LayerTreeHostImpl correctly
// re-computes GPU rasterization status.
TEST_F(LayerTreeHostImplTest, RecomputeGpuRasterOnLayerTreeFrameSinkChange) {
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;

  host_impl_ = LayerTreeHostImpl::Create(
      DefaultSettings(), this, &task_runner_provider_, &stats_instrumentation_,
      &task_graph_runner_,
      AnimationHost::CreateForTesting(ThreadInstance::IMPL), 0, nullptr,
      nullptr);
  host_impl_->SetVisible(true);

  // InitializeFrameSink with a gpu-raster enabled output surface.
  auto gpu_raster_layer_tree_frame_sink =
      FakeLayerTreeFrameSink::Create3dForGpuRasterization();
  host_impl_->InitializeFrameSink(gpu_raster_layer_tree_frame_sink.get());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());

  // Re-initialize with a software output surface.
  layer_tree_frame_sink_ = FakeLayerTreeFrameSink::CreateSoftware();
  host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
  EXPECT_FALSE(host_impl_->use_gpu_rasterization());
}

void LayerTreeHostImplTest::SetupMouseMoveAtTestScrollbarStates(
    bool main_thread_scrolling) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(500);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  gfx::Size child_layer_size(250, 150);
  gfx::Size scrollbar_size_1(gfx::Size(15, viewport_size.height()));
  gfx::Size scrollbar_size_2(gfx::Size(15, child_layer_size.height()));

  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetDeviceScaleFactor(1);
  SetupViewportLayersInnerScrolls(viewport_size, content_size);
  LayerImpl* root_scroll = OuterViewportScrollLayer();

  if (main_thread_scrolling) {
    GetScrollNode(root_scroll)->main_thread_scrolling_reasons =
        MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  }

  // scrollbar_1 on root scroll.
  auto* scrollbar_1 = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), VERTICAL, 15, 0, true);
  SetupScrollbarLayer(root_scroll, scrollbar_1);
  scrollbar_1->SetBounds(scrollbar_size_1);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kNone, gfx::Rect(scrollbar_size_1));
  scrollbar_1->SetTouchActionRegion(touch_action_region);

  host_impl_->active_tree()->UpdateScrollbarGeometries();
  host_impl_->active_tree()->DidBecomeActive();

  DrawFrame();

  ScrollbarAnimationController* scrollbar_1_animation_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          root_scroll->element_id());
  EXPECT_TRUE(scrollbar_1_animation_controller);

  const float kMouseMoveDistanceToTriggerFadeIn =
      ScrollbarAnimationController::kMouseMoveDistanceToTriggerFadeIn;

  const float kMouseMoveDistanceToTriggerExpand =
      SingleScrollbarAnimationControllerThinning::
          kMouseMoveDistanceToTriggerExpand;

  // Mouse moves close to the scrollbar, goes over the scrollbar, and
  // moves back to where it was.
  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerFadeIn, 0));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(14 + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(gfx::Point(10, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(14 + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerFadeIn, 0));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  // scrollbar_2 on child.
  auto* scrollbar_2 = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), VERTICAL, 15, 0, true);
  LayerImpl* child =
      AddScrollableLayer(root_scroll, gfx::Size(100, 100), child_layer_size);
  child->SetOffsetToTransformParent(gfx::Vector2dF(50, 50));
  if (main_thread_scrolling) {
    GetScrollNode(child)->main_thread_scrolling_reasons =
        MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  }

  SetupScrollbarLayer(child, scrollbar_2);
  scrollbar_2->SetBounds(scrollbar_size_2);
  scrollbar_2->SetOffsetToTransformParent(child->offset_to_transform_parent());

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->UpdateScrollbarGeometries();
  host_impl_->active_tree()->DidBecomeActive();

  ScrollbarAnimationController* scrollbar_2_animation_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(child->element_id());
  EXPECT_TRUE(scrollbar_2_animation_controller);

  // Mouse goes over scrollbar_2, moves close to scrollbar_2, moves close to
  // scrollbar_1, goes over scrollbar_1.
  host_impl_->MouseMoveAt(gfx::Point(60, 60));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  EXPECT_TRUE(scrollbar_2_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_2_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_TRUE(
      scrollbar_2_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(64 + kMouseMoveDistanceToTriggerExpand, 50));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  EXPECT_TRUE(scrollbar_2_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_2_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  host_impl_->MouseMoveAt(
      gfx::Point(14 + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  host_impl_->MouseMoveAt(gfx::Point(10, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  // Capture scrollbar_1, then move mouse to scrollbar_2's layer);
  animation_task_.Reset();

  // Only the MouseMove's location will affect the overlay scrollbar.
  host_impl_->MouseDown(gfx::PointF(60, 50), /*shift_modifier*/ false);
  host_impl_->MouseMoveAt(gfx::Point(60, 50));
  host_impl_->MouseUp(gfx::PointF(60, 50));

  EXPECT_FALSE(animation_task_.is_null());

  // Near scrollbar_1, then mouse down and up, should not post an event to fade
  // out scrollbar_1.
  host_impl_->MouseMoveAt(gfx::Point(40, 150));
  animation_task_.Reset();

  host_impl_->MouseDown(gfx::PointF(40, 150), /*shift_modifier*/ false);
  host_impl_->MouseUp(gfx::PointF(40, 150));
  EXPECT_TRUE(animation_task_.is_null());

  // Near scrollbar_1, then mouse down and unregister
  // scrollbar_2_animation_controller, then mouse up should not cause crash.
  host_impl_->MouseMoveAt(gfx::Point(40, 150));
  host_impl_->MouseDown(gfx::PointF(40, 150), /*shift_modifier*/ false);
  host_impl_->DidUnregisterScrollbarLayer(root_scroll->element_id());
  host_impl_->MouseUp(gfx::PointF(40, 150));
}

TEST_F(LayerTreeHostImplTest,
       LayerTreeHostImplTestScrollbarStatesInMainThreadScrolling) {
  SetupMouseMoveAtTestScrollbarStates(true);
}

TEST_F(LayerTreeHostImplTest,
       LayerTreeHostImplTestScrollbarStatesInNotMainThreadScrolling) {
  SetupMouseMoveAtTestScrollbarStates(false);
}

TEST_F(LayerTreeHostImplTest, CheckerImagingTileInvalidation) {
  LayerTreeSettings settings = LegacySWSettings();
  settings.commit_to_active_tree = false;
  settings.enable_checker_imaging = true;
  settings.min_image_bytes_to_checker = 512 * 1024;
  settings.default_tile_size = gfx::Size(256, 256);
  settings.max_untiled_layer_size = gfx::Size(256, 256);
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  gfx::Size layer_size = gfx::Size(750, 750);

  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(layer_size);
  PaintImage checkerable_image =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(500, 500)))
          .set_decoding_mode(PaintImage::DecodingMode::kAsync)
          .TakePaintImage();
  recording_source->add_draw_image(checkerable_image, gfx::Point(0, 0));

  SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
  PaintFlags non_solid_flags;
  non_solid_flags.setColor(non_solid_color);
  recording_source->add_draw_rect_with_flags(gfx::Rect(510, 0, 200, 600),
                                             non_solid_flags);
  recording_source->add_draw_rect_with_flags(gfx::Rect(0, 510, 200, 400),
                                             non_solid_flags);
  recording_source->Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->WillBeginImplFrame(begin_frame_args);

  // Create the pending tree.
  host_impl_->BeginCommit();
  LayerTreeImpl* pending_tree = host_impl_->pending_tree();
  auto* root = SetupRootLayer<FakePictureLayerImpl>(pending_tree, layer_size,
                                                    raster_source);
  root->SetDrawsContent(true);
  UpdateDrawProperties(pending_tree);

  // Update the decoding state map for the tracker so it knows the correct
  // decoding preferences for the image.
  host_impl_->tile_manager()->checker_image_tracker().UpdateImageDecodingHints(
      raster_source->TakeDecodingModeMap());

  // CompleteCommit which should perform a PrepareTiles, adding tilings for the
  // root layer, each one having a raster task.
  host_impl_->CommitComplete();
  EXPECT_EQ(root->num_tilings(), 1U);
  const PictureLayerTiling* tiling = root->tilings()->tiling_at(0);
  EXPECT_EQ(tiling->AllTilesForTesting().size(), 9U);
  for (auto* tile : tiling->AllTilesForTesting())
    EXPECT_TRUE(tile->HasRasterTask());

  // Activate the pending tree and ensure that all tiles are rasterized.
  while (!did_notify_ready_to_activate_)
    base::RunLoop().RunUntilIdle();
  for (auto* tile : tiling->AllTilesForTesting())
    EXPECT_FALSE(tile->HasRasterTask());

  // PrepareTiles should have scheduled a decode with the ImageDecodeService,
  // ensure that it requests an impl-side invalidation.
  while (!did_request_impl_side_invalidation_)
    base::RunLoop().RunUntilIdle();

  // Invalidate content on impl-side and ensure that the correct tiles are
  // invalidated on the pending tree.
  host_impl_->InvalidateContentOnImplSide();
  pending_tree = host_impl_->pending_tree();
  root = static_cast<FakePictureLayerImpl*>(pending_tree->root_layer());
  for (auto* tile : root->tilings()->tiling_at(0)->AllTilesForTesting()) {
    if (tile->tiling_i_index() < 2 && tile->tiling_j_index() < 2)
      EXPECT_TRUE(tile->HasRasterTask());
    else
      EXPECT_FALSE(tile->HasRasterTask());
  }
  const auto expected_invalidation =
      ImageRectsToRegion(raster_source->GetDisplayItemList()
                             ->discardable_image_map()
                             .GetRectsForImage(checkerable_image.stable_id()));
  EXPECT_EQ(expected_invalidation, *(root->GetPendingInvalidation()));
}

TEST_F(LayerTreeHostImplTest, RasterColorSpace) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  // The default raster color space should be sRGB.
  EXPECT_EQ(
      host_impl_->GetRasterColorSpace(gfx::ContentColorUsage::kWideColorGamut),
      gfx::ColorSpace::CreateSRGB());
  // The raster color space should update with tree activation.
  host_impl_->active_tree()->SetRasterColorSpace(
      gfx::ColorSpace::CreateDisplayP3D65());
  EXPECT_EQ(
      host_impl_->GetRasterColorSpace(gfx::ContentColorUsage::kWideColorGamut),
      gfx::ColorSpace::CreateDisplayP3D65());
}

TEST_F(LayerTreeHostImplTest, RasterColorSpaceSoftware) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, FakeLayerTreeFrameSink::CreateSoftware());
  // Software composited resources should always use sRGB as their color space.
  EXPECT_EQ(
      host_impl_->GetRasterColorSpace(gfx::ContentColorUsage::kWideColorGamut),
      gfx::ColorSpace::CreateSRGB());
  host_impl_->active_tree()->SetRasterColorSpace(
      gfx::ColorSpace::CreateDisplayP3D65());
  EXPECT_EQ(
      host_impl_->GetRasterColorSpace(gfx::ContentColorUsage::kWideColorGamut),
      gfx::ColorSpace::CreateSRGB());
}

TEST_F(LayerTreeHostImplTest, RasterColorPrefersSRGB) {
  auto p3 = gfx::ColorSpace::CreateDisplayP3D65();

  LayerTreeSettings settings = DefaultSettings();
  settings.prefer_raster_in_srgb = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetRasterColorSpace(p3);

  EXPECT_EQ(host_impl_->GetRasterColorSpace(gfx::ContentColorUsage::kSRGB),
            gfx::ColorSpace::CreateSRGB());

  settings.prefer_raster_in_srgb = false;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetRasterColorSpace(p3);
  EXPECT_EQ(host_impl_->GetRasterColorSpace(gfx::ContentColorUsage::kSRGB), p3);
}

TEST_F(LayerTreeHostImplTest, UpdatedTilingsForNonDrawingLayers) {
  gfx::Size layer_bounds(500, 500);

  CreatePendingTree();
  auto* root =
      SetupRootLayer<LayerImpl>(host_impl_->pending_tree(), layer_bounds);

  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(layer_bounds));
  auto* animated_transform_layer =
      AddLayer<FakePictureLayerImpl>(host_impl_->pending_tree(), raster_source);
  animated_transform_layer->SetBounds(layer_bounds);
  animated_transform_layer->SetDrawsContent(true);

  host_impl_->pending_tree()->SetElementIdsForTesting();
  gfx::Transform singular;
  singular.Scale3d(6, 6, 0);
  CopyProperties(root, animated_transform_layer);
  CreateTransformNode(animated_transform_layer).local = singular;

  // A layer with a non-invertible transform is not drawn or rasterized. Since
  // this layer is not rasterized, we shouldn't be creating any tilings for it.
  UpdateDrawProperties(host_impl_->pending_tree());
  EXPECT_FALSE(animated_transform_layer->HasValidTilePriorities());
  EXPECT_EQ(animated_transform_layer->tilings()->num_tilings(), 0u);
  UpdateDrawProperties(host_impl_->pending_tree());
  EXPECT_FALSE(animated_transform_layer->raster_even_if_not_drawn());
  EXPECT_FALSE(animated_transform_layer->contributes_to_drawn_render_surface());
  EXPECT_EQ(animated_transform_layer->tilings()->num_tilings(), 0u);

  // Now add a transform animation to this layer. While we don't drawn layers
  // with non-invertible transforms, we still raster them if there is a
  // transform animation.
  TransformOperations start_transform_operations;
  start_transform_operations.AppendMatrix(singular);
  TransformOperations end_transform_operations;
  AddAnimatedTransformToElementWithAnimation(
      animated_transform_layer->element_id(), timeline(), 10.0,
      start_transform_operations, end_transform_operations);

  // The layer is still not drawn, but it will be rasterized. Since the layer is
  // rasterized, we should be creating tilings for it in UpdateDrawProperties.
  // However, none of these tiles should be required for activation.
  UpdateDrawProperties(host_impl_->pending_tree());
  EXPECT_TRUE(animated_transform_layer->raster_even_if_not_drawn());
  EXPECT_FALSE(animated_transform_layer->contributes_to_drawn_render_surface());
  ASSERT_EQ(animated_transform_layer->tilings()->num_tilings(), 1u);
  EXPECT_FALSE(animated_transform_layer->tilings()
                   ->tiling_at(0)
                   ->can_require_tiles_for_activation());
}

TEST_F(LayerTreeHostImplTest, RasterTilePrioritizationForNonDrawingLayers) {
  gfx::Size layer_bounds(500, 500);
  CreatePendingTree();
  auto* root =
      SetupRootLayer<LayerImpl>(host_impl_->pending_tree(), layer_bounds);
  root->SetBounds(layer_bounds);

  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(layer_bounds));

  auto* hidden_layer =
      AddLayer<FakePictureLayerImpl>(host_impl_->pending_tree(), raster_source);
  hidden_layer->SetBounds(layer_bounds);
  hidden_layer->SetDrawsContent(true);
  hidden_layer->set_contributes_to_drawn_render_surface(true);
  CopyProperties(root, hidden_layer);

  auto* drawing_layer =
      AddLayer<FakePictureLayerImpl>(host_impl_->pending_tree(), raster_source);
  drawing_layer->SetBounds(layer_bounds);
  drawing_layer->SetDrawsContent(true);
  drawing_layer->set_contributes_to_drawn_render_surface(true);
  CopyProperties(root, drawing_layer);

  gfx::Rect layer_rect(0, 0, 500, 500);

  hidden_layer->tilings()->AddTiling(gfx::AxisTransform2d(), raster_source);
  PictureLayerTiling* hidden_tiling = hidden_layer->tilings()->tiling_at(0);
  hidden_tiling->set_resolution(TileResolution::LOW_RESOLUTION);
  hidden_tiling->CreateAllTilesForTesting();
  hidden_tiling->SetTilePriorityRectsForTesting(
      layer_rect,   // Visible rect.
      layer_rect,   // Skewport rect.
      layer_rect,   // Soon rect.
      layer_rect);  // Eventually rect.

  drawing_layer->tilings()->AddTiling(gfx::AxisTransform2d(), raster_source);
  PictureLayerTiling* drawing_tiling = drawing_layer->tilings()->tiling_at(0);
  drawing_tiling->set_resolution(TileResolution::HIGH_RESOLUTION);
  drawing_tiling->CreateAllTilesForTesting();
  drawing_tiling->SetTilePriorityRectsForTesting(
      layer_rect,   // Visible rect.
      layer_rect,   // Skewport rect.
      layer_rect,   // Soon rect.
      layer_rect);  // Eventually rect.

  // Both layers are drawn. Since the hidden layer has a low resolution tiling,
  // in smoothness priority mode its tile is higher priority.
  std::unique_ptr<RasterTilePriorityQueue> queue =
      host_impl_->BuildRasterQueue(TreePriority::SMOOTHNESS_TAKES_PRIORITY,
                                   RasterTilePriorityQueue::Type::ALL);
  EXPECT_EQ(queue->Top().tile()->layer_id(), 2);

  // Hide the hidden layer and set it to so it still rasters. Now the drawing
  // layer should be prioritized over the hidden layer.
  hidden_layer->set_contributes_to_drawn_render_surface(false);
  hidden_layer->set_raster_even_if_not_drawn(true);
  queue = host_impl_->BuildRasterQueue(TreePriority::SMOOTHNESS_TAKES_PRIORITY,
                                       RasterTilePriorityQueue::Type::ALL);
  EXPECT_EQ(queue->Top().tile()->layer_id(), 3);
}

TEST_F(LayerTreeHostImplTest, DrawAfterDroppingTileResources) {
  LayerTreeSettings settings = DefaultSettings();
  settings.using_synchronous_renderer_compositor = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  CreatePendingTree();

  gfx::Size bounds(100, 100);
  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(bounds));
  auto* root = SetupRootLayer<FakePictureLayerImpl>(host_impl_->pending_tree(),
                                                    bounds, raster_source);
  root->SetDrawsContent(true);
  host_impl_->ActivateSyncTree();

  FakePictureLayerImpl* layer = static_cast<FakePictureLayerImpl*>(
      host_impl_->active_tree()->FindActiveTreeLayerById(root->id()));

  DrawFrame();
  EXPECT_FALSE(host_impl_->active_tree()->needs_update_draw_properties());
  EXPECT_LT(0, layer->raster_page_scale());
  EXPECT_GT(layer->tilings()->num_tilings(), 0u);

  const ManagedMemoryPolicy policy = host_impl_->ActualManagedMemoryPolicy();
  const ManagedMemoryPolicy zero_policy(0u);
  host_impl_->SetMemoryPolicy(zero_policy);
  EXPECT_EQ(0, layer->raster_page_scale());
  EXPECT_EQ(layer->tilings()->num_tilings(), 0u);

  host_impl_->SetMemoryPolicy(policy);
  DrawFrame();
  EXPECT_LT(0, layer->raster_page_scale());
  EXPECT_GT(layer->tilings()->num_tilings(), 0u);
}

TEST_F(LayerTreeHostImplTest, WhiteListedTouchActionTest1) {
  WhiteListedTouchActionTestHelper(1.0f, 1.0f);
}

TEST_F(LayerTreeHostImplTest, WhiteListedTouchActionTest2) {
  WhiteListedTouchActionTestHelper(1.0f, 0.789f);
}

TEST_F(LayerTreeHostImplTest, WhiteListedTouchActionTest3) {
  WhiteListedTouchActionTestHelper(2.345f, 1.0f);
}

TEST_F(LayerTreeHostImplTest, WhiteListedTouchActionTest4) {
  WhiteListedTouchActionTestHelper(2.654f, 0.678f);
}

// Test implementation of RenderFrameMetadataObserver which can optionally
// request the frame-token to be sent to the embedder during frame submission.
class TestRenderFrameMetadataObserver : public RenderFrameMetadataObserver {
 public:
  explicit TestRenderFrameMetadataObserver(bool increment_counter)
      : increment_counter_(increment_counter) {}
  TestRenderFrameMetadataObserver(const TestRenderFrameMetadataObserver&) =
      delete;
  ~TestRenderFrameMetadataObserver() override {}

  TestRenderFrameMetadataObserver& operator=(
      const TestRenderFrameMetadataObserver&) = delete;

  void BindToCurrentThread() override {}
  void OnRenderFrameSubmission(
      const RenderFrameMetadata& render_frame_metadata,
      viz::CompositorFrameMetadata* compositor_frame_metadata,
      bool force_send) override {
    if (increment_counter_)
      compositor_frame_metadata->send_frame_token_to_embedder = true;
    last_metadata_ = render_frame_metadata;
  }

  const base::Optional<RenderFrameMetadata>& last_metadata() const {
    return last_metadata_;
  }

 private:
  bool increment_counter_;
  base::Optional<RenderFrameMetadata> last_metadata_;
};

TEST_F(LayerTreeHostImplTest, RenderFrameMetadata) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(50, 50));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);

  {
    // Check initial metadata is correct.
    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();

    EXPECT_EQ(gfx::Vector2dF(), metadata.root_scroll_offset);
    EXPECT_EQ(1, metadata.page_scale_factor);

#if defined(OS_ANDROID)
    EXPECT_EQ(gfx::SizeF(50, 50), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
    EXPECT_EQ(4, metadata.max_page_scale_factor);
    EXPECT_EQ(gfx::SizeF(100, 100), metadata.root_layer_size);
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
#endif
  }

  // Scrolling should update metadata immediately.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel)
                .thread);
  host_impl_->ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                       ui::ScrollInputType::kWheel)
                               .get());
  {
    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0, 10), metadata.root_scroll_offset);
  }
  host_impl_->ScrollEnd();
  {
    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0, 10), metadata.root_scroll_offset);
  }

#if defined(OS_ANDROID)
  // Root "overflow: hidden" properties should be reflected on the outer
  // viewport scroll layer.
  {
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->OuterViewportScrollNode()->user_scrollable_horizontal = false;

    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
  }

  {
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->OuterViewportScrollNode()->user_scrollable_vertical = false;

    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_TRUE(metadata.root_overflow_y_hidden);
  }

  // Re-enable scrollability and verify that overflows are no longer
  // hidden.
  {
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->OuterViewportScrollNode()->user_scrollable_horizontal = true;
    host_impl_->OuterViewportScrollNode()->user_scrollable_vertical = true;

    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
  }

  // Root "overflow: hidden" properties should also be reflected on the
  // inner viewport scroll layer.
  {
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->OuterViewportScrollNode()->user_scrollable_horizontal = false;

    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
  }

  {
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->OuterViewportScrollNode()->user_scrollable_vertical = false;

    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_TRUE(metadata.root_overflow_y_hidden);
  }
#endif

  // Page scale should update metadata correctly (shrinking only the viewport).
  host_impl_->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, gfx::Point());
  host_impl_->PinchGestureEnd(gfx::Point(), true);
  host_impl_->ScrollEnd();
  {
    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();

    EXPECT_EQ(gfx::Vector2dF(0, 10), metadata.root_scroll_offset);
    EXPECT_EQ(2, metadata.page_scale_factor);

#if defined(OS_ANDROID)
    EXPECT_EQ(gfx::SizeF(25, 25), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
    EXPECT_EQ(4, metadata.max_page_scale_factor);
    EXPECT_EQ(gfx::SizeF(100, 100), metadata.root_layer_size);
#endif
  }

  // Likewise if set from the main thread.
  host_impl_->ProcessScrollDeltas();
  host_impl_->active_tree()->PushPageScaleFromMainThread(4, 0.5f, 4);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(4);
  {
    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();

    EXPECT_EQ(gfx::Vector2dF(0, 10), metadata.root_scroll_offset);
    EXPECT_EQ(4, metadata.page_scale_factor);

#if defined(OS_ANDROID)
    EXPECT_EQ(gfx::SizeF(12.5f, 12.5f), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
    EXPECT_EQ(4, metadata.max_page_scale_factor);
    EXPECT_EQ(gfx::SizeF(100, 100), metadata.root_layer_size);
#endif
  }
}

TEST_F(LayerTreeHostImplTest, SelectionBoundsPassedToRenderFrameMetadata) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

  auto observer = std::make_unique<TestRenderFrameMetadataObserver>(false);
  auto* observer_ptr = observer.get();
  host_impl_->SetRenderFrameObserver(std::move(observer));
  EXPECT_FALSE(observer_ptr->last_metadata());

  // Trigger a draw-swap sequence.
  host_impl_->SetNeedsRedraw();
  DrawFrame();

  // Ensure the selection bounds propagated to the render frame metadata
  // represent an empty selection.
  ASSERT_TRUE(observer_ptr->last_metadata());
  const viz::Selection<gfx::SelectionBound>& selection_1 =
      observer_ptr->last_metadata()->selection;
  EXPECT_EQ(gfx::SelectionBound::EMPTY, selection_1.start.type());
  EXPECT_EQ(gfx::SelectionBound::EMPTY, selection_1.end.type());
  EXPECT_EQ(gfx::PointF(), selection_1.start.edge_end());
  EXPECT_EQ(gfx::PointF(), selection_1.start.edge_start());
  EXPECT_FALSE(selection_1.start.visible());
  EXPECT_FALSE(selection_1.end.visible());

  // Plumb the layer-local selection bounds.
  gfx::Point selection_start(5, 0);
  gfx::Point selection_end(5, 5);
  LayerSelection selection;
  selection.start.type = gfx::SelectionBound::CENTER;
  selection.start.layer_id = root->id();
  selection.start.edge_end = selection_end;
  selection.start.edge_start = selection_start;
  selection.end = selection.start;
  host_impl_->active_tree()->RegisterSelection(selection);

  // Trigger a draw-swap sequence.
  host_impl_->SetNeedsRedraw();
  DrawFrame();

  // Ensure the selection bounds have propagated to the render frame metadata.
  ASSERT_TRUE(observer_ptr->last_metadata());
  const viz::Selection<gfx::SelectionBound>& selection_2 =
      observer_ptr->last_metadata()->selection;
  EXPECT_EQ(selection.start.type, selection_2.start.type());
  EXPECT_EQ(selection.end.type, selection_2.end.type());
  EXPECT_EQ(gfx::PointF(selection_end), selection_2.start.edge_end());
  EXPECT_EQ(gfx::PointF(selection_start), selection_2.start.edge_start());
  EXPECT_TRUE(selection_2.start.visible());
  EXPECT_TRUE(selection_2.end.visible());
}

TEST_F(LayerTreeHostImplTest,
       VerticalScrollDirectionChangesPassedToRenderFrameMetadata) {
  // Set up the viewport.
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(50, 50));

  // Set up the render frame metadata observer.
  auto observer = std::make_unique<TestRenderFrameMetadataObserver>(false);
  auto* observer_ptr = observer.get();
  host_impl_->SetRenderFrameObserver(std::move(observer));
  EXPECT_FALSE(observer_ptr->last_metadata());

  // Our test will be comprised of multiple legs, each leg consisting of a
  // distinct scroll event and an expectation regarding the vertical scroll
  // direction passed to the render frame metadata.
  typedef struct {
    gfx::Vector2d scroll_delta;
    viz::VerticalScrollDirection expected_vertical_scroll_direction;
  } TestLeg;

  std::vector<TestLeg> test_legs;

  // Initially, vertical scroll direction should be |kNull| indicating absence.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kNull});

  // Scrolling to the right should not affect vertical scroll direction.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(10, 0),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kNull});

  // After scrolling down, the vertical scroll direction should be |kDown|.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(0, 10),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kDown});

  // If we scroll down again, the vertical scroll direction should be |kNull| as
  // there was no change in vertical scroll direction.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(0, 10),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kNull});

  // Scrolling to the left should not affect last vertical scroll direction.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(-10, 0),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kNull});

  // After scrolling up, the vertical scroll direction should be |kUp|.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(0, -10),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kUp});

  // If we scroll up again, the vertical scroll direction should be |kNull| as
  // there was no change in vertical scroll direction.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(0, -10),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kNull});

  // Iterate over all legs of our test.
  for (auto& test_leg : test_legs) {
    // If the test leg contains a scroll, perform it.
    if (!test_leg.scroll_delta.IsZero()) {
      host_impl_->ScrollBegin(BeginState(gfx::Point(), test_leg.scroll_delta,
                                         ui::ScrollInputType::kWheel)
                                  .get(),
                              ui::ScrollInputType::kWheel);
      host_impl_->ScrollUpdate(UpdateState(gfx::Point(), test_leg.scroll_delta,
                                           ui::ScrollInputType::kWheel)
                                   .get());
    }

    // Trigger draw.
    host_impl_->SetNeedsRedraw();
    DrawFrame();

    // Assert our expectation regarding the vertical scroll direction.
    EXPECT_EQ(test_leg.expected_vertical_scroll_direction,
              observer_ptr->last_metadata()->new_vertical_scroll_direction);

    // If the test leg contains a scroll, end it.
    if (!test_leg.scroll_delta.IsZero())
      host_impl_->ScrollEnd();
  }
}

// Tests ScrollUpdate() to see if the method sets the scroll tree's currently
// scrolling node.
TEST_F(LayerTreeHostImplTest, ScrollUpdateDoesNotSetScrollingNode) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  UpdateDrawProperties(host_impl_->active_tree());

  ScrollStateData scroll_state_data;
  std::unique_ptr<ScrollState> scroll_state(new ScrollState(scroll_state_data));

  ScrollTree& scroll_tree =
      host_impl_->active_tree()->property_trees()->scroll_tree;

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                         ui::ScrollInputType::kTouchscreen)
                                  .get(),
                              ui::ScrollInputType::kTouchscreen)
                .thread);

  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  EXPECT_TRUE(scroll_node);

  host_impl_->ScrollUpdate(scroll_state.get());

  // Check to see the scroll tree's currently scrolling node is
  // still the same.
  EXPECT_EQ(scroll_node, scroll_tree.CurrentlyScrollingNode());

  // Set the scroll tree's currently scrolling node to null.
  host_impl_->active_tree()->SetCurrentlyScrollingNode(nullptr);
  EXPECT_FALSE(scroll_tree.CurrentlyScrollingNode());

  host_impl_->ScrollUpdate(scroll_state.get());

  EXPECT_EQ(nullptr, scroll_tree.CurrentlyScrollingNode());
}

class HitTestRegionListGeneratingLayerTreeHostImplTest
    : public LayerTreeHostImplTest {
 public:
  bool CreateHostImpl(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) override {
    // Enable hit test data generation with the CompositorFrame.
    LayerTreeSettings new_settings = settings;
    return LayerTreeHostImplTest::CreateHostImpl(
        new_settings, std::move(layer_tree_frame_sink));
  }
};

// Test to ensure that hit test data is created correctly from the active layer
// tree.
TEST_F(HitTestRegionListGeneratingLayerTreeHostImplTest, BuildHitTestData) {
  // Setup surface layers in LayerTreeHostImpl.
  host_impl_->CreatePendingTree();
  host_impl_->ActivateSyncTree();

  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---intermediate_layer (200, 300), 200x200
  // +-----surface_child1 (50, 50), 100x100, Rotate(45)
  // +---surface_child2 (450, 300), 100x100
  // +---overlapping_layer (500, 350), 200x200
  auto* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* intermediate_layer = AddLayer();
  auto* surface_child1 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());
  auto* surface_child2 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());
  auto* overlapping_layer = AddLayer();

  intermediate_layer->SetBounds(gfx::Size(200, 200));

  surface_child1->SetBounds(gfx::Size(100, 100));
  gfx::Transform rotate;
  rotate.Rotate(45);
  surface_child1->SetDrawsContent(true);
  surface_child1->SetHitTestable(true);
  surface_child1->SetSurfaceHitTestable(true);

  surface_child2->SetBounds(gfx::Size(100, 100));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetHitTestable(true);
  surface_child2->SetSurfaceHitTestable(true);

  overlapping_layer->SetBounds(gfx::Size(200, 200));
  overlapping_layer->SetDrawsContent(true);
  overlapping_layer->SetHitTestable(true);

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetRange(viz::SurfaceRange(base::nullopt, child_surface_id),
                           base::nullopt);
  surface_child2->SetRange(viz::SurfaceRange(base::nullopt, child_surface_id),
                           base::nullopt);

  CopyProperties(root, intermediate_layer);
  intermediate_layer->SetOffsetToTransformParent(gfx::Vector2dF(200, 300));
  CopyProperties(root, surface_child2);
  surface_child2->SetOffsetToTransformParent(gfx::Vector2dF(450, 300));
  CopyProperties(root, overlapping_layer);
  overlapping_layer->SetOffsetToTransformParent(gfx::Vector2dF(500, 350));

  CopyProperties(intermediate_layer, surface_child1);
  auto& surface_child1_transform_node = CreateTransformNode(surface_child1);
  // The post_translation includes offset of intermediate_layer.
  surface_child1_transform_node.post_translation = gfx::Vector2dF(250, 350);
  surface_child1_transform_node.local = rotate;

  UpdateDrawProperties(host_impl_->active_tree());
  draw_property_utils::ComputeEffects(
      &host_impl_->active_tree()->property_trees()->effect_tree);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  base::Optional<viz::HitTestRegionList> hit_test_region_list =
      host_impl_->BuildHitTestData();
  // Generating HitTestRegionList should have been enabled for this test.
  ASSERT_TRUE(hit_test_region_list);

  // Since surface_child2 draws in front of surface_child1, it should also be in
  // the front of the hit test region list.
  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(2u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[1].frame_sink_id);
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[1].flags);
  gfx::Transform child1_transform;
  child1_transform.Rotate(-45);
  child1_transform.Translate(-250, -350);
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[1].transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            hit_test_region_list->regions[1].rect.ToString());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface |
                   viz::HitTestRegionFlags::kHitTestAsk;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child2_transform;
  child2_transform.Translate(-450, -300);
  EXPECT_TRUE(child2_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            hit_test_region_list->regions[0].rect.ToString());
}

TEST_F(HitTestRegionListGeneratingLayerTreeHostImplTest, PointerEvents) {
  // Setup surface layers in LayerTreeHostImpl.
  host_impl_->CreatePendingTree();
  host_impl_->ActivateSyncTree();

  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---surface_child1 (0, 0), 100x100
  // +---overlapping_surface_child2 (50, 50), 100x100, pointer-events: none,
  // does not generate hit test region
  auto* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* surface_child1 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());
  auto* surface_child2 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());

  surface_child1->SetBounds(gfx::Size(100, 100));
  surface_child1->SetDrawsContent(true);
  surface_child1->SetHitTestable(true);
  surface_child1->SetSurfaceHitTestable(true);
  surface_child1->SetHasPointerEventsNone(false);
  CopyProperties(root, surface_child1);

  surface_child2->SetBounds(gfx::Size(100, 100));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetHitTestable(true);
  surface_child2->SetSurfaceHitTestable(false);
  surface_child2->SetHasPointerEventsNone(true);
  CopyProperties(root, surface_child2);
  surface_child2->SetOffsetToTransformParent(gfx::Vector2dF(50, 50));

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetRange(viz::SurfaceRange(base::nullopt, child_surface_id),
                           base::nullopt);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  UpdateDrawProperties(host_impl_->active_tree());
  base::Optional<viz::HitTestRegionList> hit_test_region_list =
      host_impl_->BuildHitTestData();
  // Generating HitTestRegionList should have been enabled for this test.
  ASSERT_TRUE(hit_test_region_list);

  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  // Since |surface_child2| is not |surface_hit_testable|, it does not
  // contribute to a hit test region. Although it overlaps |surface_child1|, it
  // does not make |surface_child1| kHitTestAsk because it has pointer-events
  // none property.
  EXPECT_EQ(1u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child1_transform;
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            hit_test_region_list->regions[0].rect.ToString());
}

TEST_F(HitTestRegionListGeneratingLayerTreeHostImplTest, ComplexPage) {
  // Setup surface layers in LayerTreeHostImpl.
  host_impl_->CreatePendingTree();
  host_impl_->ActivateSyncTree();

  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---surface_child (0, 0), 100x100
  // +---100x non overlapping layers (110, 110), 1x1
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* surface_child = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());

  surface_child->SetBounds(gfx::Size(100, 100));
  surface_child->SetDrawsContent(true);
  surface_child->SetHitTestable(true);
  surface_child->SetSurfaceHitTestable(true);
  surface_child->SetHasPointerEventsNone(false);

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child->SetRange(viz::SurfaceRange(base::nullopt, child_surface_id),
                          base::nullopt);

  CopyProperties(root, surface_child);

  // Create 101 non overlapping layers.
  for (size_t i = 0; i <= 100; ++i) {
    LayerImpl* layer = AddLayer();
    layer->SetBounds(gfx::Size(1, 1));
    layer->SetDrawsContent(true);
    layer->SetHitTestable(true);
    CopyProperties(root, layer);
  }

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  UpdateDrawProperties(host_impl_->active_tree());
  base::Optional<viz::HitTestRegionList> hit_test_region_list =
      host_impl_->BuildHitTestData();
  // Generating HitTestRegionList should have been enabled for this test.
  ASSERT_TRUE(hit_test_region_list);

  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  // Since the layer count is greater than 100, in order to save time, we do not
  // check whether each layer overlaps the surface layer, instead, we are being
  // conservative and make the surface layer slow hit testing.
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface |
                   viz::HitTestRegionFlags::kHitTestAsk;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child1_transform;
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            hit_test_region_list->regions[0].rect.ToString());
}

TEST_F(HitTestRegionListGeneratingLayerTreeHostImplTest, InvalidFrameSinkId) {
  // Setup surface layers in LayerTreeHostImpl.
  host_impl_->CreatePendingTree();
  host_impl_->ActivateSyncTree();

  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---surface_child1 (0, 0), 100x100
  // +---surface_child2 (0, 0), 50x50, frame_sink_id = (0, 0)
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* surface_child1 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());

  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(1024, 768));

  surface_child1->SetBounds(gfx::Size(100, 100));
  surface_child1->SetDrawsContent(true);
  surface_child1->SetHitTestable(true);
  surface_child1->SetSurfaceHitTestable(true);
  surface_child1->SetHasPointerEventsNone(false);
  CopyProperties(root, surface_child1);

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetRange(viz::SurfaceRange(base::nullopt, child_surface_id),
                           base::nullopt);

  auto* surface_child2 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());

  surface_child2->SetBounds(gfx::Size(50, 50));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetHitTestable(true);
  surface_child2->SetSurfaceHitTestable(true);
  surface_child2->SetHasPointerEventsNone(false);
  CopyProperties(root, surface_child2);

  surface_child2->SetRange(viz::SurfaceRange(base::nullopt, viz::SurfaceId()),
                           base::nullopt);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  UpdateDrawProperties(host_impl_->active_tree());
  base::Optional<viz::HitTestRegionList> hit_test_region_list =
      host_impl_->BuildHitTestData();
  // Generating HitTestRegionList should have been enabled for this test.
  ASSERT_TRUE(hit_test_region_list);

  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  // We do not populate hit test region for a surface layer with invalid frame
  // sink id to avoid deserialization failure. Instead we make the overlapping
  // hit test region kHitTestAsk.
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface |
                   viz::HitTestRegionFlags::kHitTestAsk;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child1_transform;
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            hit_test_region_list->regions[0].rect.ToString());
}

TEST_F(LayerTreeHostImplTest, ImplThreadPhaseUponImplSideInvalidation) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  // In general invalidation should never be ran outside the impl frame.
  auto args = viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->WillBeginImplFrame(args);
  // Expect no crash because the operation is within an impl frame.
  host_impl_->InvalidateContentOnImplSide();

  // Once the impl frame is finished the impl thread phase is set to IDLE.
  host_impl_->DidFinishImplFrame(args);

  settings.using_synchronous_renderer_compositor = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  // Expect no crash when using synchronous renderer compositor regardless the
  // impl thread phase.
  host_impl_->InvalidateContentOnImplSide();

  // Test passes when there is no crash.
}

TEST_F(LayerTreeHostImplTest, SkipOnDrawDoesNotUpdateDrawParams) {
  EXPECT_TRUE(CreateHostImpl(DefaultSettings(),
                             FakeLayerTreeFrameSink::CreateSoftware()));
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* layer = InnerViewportScrollLayer();
  layer->SetDrawsContent(true);
  gfx::Transform transform;
  transform.Translate(20, 20);
  gfx::Rect viewport(0, 0, 50, 50);
  const bool resourceless_software_draw = false;

  bool skip_draw = false;
  host_impl_->OnDraw(transform, viewport, resourceless_software_draw,
                     skip_draw);
  EXPECT_EQ(transform, host_impl_->DrawTransform());
  EXPECT_EQ(viewport, host_impl_->active_tree()->GetDeviceViewport());

  skip_draw = true;
  gfx::Transform new_transform;
  gfx::Rect new_viewport;
  host_impl_->OnDraw(new_transform, new_viewport, resourceless_software_draw,
                     skip_draw);
  EXPECT_EQ(transform, host_impl_->DrawTransform());
  EXPECT_EQ(viewport, host_impl_->active_tree()->GetDeviceViewport());
}

// Test that a touch scroll over a SolidColorScrollbarLayer, the scrollbar used
// on Android, does not register as a scrollbar scroll and result in main
// threaded scrolling.
TEST_F(LayerTreeHostImplTest, TouchScrollOnAndroidScrollbar) {
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  gfx::Size viewport_size = gfx::Size(360, 600);
  gfx::Size scroll_content_size = gfx::Size(360, 3800);
  gfx::Size scrollbar_size = gfx::Size(15, 600);

  SetupViewportLayersNoScrolls(viewport_size);
  LayerImpl* content = AddScrollableLayer(OuterViewportScrollLayer(),
                                          viewport_size, scroll_content_size);

  auto* scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      layer_tree_impl, VERTICAL, 10, 0, false);
  SetupScrollbarLayer(content, scrollbar);
  scrollbar->SetBounds(scrollbar_size);
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  UpdateDrawProperties(layer_tree_impl);
  layer_tree_impl->DidBecomeActive();

  // Do a scroll over the scrollbar layer as well as the content layer, which
  // should result in scrolling the scroll layer on the impl thread as the
  // scrollbar should not be hit.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(350, 50), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       CommitWithNoPaintWorkletLayerPainter) {
  ASSERT_FALSE(host_impl_->GetPaintWorkletLayerPainterForTesting());
  host_impl_->CreatePendingTree();

  // When there is no PaintWorkletLayerPainter registered, commits should finish
  // immediately and move onto preparing tiles.
  ASSERT_FALSE(did_prepare_tiles_);
  host_impl_->CommitComplete();
  EXPECT_TRUE(did_prepare_tiles_);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest, CommitWithNoPaintWorklets) {
  host_impl_->SetPaintWorkletLayerPainter(
      std::make_unique<TestPaintWorkletLayerPainter>());
  host_impl_->CreatePendingTree();

  // When there are no PaintWorklets in the committed display lists, commits
  // should finish immediately and move onto preparing tiles.
  ASSERT_FALSE(did_prepare_tiles_);
  host_impl_->CommitComplete();
  EXPECT_TRUE(did_prepare_tiles_);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest, CommitWithDirtyPaintWorklets) {
  auto painter_owned = std::make_unique<TestPaintWorkletLayerPainter>();
  TestPaintWorkletLayerPainter* painter = painter_owned.get();
  host_impl_->SetPaintWorkletLayerPainter(std::move(painter_owned));

  // Setup the pending tree with a PictureLayerImpl that will contain
  // PaintWorklets.
  host_impl_->CreatePendingTree();
  auto* root = SetupRootLayer<PictureLayerImpl>(host_impl_->pending_tree(),
                                                gfx::Size(100, 100));
  root->SetNeedsPushProperties();

  // Add a PaintWorkletInput to the PictureLayerImpl.
  scoped_refptr<RasterSource> raster_source_with_pws(
      FakeRasterSource::CreateFilledWithPaintWorklet(root->bounds()));
  Region empty_invalidation;
  root->UpdateRasterSource(raster_source_with_pws, &empty_invalidation, nullptr,
                           nullptr);

  UpdateDrawProperties(host_impl_->pending_tree());

  // Since we have dirty PaintWorklets, committing will not cause tile
  // preparation to happen. Instead, it will be delayed until the callback
  // passed to the PaintWorkletLayerPainter is called.
  did_prepare_tiles_ = false;
  host_impl_->CommitComplete();
  EXPECT_FALSE(did_prepare_tiles_);

  // Set up a result to have been 'painted'.
  ASSERT_EQ(root->GetPaintWorkletRecordMap().size(), 1u);
  scoped_refptr<const PaintWorkletInput> input =
      root->GetPaintWorkletRecordMap().begin()->first;
  int worklet_id = input->WorkletId();

  PaintWorkletJob painted_job(worklet_id, input, {});
  sk_sp<PaintRecord> record = sk_make_sp<PaintRecord>();
  painted_job.SetOutput(record);

  auto painted_job_vector = base::MakeRefCounted<PaintWorkletJobVector>();
  painted_job_vector->data.push_back(std::move(painted_job));
  PaintWorkletJobMap painted_job_map;
  painted_job_map[worklet_id] = std::move(painted_job_vector);

  // Finally, 'paint' the content. This should unlock tile preparation and
  // update the PictureLayerImpl's map.
  std::move(painter->TakeDoneCallback()).Run(std::move(painted_job_map));
  EXPECT_EQ(root->GetPaintWorkletRecordMap().find(input)->second.second,
            record);
  EXPECT_TRUE(did_prepare_tiles_);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       CommitWithNoDirtyPaintWorklets) {
  host_impl_->SetPaintWorkletLayerPainter(
      std::make_unique<TestPaintWorkletLayerPainter>());

  host_impl_->CreatePendingTree();
  auto* root = SetupRootLayer<PictureLayerImpl>(host_impl_->pending_tree(),
                                                gfx::Size(100, 100));
  root->SetNeedsPushProperties();

  // Add some PaintWorklets.
  scoped_refptr<RasterSource> raster_source_with_pws(
      FakeRasterSource::CreateFilledWithPaintWorklet(root->bounds()));
  Region empty_invalidation;
  root->UpdateRasterSource(raster_source_with_pws, &empty_invalidation, nullptr,
                           nullptr);

  UpdateDrawProperties(host_impl_->pending_tree());

  // Pretend that our worklets were already painted.
  ASSERT_EQ(root->GetPaintWorkletRecordMap().size(), 1u);
  root->SetPaintWorkletRecord(root->GetPaintWorkletRecordMap().begin()->first,
                              sk_make_sp<PaintRecord>());

  // Since there are no dirty PaintWorklets, the commit should immediately
  // prepare tiles.
  ASSERT_FALSE(did_prepare_tiles_);
  host_impl_->CommitComplete();
  EXPECT_TRUE(did_prepare_tiles_);
}

class ForceActivateAfterPaintWorkletPaintLayerTreeHostImplTest
    : public CommitToPendingTreeLayerTreeHostImplTest {
 public:
  void NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState state) override {
    if (state == Scheduler::PaintWorkletState::IDLE) {
      // Pretend a force activation happened.
      host_impl_->ActivateSyncTree();
      ASSERT_FALSE(host_impl_->pending_tree());
    }
  }
};

TEST_F(ForceActivateAfterPaintWorkletPaintLayerTreeHostImplTest,
       ForceActivationAfterPaintWorkletsFinishPainting) {
  auto painter_owned = std::make_unique<TestPaintWorkletLayerPainter>();
  TestPaintWorkletLayerPainter* painter = painter_owned.get();
  host_impl_->SetPaintWorkletLayerPainter(std::move(painter_owned));

  // Setup the pending tree with a PictureLayerImpl that will contain
  // PaintWorklets.
  host_impl_->CreatePendingTree();
  auto* root = SetupRootLayer<PictureLayerImpl>(host_impl_->pending_tree(),
                                                gfx::Size(100, 100));
  root->SetNeedsPushProperties();

  // Add a PaintWorkletInput to the PictureLayerImpl.
  scoped_refptr<RasterSource> raster_source_with_pws(
      FakeRasterSource::CreateFilledWithPaintWorklet(root->bounds()));
  Region empty_invalidation;
  root->UpdateRasterSource(raster_source_with_pws, &empty_invalidation, nullptr,
                           nullptr);

  UpdateDrawProperties(host_impl_->pending_tree());

  // Since we have dirty PaintWorklets, committing will not cause tile
  // preparation to happen. Instead, it will be delayed until the callback
  // passed to the PaintWorkletLayerPainter is called.
  did_prepare_tiles_ = false;
  host_impl_->CommitComplete();
  EXPECT_FALSE(did_prepare_tiles_);

  // Set up a result to have been 'painted'.
  ASSERT_EQ(root->GetPaintWorkletRecordMap().size(), 1u);
  scoped_refptr<const PaintWorkletInput> input =
      root->GetPaintWorkletRecordMap().begin()->first;
  int worklet_id = input->WorkletId();

  PaintWorkletJob painted_job(worklet_id, input, {});
  sk_sp<PaintRecord> record = sk_make_sp<PaintRecord>();
  painted_job.SetOutput(record);

  auto painted_job_vector = base::MakeRefCounted<PaintWorkletJobVector>();
  painted_job_vector->data.push_back(std::move(painted_job));
  PaintWorkletJobMap painted_job_map;
  painted_job_map[worklet_id] = std::move(painted_job_vector);

  // Finally, 'paint' the content. The test class causes a forced activation
  // during NotifyPaintWorkletStateChange. The PictureLayerImpl should still be
  // updated, but since the tree was force activated there should be no tile
  // preparation.
  std::move(painter->TakeDoneCallback()).Run(std::move(painted_job_map));
  EXPECT_EQ(root->GetPaintWorkletRecordMap().find(input)->second.second,
            record);
  EXPECT_FALSE(did_prepare_tiles_);
}

// Verify that the device scale factor is not used to rescale scrollbar deltas
// in percent-based scrolling.
TEST_F(LayerTreeHostImplTest, PercentBasedScrollbarDeltasDSF3) {
  LayerTreeSettings settings = DefaultSettings();
  settings.percent_based_scrolling = true;
  settings.use_zoom_for_dsf = true;
  settings.use_painted_device_scale_factor = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  const gfx::Size viewport_size = gfx::Size(800, 800);
  SetupViewportLayersOuterScrolls(viewport_size, viewport_size);

  LayerImpl* content_layer = AddContentLayer();
  LayerImpl* scroll_layer = AddScrollableLayer(
      content_layer, gfx::Size(185, 200), gfx::Size(185, 3800));

  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      host_impl_->active_tree(), VERTICAL, false, true);
  SetupScrollbarLayer(scroll_layer, scrollbar);

  scrollbar->SetBounds(gfx::Size(15, 200));

  scrollbar->SetThumbThickness(15);
  scrollbar->SetThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 185));

  scrollbar->SetBackButtonRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(0, 185), gfx::Size(15, 15)));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(185, 0));

  DrawFrame();

  TestInputHandlerClient input_handler_client;
  host_impl_->BindToClient(&input_handler_client);

  // Test scrolling with device scale factor = 3.
  const float expected_delta = kPercentDeltaForDirectionalScroll * 200u;

  host_impl_->active_tree()->set_painted_device_scale_factor(3);

  InputHandlerPointerResult scroll_result =
      host_impl_->MouseDown(gfx::PointF(190, 190), false);
  host_impl_->MouseUp(gfx::PointF(190, 190));

  EXPECT_EQ(scroll_result.scroll_offset.y(), expected_delta);
  EXPECT_FALSE(GetScrollNode(scroll_layer)->main_thread_scrolling_reasons);

  // Test with DSF = 1. As the scrollable layers aren't rescaled by the DSF,
  // neither the scroll offset, the same result for DSF = 3 is expected.
  host_impl_->active_tree()->set_painted_device_scale_factor(1);

  InputHandlerPointerResult scroll_with_dsf_1 =
      host_impl_->MouseDown(gfx::PointF(190, 190), false);
  host_impl_->MouseUp(gfx::PointF(190, 190));

  EXPECT_EQ(scroll_with_dsf_1.scroll_offset.y(), expected_delta);
  EXPECT_FALSE(GetScrollNode(scroll_layer)->main_thread_scrolling_reasons);

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

}  // namespace
}  // namespace cc
