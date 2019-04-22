// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/ephemeral_tab_scene_layer.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "cc/layers/solid_color_layer.h"
#include "chrome/browser/android/compositor/layer/ephemeral_tab_layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/web_contents.h"
#include "jni/EphemeralTabSceneLayer_jni.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

EphemeralTabSceneLayer::EphemeralTabSceneLayer(JNIEnv* env,
                                               const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      base_page_brightness_(1.0f),
      content_container_(cc::Layer::Create()) {
  // Responsible for moving the base page without modifying the layer itself.
  content_container_->SetIsDrawable(true);
  content_container_->SetPosition(gfx::PointF(0.0f, 0.0f));
  layer()->AddChild(content_container_);
}

EphemeralTabSceneLayer::~EphemeralTabSceneLayer() {}

void EphemeralTabSceneLayer::CreateEphemeralTabLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jobject>& jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ephemeral_tab_layer_ = EphemeralTabLayer::Create(resource_manager);

  // The layer is initially invisible.
  ephemeral_tab_layer_->layer()->SetHideLayerAndSubtree(true);
  layer()->AddChild(ephemeral_tab_layer_->layer());
}

void EphemeralTabSceneLayer::SetResourceIds(JNIEnv* env,
                                            const JavaParamRef<jobject>& object,
                                            jint text_resource_id,
                                            jint bar_background_resource_id,
                                            jint bar_shadow_resource_id,
                                            jint panel_icon_resource_id,
                                            jint close_icon_resource_id) {
  ephemeral_tab_layer_->SetResourceIds(
      text_resource_id, bar_background_resource_id, bar_shadow_resource_id,
      panel_icon_resource_id, close_icon_resource_id);
}

void EphemeralTabSceneLayer::Update(JNIEnv* env,
                                    const JavaParamRef<jobject>& object,
                                    jint title_view_resource_id,
                                    jint caption_view_resource_id,
                                    jfloat caption_animation_percentage,
                                    jfloat text_layer_min_height,
                                    jfloat title_caption_spacing,
                                    jboolean caption_visible,
                                    jint progress_bar_background_resource_id,
                                    jint progress_bar_resource_id,
                                    jfloat dp_to_px,
                                    jfloat base_page_brightness,
                                    jfloat base_page_offset,
                                    const JavaParamRef<jobject>& jweb_contents,
                                    jfloat panel_x,
                                    jfloat panel_y,
                                    jfloat panel_width,
                                    jfloat panel_height,
                                    jint bar_background_color,
                                    jfloat bar_margin_side,
                                    jfloat bar_height,
                                    jboolean bar_border_visible,
                                    jfloat bar_border_height,
                                    jboolean bar_shadow_visible,
                                    jfloat bar_shadow_opacity,
                                    jint icon_color,
                                    jboolean progress_bar_visible,
                                    jfloat progress_bar_height,
                                    jfloat progress_bar_opacity,
                                    jint progress_bar_completion) {
  // NOTE(mdjones): It is possible to render the panel before content has been
  // created. If this is the case, do not attempt to access the WebContents
  // and instead pass null.
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  scoped_refptr<cc::Layer> content_layer =
      web_contents ? web_contents->GetNativeView()->GetLayer() : nullptr;
  // Fade the base page out.
  if (base_page_brightness_ != base_page_brightness) {
    base_page_brightness_ = base_page_brightness;
    cc::FilterOperations filters;
    if (base_page_brightness < 1.f) {
      filters.Append(
          cc::FilterOperation::CreateBrightnessFilter(base_page_brightness));
    }
    content_container_->SetFilters(filters);
  }
  // Move the base page contents up.
  content_container_->SetPosition(gfx::PointF(0.0f, base_page_offset));
  ephemeral_tab_layer_->SetProperties(
      title_view_resource_id, caption_view_resource_id,
      caption_animation_percentage, text_layer_min_height,
      title_caption_spacing, caption_visible,
      progress_bar_background_resource_id, progress_bar_resource_id, dp_to_px,
      content_layer, panel_x, panel_y, panel_width, panel_height,
      bar_background_color, bar_margin_side, bar_height, bar_border_visible,
      bar_border_height, bar_shadow_visible, bar_shadow_opacity, icon_color,
      progress_bar_visible, progress_bar_height, progress_bar_opacity,
      progress_bar_completion);
  // Make the layer visible if it is not already.
  ephemeral_tab_layer_->layer()->SetHideLayerAndSubtree(false);
}

void EphemeralTabSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer())
    return;
  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != content_container_->id())) {
    content_container_->AddChild(content_tree->layer());
  }
}

void EphemeralTabSceneLayer::HideTree(JNIEnv* env,
                                      const JavaParamRef<jobject>& jobj) {
  // TODO(mdjones): Create super class for this logic.
  if (ephemeral_tab_layer_) {
    ephemeral_tab_layer_->layer()->SetHideLayerAndSubtree(true);
  }
  // Reset base page brightness.
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateBrightnessFilter(1.0f));
  content_container_->SetFilters(filters);
  // Reset base page offset.
  content_container_->SetPosition(gfx::PointF(0.0f, 0.0f));
}

static jlong JNI_EphemeralTabSceneLayer_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  EphemeralTabSceneLayer* ephemeral_tab_scene_layer =
      new EphemeralTabSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(ephemeral_tab_scene_layer);
}

}  // namespace android
