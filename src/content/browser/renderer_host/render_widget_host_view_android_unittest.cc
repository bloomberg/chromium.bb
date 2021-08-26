// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_android.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "cc/layers/deadline_policy.h"
#include "cc/layers/layer.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/mock_render_widget_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_render_widget_host_delegate.h"
#include "content/test/test_view_android_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

namespace content {

class RenderWidgetHostViewAndroidTest : public testing::Test {
 public:
  RenderWidgetHostViewAndroidTest();
  ~RenderWidgetHostViewAndroidTest() override {}

  RenderWidgetHostViewAndroid* render_widget_host_view_android() {
    return render_widget_host_view_android_;
  }

  MockRenderWidgetHostDelegate* delegate() { return delegate_.get(); }

  // Directly map to RenderWidgetHostViewAndroid methods.
  bool SynchronizeVisualProperties(
      const cc::DeadlinePolicy& deadline_policy,
      const absl::optional<viz::LocalSurfaceId>& child_local_surface_id);
  void WasEvicted();
  ui::ViewAndroid* GetViewAndroid() { return &native_view_; }

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  ui::ViewAndroid* parent_view() { return &parent_view_; }

  std::unique_ptr<TestViewAndroidDelegate> test_view_android_delegate_;

 private:
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> process_;
  std::unique_ptr<AgentSchedulingGroupHost> agent_scheduling_group_;
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;
  scoped_refptr<cc::Layer> parent_layer_;
  scoped_refptr<cc::Layer> layer_;
  ui::ViewAndroid parent_view_;
  ui::ViewAndroid native_view_;
  std::unique_ptr<MockRenderWidgetHost> host_;
  RenderWidgetHostViewAndroid* render_widget_host_view_android_;

  BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAndroidTest);
};

RenderWidgetHostViewAndroidTest::RenderWidgetHostViewAndroidTest()
    : parent_view_(ui::ViewAndroid::LayoutType::NORMAL),
      native_view_(ui::ViewAndroid::LayoutType::NORMAL) {}

bool RenderWidgetHostViewAndroidTest::SynchronizeVisualProperties(
    const cc::DeadlinePolicy& deadline_policy,
    const absl::optional<viz::LocalSurfaceId>& child_local_surface_id) {
  return render_widget_host_view_android_->SynchronizeVisualProperties(
      deadline_policy, child_local_surface_id);
}

void RenderWidgetHostViewAndroidTest::WasEvicted() {
  render_widget_host_view_android_->WasEvicted();
}

void RenderWidgetHostViewAndroidTest::SetUp() {
  browser_context_ = std::make_unique<TestBrowserContext>();
  delegate_ = std::make_unique<MockRenderWidgetHostDelegate>();
  process_ = std::make_unique<MockRenderProcessHost>(browser_context_.get());
  agent_scheduling_group_ =
      std::make_unique<AgentSchedulingGroupHost>(*process_);
  host_ = MockRenderWidgetHost::Create(/*frame_tree=*/nullptr, delegate_.get(),
                                       *agent_scheduling_group_,
                                       process_->GetNextRoutingID());
  parent_layer_ = cc::Layer::Create();
  parent_view_.SetLayer(parent_layer_);
  layer_ = cc::Layer::Create();
  native_view_.SetLayer(layer_);
  parent_view_.AddChild(&native_view_);
  EXPECT_EQ(&parent_view_, native_view_.parent());
  render_widget_host_view_android_ =
      new RenderWidgetHostViewAndroid(host_.get(), &native_view_);
  test_view_android_delegate_ = std::make_unique<TestViewAndroidDelegate>();
}

void RenderWidgetHostViewAndroidTest::TearDown() {
  render_widget_host_view_android_->Destroy();
  host_.reset();
  delegate_.reset();
  process_->Cleanup();
  agent_scheduling_group_ = nullptr;
  process_ = nullptr;
  browser_context_.reset();
}

