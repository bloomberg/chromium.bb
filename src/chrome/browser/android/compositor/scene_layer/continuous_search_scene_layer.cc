// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/continuous_search_scene_layer.h"

#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/continuous_search/jni_headers/ContinuousSearchSceneLayer_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

ContinuousSearchSceneLayer::ContinuousSearchSceneLayer(
    JNIEnv* env,
    const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      view_container_(cc::Layer::Create()),
      view_layer_(cc::UIResourceLayer::Create()) {
  layer()->SetIsDrawable(true);

  view_container_->SetIsDrawable(true);
  view_container_->SetMasksToBounds(true);

  view_layer_->SetIsDrawable(true);
  view_container_->AddChild(view_layer_);
}

ContinuousSearchSceneLayer::~ContinuousSearchSceneLayer() {}

void ContinuousSearchSceneLayer::UpdateContinuousSearchLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jresource_manager,
    jint view_resource_id,
    jint y_offset,
    jboolean shadow_visible,
    jint shadow_height) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::Resource* resource = resource_manager->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, view_resource_id);

  // If the resource isn't available, don't bother doing anything else.
  if (!resource)
    return;

  view_layer_->SetUIResourceId(resource->ui_resource()->id());

  int container_height = resource->size().height();

  if (!shadow_visible) {
    container_height -= shadow_height;
  }

  view_container_->SetBounds(
      gfx::Size(resource->size().width(), container_height));
  view_container_->SetPosition(gfx::PointF(0, y_offset));

  // The view's layer should be the same size as the texture.
  view_layer_->SetBounds(resource->size());
  view_layer_->SetPosition(gfx::PointF(0, 0));
}

void ContinuousSearchSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer())
    return;

  if (!content_tree->layer()->parent() ||
      (content_tree->layer()->parent()->id() != layer_->id())) {
    layer_->AddChild(content_tree->layer());
    layer_->AddChild(view_container_);
  }

  // Propagate the background color up from the content layer.
  should_show_background_ = content_tree->ShouldShowBackground();
  background_color_ = content_tree->GetBackgroundColor();
}

SkColor ContinuousSearchSceneLayer::GetBackgroundColor() {
  return background_color_;
}

bool ContinuousSearchSceneLayer::ShouldShowBackground() {
  return should_show_background_;
}

static jlong JNI_ContinuousSearchSceneLayer_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  ContinuousSearchSceneLayer* scene_layer =
      new ContinuousSearchSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
