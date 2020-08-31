// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_controls_container_view.h"

#include "base/android/jni_string.h"
#include "cc/layers/ui_resource_layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_controls_state.h"
#include "ui/android/resources/resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/view_android.h"
#include "weblayer/browser/content_view_render_view.h"
#include "weblayer/browser/java/jni/BrowserControlsContainerView_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace weblayer {

BrowserControlsContainerView::BrowserControlsContainerView(
    const JavaParamRef<jobject>& java_browser_controls_container_view,
    ContentViewRenderView* content_view_render_view,
    bool is_top)
    : java_browser_controls_container_view_(
          java_browser_controls_container_view),
      content_view_render_view_(content_view_render_view),
      is_top_(is_top) {
  DCHECK(content_view_render_view_);
}

BrowserControlsContainerView::~BrowserControlsContainerView() = default;

int BrowserControlsContainerView::GetControlsHeight() {
  return controls_layer_ ? controls_layer_->bounds().height() : 0;
}

int BrowserControlsContainerView::GetContentHeightDelta() {
  if (!controls_layer_ || !web_contents())
    return 0;

  if (is_top_)
    return web_contents()->GetNativeView()->GetLayer()->position().y();

  return content_view_render_view_->height() - controls_layer_->position().y();
}

bool BrowserControlsContainerView::IsFullyVisible() const {
  // At this time this is only needed for top-controls.
  DCHECK(is_top_);
  return controls_layer_ && controls_layer_->position().y() == 0;
}

void BrowserControlsContainerView::CreateControlsLayer(
    JNIEnv* env,
    int id) {
  controls_resource_id_ = id;
  controls_layer_ = cc::UIResourceLayer::Create();
  // Real size is sent in SetControlsSize().
  controls_layer_->SetBounds(gfx::Size(1, 1));
  controls_layer_->SetPosition(gfx::PointF(0, 0));
  controls_layer_->SetElementId(cc::ElementId(controls_layer_->id()));
  controls_layer_->SetHitTestable(false);
  controls_layer_->SetIsDrawable(true);
  content_view_render_view_->root_container_layer()->AddChild(controls_layer_);
  UpdateControlsResource(env);
}

void BrowserControlsContainerView::DeleteBrowserControlsContainerView(
    JNIEnv* env) {
  delete this;
}

void BrowserControlsContainerView::DeleteControlsLayer(JNIEnv* env) {
  controls_layer_.reset();
}

void BrowserControlsContainerView::SetTopControlsOffset(JNIEnv* env,
                                                        int controls_offset_y,
                                                        int content_offset_y) {
  DCHECK(is_top_);
  // |controls_layer_| may not be created if the controls view has 0 height.
  if (controls_layer_)
    controls_layer_->SetPosition(gfx::PointF(0, controls_offset_y));
  if (web_contents()) {
    web_contents()->GetNativeView()->GetLayer()->SetPosition(
        gfx::PointF(0, content_offset_y));
  }
}

void BrowserControlsContainerView::SetBottomControlsOffset(
    JNIEnv* env,
    int controls_offset_y) {
  DCHECK(!is_top_);
  // |controls_layer_| may not be created if the controls view has 0 height.
  if (controls_layer_) {
    controls_layer_->SetPosition(
        gfx::PointF(0, content_view_render_view_->height() -
                           GetControlsHeight() + controls_offset_y));
  }
}

void BrowserControlsContainerView::SetControlsSize(
    JNIEnv* env,
    int width,
    int height) {
  DCHECK(controls_layer_);
  controls_layer_->SetBounds(gfx::Size(width, height));
}

void BrowserControlsContainerView::UpdateControlsResource(JNIEnv* env) {
  DCHECK(controls_layer_);
  ui::ResourceManager& resource_manager =
      content_view_render_view_->compositor()->GetResourceManager();
  ui::Resource* controls_resource = resource_manager.GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, controls_resource_id_);
  DCHECK(controls_resource);
  controls_layer_->SetUIResourceId(controls_resource->ui_resource()->id());
}

void BrowserControlsContainerView::SetWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& web_contents) {
  Observe(content::WebContents::FromJavaWebContents(web_contents));
}

void BrowserControlsContainerView::DidToggleFullscreenModeForTab(
    bool entered_fullscreen,
    bool will_cause_resize) {
  TRACE_EVENT0(
      "weblayer",
      "Java_BrowserControlsContainerView_didToggleFullscreenModeForTab");
  Java_BrowserControlsContainerView_didToggleFullscreenModeForTab(
      AttachCurrentThread(), java_browser_controls_container_view_,
      entered_fullscreen);
}

static jlong
JNI_BrowserControlsContainerView_CreateBrowserControlsContainerView(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_browser_controls_container_view,
    jlong native_content_view_render_view,
    jboolean is_top) {
  return reinterpret_cast<jlong>(new BrowserControlsContainerView(
      java_browser_controls_container_view,
      reinterpret_cast<ContentViewRenderView*>(native_content_view_render_view),
      is_top));
}

}  // namespace weblayer