// Tests that when a child responds to a Surface Synchronization message, while
// we are evicted, that we do not attempt to embed an invalid
// viz::LocalSurfaceId. This test should not crash.
TEST_F(RenderWidgetHostViewAndroidTest, NoSurfaceSynchronizationWhileEvicted) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // Evicting while hidden should invalidate the current viz::LocalSurfaceId.
  rwhva->Hide();
  EXPECT_FALSE(rwhva->IsShowing());
  WasEvicted();
  EXPECT_FALSE(rwhva->GetLocalSurfaceId().is_valid());

  // When a child acknowledges a Surface Synchronization message, and has no new
  // properties to change, it responds with the original viz::LocalSurfaceId.
  // If we are evicted, we should not attempt to embed our invalid id. Nor
  // should we continue the synchronization process. This should not cause a
  // crash in DelegatedFrameHostAndroid.
  EXPECT_FALSE(SynchronizeVisualProperties(
      cc::DeadlinePolicy::UseDefaultDeadline(), initial_local_surface_id));
}

// Tests insetting the Visual Viewport.
TEST_F(RenderWidgetHostViewAndroidTest, InsetVisualViewport) {
  // Android default viewport should not have an inset bottom.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_EQ(0, GetViewAndroid()->GetViewportInsetBottom());

  // Set up SurfaceId checking.
  const viz::LocalSurfaceId original_local_surface_id =
      rwhva->GetLocalSurfaceId();

  // Set up our test delegate connected to this ViewAndroid.
  test_view_android_delegate_->SetupTestDelegate(GetViewAndroid());
  EXPECT_EQ(0, GetViewAndroid()->GetViewportInsetBottom());

  JNIEnv* env = base::android::AttachCurrentThread();

  // Now inset the bottom and make sure the surface changes, and the inset is
  // known to our ViewAndroid.
  test_view_android_delegate_->InsetViewportBottom(100);
  EXPECT_EQ(100, GetViewAndroid()->GetViewportInsetBottom());
  rwhva->OnViewportInsetBottomChanged(env, nullptr);
  viz::LocalSurfaceId inset_surface = rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(inset_surface.IsNewerThan(original_local_surface_id));

  // Reset the bottom; should go back to the original inset and have a new
  // surface.
  test_view_android_delegate_->InsetViewportBottom(0);
  rwhva->OnViewportInsetBottomChanged(env, nullptr);
  EXPECT_EQ(0, GetViewAndroid()->GetViewportInsetBottom());
  EXPECT_TRUE(rwhva->GetLocalSurfaceId().IsNewerThan(inset_surface));
}

TEST_F(RenderWidgetHostViewAndroidTest, HideWindowRemoveViewAddViewShowWindow) {
  std::unique_ptr<ui::WindowAndroid> window(
      ui::WindowAndroid::CreateForTesting());
  window->AddChild(parent_view());
  EXPECT_TRUE(render_widget_host_view_android()->IsShowing());
  // The layer should be visible once attached to a window.
  EXPECT_FALSE(render_widget_host_view_android()
                   ->GetNativeView()
                   ->GetLayer()
                   ->hide_layer_and_subtree());

  // Hiding the window should and removing the view should hide the layer.
  window->OnVisibilityChanged(nullptr, nullptr, false);
  parent_view()->RemoveFromParent();
  EXPECT_TRUE(render_widget_host_view_android()->IsShowing());
  EXPECT_TRUE(render_widget_host_view_android()
                  ->GetNativeView()
                  ->GetLayer()
                  ->hide_layer_and_subtree());

  // Adding the view back to a window and notifying the window is visible should
  // make the layer visible again.
  window->AddChild(parent_view());
  window->OnVisibilityChanged(nullptr, nullptr, true);
  EXPECT_TRUE(render_widget_host_view_android()->IsShowing());
  EXPECT_FALSE(render_widget_host_view_android()
                   ->GetNativeView()
                   ->GetLayer()
                   ->hide_layer_and_subtree());
}

TEST_F(RenderWidgetHostViewAndroidTest, DisplayFeature) {
  // By default there is no display feature so verify we get back null.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  RenderWidgetHostViewBase* rwhv = rwhva;
  rwhva->GetNativeView()->SetLayoutForTesting(0, 0, 200, 400);
  test_view_android_delegate_->SetupTestDelegate(GetViewAndroid());
  EXPECT_EQ(absl::nullopt, rwhv->GetDisplayFeature());

  // Set a vertical display feature, and verify this is reflected in the
  // computed display feature.
  test_view_android_delegate_->SetDisplayFeatureForTesting(
      gfx::Rect(95, 0, 10, 400));
  DisplayFeature expected_display_feature = {
      DisplayFeature::Orientation::kVertical,
      /* offset */ 95,
      /* mask_length */ 10};
  EXPECT_EQ(expected_display_feature, *rwhv->GetDisplayFeature());

  // Validate that a display feature in the middle of the view results in not
  // being exposed as a content::DisplayFeature (we currently only consider
  // display features that completely cover one of the view's dimensions).
  rwhva->GetNativeView()->SetLayoutForTesting(0, 0, 400, 200);
  test_view_android_delegate_->SetDisplayFeatureForTesting(
      gfx::Rect(200, 100, 100, 200));
  EXPECT_EQ(absl::nullopt, rwhv->GetDisplayFeature());

  // Verify that horizontal display feature is correctly validated.
  test_view_android_delegate_->SetDisplayFeatureForTesting(
      gfx::Rect(0, 90, 400, 20));
  expected_display_feature = {DisplayFeature::Orientation::kHorizontal,
                              /* offset */ 90,
                              /* mask_length */ 20};
  EXPECT_EQ(expected_display_feature, *rwhv->GetDisplayFeature());

  test_view_android_delegate_->SetDisplayFeatureForTesting(
      gfx::Rect(0, 95, 600, 10));
  expected_display_feature = {DisplayFeature::Orientation::kHorizontal,
                              /* offset */ 95,
                              /* mask_length */ 10};
  EXPECT_EQ(expected_display_feature, *rwhv->GetDisplayFeature());

  test_view_android_delegate_->SetDisplayFeatureForTesting(
      gfx::Rect(195, 0, 10, 300));
  expected_display_feature = {DisplayFeature::Orientation::kVertical,
                              /* offset */ 195,
                              /* mask_length */ 10};
  EXPECT_EQ(expected_display_feature, *rwhv->GetDisplayFeature());
}

// Tests Rotation improvements that are behind the
// features::kSurfaceSyncThrottling flag.
class RenderWidgetHostViewAndroidRotationTest
    : public RenderWidgetHostViewAndroidTest {
 public:
  RenderWidgetHostViewAndroidRotationTest();
  ~RenderWidgetHostViewAndroidRotationTest() override {}

  void OnDidUpdateVisualPropertiesComplete(
      const cc::RenderFrameMetadata& metadata);
  void OnRenderFrameMetadataChangedAfterActivation(
      cc::RenderFrameMetadata metadata,
      base::TimeTicks activation_time);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

RenderWidgetHostViewAndroidRotationTest::
    RenderWidgetHostViewAndroidRotationTest() {
  scoped_feature_list_.InitAndEnableFeature(features::kSurfaceSyncThrottling);
}

void RenderWidgetHostViewAndroidRotationTest::
    OnDidUpdateVisualPropertiesComplete(
        const cc::RenderFrameMetadata& metadata) {
  render_widget_host_view_android()->OnDidUpdateVisualPropertiesComplete(
      metadata);
}

void RenderWidgetHostViewAndroidRotationTest::
    OnRenderFrameMetadataChangedAfterActivation(
        cc::RenderFrameMetadata metadata,
        base::TimeTicks activation_time) {
  render_widget_host_view_android()
      ->host()
      ->render_frame_metadata_provider()
      ->OnRenderFrameMetadataChangedAfterActivation(metadata, activation_time);
}

// Tests that when a rotation occurs, that we only advance the
// viz::LocalSurfaceId once, and that no other visual changes occurring during
// this time can separately trigger SurfaceSync. (https://crbug.com/1203804)
TEST_F(RenderWidgetHostViewAndroidRotationTest,
       RotationOnlyAdvancesSurfaceSyncOnce) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // When rotation has started we should not be performing Surface Sync. The
  // viz::LocalSurfaceId should not have advanced.
  rwhva->OnSynchronizedDisplayPropertiesChanged(/* rotation= */ true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // When rotation has completed we should begin Surface Sync again. There
  // should also be a new viz::LocalSurfaceId.
  rwhva->OnPhysicalBackingSizeChanged(/* deadline_override= */ absl::nullopt);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_rotation_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_NE(initial_local_surface_id, post_rotation_local_surface_id);
  EXPECT_TRUE(post_rotation_local_surface_id.is_valid());
  EXPECT_TRUE(
      post_rotation_local_surface_id.IsNewerThan(initial_local_surface_id));
}

// Tests that when a rotation occurs while the view is hidden, that we advance
// the viz::LocalSurfaceId upon becoming visible again. Then once rotation has
// completed we update again, and unblock all other visual changes.
// (https://crbug.com/1223299)
TEST_F(RenderWidgetHostViewAndroidRotationTest,
       HiddenRotationDisplaysImmediately) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  rwhva->Hide();

  // When rotation has started we should not be performing Surface Sync. The
  // viz::LocalSurfaceId should not have advanced.
  rwhva->OnSynchronizedDisplayPropertiesChanged(/* rotation= */ true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // When rotation occurs while hidden we will only have a partial state.
  // However we do not want to delay showing content until Android tells us of
  // the final state. So we advance the viz::LocalSurfaceId to have the newest
  // possible content ready.
  rwhva->Show();
  const viz::LocalSurfaceId shown_local_surface_id = rwhva->GetLocalSurfaceId();
  EXPECT_NE(initial_local_surface_id, shown_local_surface_id);
  EXPECT_TRUE(shown_local_surface_id.is_valid());
  EXPECT_TRUE(shown_local_surface_id.IsNewerThan(initial_local_surface_id));
  // However we should still block further updates until rotation has completed.
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // When rotation has completed we should begin Surface Sync again. There
  // should also be a new viz::LocalSurfaceId.
  rwhva->OnPhysicalBackingSizeChanged(/* deadline_override= */ absl::nullopt);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_rotation_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_NE(shown_local_surface_id, post_rotation_local_surface_id);
  EXPECT_TRUE(post_rotation_local_surface_id.is_valid());
  EXPECT_TRUE(
      post_rotation_local_surface_id.IsNewerThan(shown_local_surface_id));
}

// Test that when the view is hidden, and another enters Fullscreen, that we
// complete the advancement of the viz::LocalSurfaceId upon becoming visible
// again. Since the Fullscreen flow does not trigger
// OnPhysicalBackingSizeChanged, we will continue to block further Surface Sync
// until the post rotation frame has been generated. (https://crbug.com/1223299)
TEST_F(RenderWidgetHostViewAndroidRotationTest,
       HiddenPartialRotationFromFullscreen) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  rwhva->Hide();

  // When another view enters Fullscreen, all hidden views are notified of the
  // start of rotation. We should not be performing Surface Sync. The
  // viz::LocalSurfaceId should not have advanced.
  rwhva->OnSynchronizedDisplayPropertiesChanged(/* rotation= */ true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // When the other view exits Fullscreen, all hidden views are notified of a
  // rotation back to the original orientation. We should continue in the same
  // state.
  rwhva->OnSynchronizedDisplayPropertiesChanged(/* rotation= */ true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // When the cause of rotation is Fullscreen, we will NOT receive a call to
  // OnPhysicalBackingSizeChanged. Due to this we advance the
  // viz::LocalSurfaceId upon becoming visible, to send all visual updates to
  // the Renderer.
  rwhva->Show();
  const viz::LocalSurfaceId shown_local_surface_id = rwhva->GetLocalSurfaceId();
  EXPECT_NE(initial_local_surface_id, shown_local_surface_id);
  EXPECT_TRUE(shown_local_surface_id.is_valid());
  EXPECT_TRUE(shown_local_surface_id.IsNewerThan(initial_local_surface_id));
  // However we should still block further updates until rotation has completed.
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());

  // When the Renderer submits the first post rotation frame we unblock further
  // Surface Sync.
  cc::RenderFrameMetadata metadata;
  metadata.local_surface_id = shown_local_surface_id;
  OnRenderFrameMetadataChangedAfterActivation(metadata, base::TimeTicks::Now());
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
}

// Tests that when a rotation occurs, that we accept updated viz::LocalSurfaceId
// from the Renderer, and merge them with any of our own changes.
// (https://crbug.com/1223299)
TEST_F(RenderWidgetHostViewAndroidRotationTest,
       ChildLocalSurfaceIdsAcceptedDuringRotation) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // When rotation has started we should not be performing Surface Sync. The
  // viz::LocalSurfaceId should not have advanced.
  rwhva->OnSynchronizedDisplayPropertiesChanged(/* rotation= */ true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // If the child Renderer advances the viz::LocalSurfaceId we should accept it
  // and merge it. So that we are up to date for when rotation completes.
  const viz::LocalSurfaceId child_advanced_local_surface_id(
      initial_local_surface_id.parent_sequence_number(),
      initial_local_surface_id.child_sequence_number() + 1,
      initial_local_surface_id.embed_token());
  cc::RenderFrameMetadata metadata;
  metadata.local_surface_id = child_advanced_local_surface_id;
  OnDidUpdateVisualPropertiesComplete(metadata);
  const viz::LocalSurfaceId merged_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_NE(initial_local_surface_id, merged_local_surface_id);
  EXPECT_TRUE(merged_local_surface_id.is_valid());
  EXPECT_TRUE(merged_local_surface_id.IsNewerThan(initial_local_surface_id));
  // Still wait for rotation to end before resuming Surface Sync from other
  // sources.
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
}

TEST_F(RenderWidgetHostViewAndroidRotationTest, FullscreenRotation) {
  // Android default host and views initialize as visible.
  RenderWidgetHostViewAndroid* rwhva = render_widget_host_view_android();
  EXPECT_TRUE(rwhva->IsShowing());
  const viz::LocalSurfaceId initial_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_TRUE(initial_local_surface_id.is_valid());

  // When entering fullscreen the rotation should behave as normal.
  delegate()->set_is_fullscreen(true);

  // When rotation has started we should not be performing Surface Sync. The
  // viz::LocalSurfaceId should not have advanced.
  rwhva->OnSynchronizedDisplayPropertiesChanged(/* rotation= */ true);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(initial_local_surface_id, rwhva->GetLocalSurfaceId());

  // When rotation has completed we should begin Surface Sync again. There
  // should also be a new viz::LocalSurfaceId.
  rwhva->OnPhysicalBackingSizeChanged(/* deadline_override= */ absl::nullopt);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_rotation_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_NE(initial_local_surface_id, post_rotation_local_surface_id);
  EXPECT_TRUE(post_rotation_local_surface_id.is_valid());
  EXPECT_TRUE(
      post_rotation_local_surface_id.IsNewerThan(initial_local_surface_id));

  {
    cc::RenderFrameMetadata metadata;
    metadata.local_surface_id = post_rotation_local_surface_id;
    OnRenderFrameMetadataChangedAfterActivation(metadata,
                                                base::TimeTicks::Now());
  }

  // When exiting rotation the order of visual property updates can differ.
  delegate()->set_is_fullscreen(false);
  rwhva->OnPhysicalBackingSizeChanged(/* deadline_override= */ absl::nullopt);
  EXPECT_FALSE(rwhva->CanSynchronizeVisualProperties());
  EXPECT_EQ(post_rotation_local_surface_id, rwhva->GetLocalSurfaceId());

  rwhva->OnSynchronizedDisplayPropertiesChanged(/* rotation= */ true);
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());
  const viz::LocalSurfaceId post_fullscreen_local_surface_id =
      rwhva->GetLocalSurfaceId();
  EXPECT_NE(post_rotation_local_surface_id, post_fullscreen_local_surface_id);
  EXPECT_TRUE(post_fullscreen_local_surface_id.is_valid());
  EXPECT_TRUE(post_fullscreen_local_surface_id.IsNewerThan(
      post_rotation_local_surface_id));
  EXPECT_TRUE(rwhva->CanSynchronizeVisualProperties());

  {
    cc::RenderFrameMetadata metadata;
    metadata.local_surface_id = post_fullscreen_local_surface_id;
    OnRenderFrameMetadataChangedAfterActivation(metadata,
                                                base::TimeTicks::Now());
  }
}

}  // namespace content
