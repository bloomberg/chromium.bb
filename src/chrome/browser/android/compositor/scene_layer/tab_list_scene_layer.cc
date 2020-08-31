// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/tab_list_scene_layer.h"

#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "cc/layers/picture_image_layer.h"
#include "chrome/android/chrome_jni_headers/TabListSceneLayer_jni.h"
#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "chrome/browser/android/compositor/layer/tab_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "ui/android/resources/resource_manager_impl.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace android {

TabListSceneLayer::TabListSceneLayer(JNIEnv* env, const JavaRef<jobject>& jobj)
    : SceneLayer(env, jobj),
      content_obscures_self_(false),
      resource_manager_(nullptr),
      layer_title_cache_(nullptr),
      tab_content_manager_(nullptr),
      background_color_(SK_ColorWHITE),
      own_tree_(cc::Layer::Create()) {
  layer()->AddChild(own_tree_);
}

TabListSceneLayer::~TabListSceneLayer() {
}

void TabListSceneLayer::BeginBuildingFrame(JNIEnv* env,
                                           const JavaParamRef<jobject>& jobj) {
  content_obscures_self_ = false;

  // Remove (and re-add) all layers every frame to guarantee that z-order
  // matches PutTabLayer call order.
  for (auto tab : tab_map_)
    tab.second->layer()->RemoveFromParent();

  used_tints_.clear();
}

void TabListSceneLayer::FinishBuildingFrame(JNIEnv* env,
                                            const JavaParamRef<jobject>& jobj) {
  // Destroy all tabs that weren't used this frame.
  for (auto it = tab_map_.cbegin(); it != tab_map_.cend();) {
    if (visible_tabs_this_frame_.find(it->first) ==
        visible_tabs_this_frame_.end())
      it = tab_map_.erase(it);
    else
      ++it;
  }
  visible_tabs_this_frame_.clear();
  DCHECK(resource_manager_);
  resource_manager_->RemoveUnusedTints(used_tints_);
}

void TabListSceneLayer::UpdateLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jint background_color,
    jfloat viewport_x,
    jfloat viewport_y,
    jfloat viewport_width,
    jfloat viewport_height,
    const JavaParamRef<jobject>& jlayer_title_cache,
    const JavaParamRef<jobject>& jtab_content_manager,
    const JavaParamRef<jobject>& jresource_manager) {
  // TODO(changwan): move these to constructor if possible
  if (!resource_manager_) {
    resource_manager_ =
        ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  }
  if (!layer_title_cache_)
    layer_title_cache_ = LayerTitleCache::FromJavaObject(jlayer_title_cache);
  if (!tab_content_manager_) {
    tab_content_manager_ =
        TabContentManager::FromJavaObject(jtab_content_manager);
  }

  background_color_ = background_color;
  own_tree_->SetPosition(gfx::PointF(viewport_x, viewport_y));
  own_tree_->SetBounds(gfx::Size(viewport_width, viewport_height));
}

void TabListSceneLayer::PutTabLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint id,
    const base::android::JavaRef<jintArray>& tab_ids_list,
    jboolean use_tab_ids_list,
    jint toolbar_resource_id,
    jint close_button_resource_id,
    jint shadow_resource_id,
    jint contour_resource_id,
    jint back_logo_resource_id,
    jint border_resource_id,
    jint border_inner_shadow_resource_id,
    jboolean can_use_live_layer,
    jint tab_background_color,
    jint back_logo_color,
    jboolean incognito,
    jboolean close_button_on_right,
    jfloat x,
    jfloat y,
    jfloat width,
    jfloat height,
    jfloat content_width,
    jfloat content_height,
    jfloat visible_content_height,
    jfloat shadow_x,
    jfloat shadow_y,
    jfloat shadow_width,
    jfloat shadow_height,
    jfloat pivot_x,
    jfloat pivot_y,
    jfloat rotation_x,
    jfloat rotation_y,
    jfloat alpha,
    jfloat border_alpha,
    jfloat border_inner_shadow_alpha,
    jfloat contour_alpha,
    jfloat shadow_alpha,
    jfloat close_alpha,
    jfloat close_btn_width,
    jfloat close_btn_asset_size,
    jfloat static_to_view_blend,
    jfloat border_scale,
    jfloat saturation,
    jfloat brightness,
    jboolean show_toolbar,
    jint default_theme_color,
    jint toolbar_background_color,
    jint close_button_color,
    jboolean anonymize_toolbar,
    jboolean show_tab_title,
    jint toolbar_textbox_resource_id,
    jint toolbar_textbox_background_color,
    jfloat toolbar_textbox_alpha,
    jfloat toolbar_alpha,
    jfloat content_offset,
    jfloat side_border_scale,
    jboolean inset_border) {
  scoped_refptr<TabLayer> layer;
  auto iter = tab_map_.find(id);
  if (iter != tab_map_.end()) {
    layer = iter->second;
  } else {
    layer = TabLayer::Create(incognito, resource_manager_, layer_title_cache_,
                             tab_content_manager_);
    tab_map_.insert(TabMap::value_type(id, layer));
  }
  own_tree_->AddChild(layer->layer());
  visible_tabs_this_frame_.insert(id);

  // Add the tints for the border asset and close icon to the list that was
  // used for this frame.
  used_tints_.insert(toolbar_background_color);
  used_tints_.insert(close_button_color);
  used_tints_.insert(default_theme_color);
  used_tints_.insert(toolbar_textbox_background_color);

  DCHECK(layer);
  if (layer) {
    std::vector<int> tab_ids;
    if (use_tab_ids_list)
      base::android::JavaIntArrayToIntVector(env, tab_ids_list, &tab_ids);

    // TODO(meiliang): This method pass another argument, a resource that can be
    // used to indicate the currently selected tab for the TabLayer.
    layer->SetProperties(
        id, tab_ids, can_use_live_layer, toolbar_resource_id,
        close_button_resource_id, shadow_resource_id, contour_resource_id,
        back_logo_resource_id, border_resource_id,
        border_inner_shadow_resource_id, tab_background_color, back_logo_color,
        close_button_on_right, x, y, width, height, shadow_x, shadow_y,
        shadow_width, shadow_height, pivot_x, pivot_y, rotation_x, rotation_y,
        alpha, border_alpha, border_inner_shadow_alpha, contour_alpha,
        shadow_alpha, close_alpha, border_scale, saturation, brightness,
        close_btn_width, close_btn_asset_size, static_to_view_blend,
        content_width, content_height, content_width, visible_content_height,
        show_toolbar, default_theme_color, toolbar_background_color,
        close_button_color, anonymize_toolbar, show_tab_title,
        toolbar_textbox_resource_id, toolbar_textbox_background_color,
        toolbar_textbox_alpha, toolbar_alpha, content_offset, side_border_scale,
        inset_border);
  }

  gfx::RectF self(own_tree_->position(), gfx::SizeF(own_tree_->bounds()));
  gfx::RectF content(x, y, width, height);

  content_obscures_self_ |= content.Contains(self);
}

void TabListSceneLayer::PutBackgroundLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jint resource_id,
    jfloat alpha,
    jint top_offset) {
  int ui_resource_id = resource_manager_->GetUIResourceId(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, resource_id);
  if (ui_resource_id == 0)
    return;

  if (!background_layer_) {
    background_layer_ = cc::UIResourceLayer::Create();
    background_layer_->SetIsDrawable(true);
    own_tree_->AddChild(background_layer_);
  }
  DCHECK(background_layer_);
  background_layer_->SetUIResourceId(ui_resource_id);
  gfx::Size size =
      resource_manager_
          ->GetResource(ui::ANDROID_RESOURCE_TYPE_DYNAMIC, resource_id)
          ->size();
  background_layer_->SetBounds(size);
  background_layer_->SetOpacity(alpha);
  background_layer_->SetPosition(gfx::PointF(0, top_offset));
}

void TabListSceneLayer::OnDetach() {
  SceneLayer::OnDetach();
  for (auto tab : tab_map_)
    tab.second->layer()->RemoveFromParent();
  tab_map_.clear();
}

bool TabListSceneLayer::ShouldShowBackground() {
  return !content_obscures_self_;
}

SkColor TabListSceneLayer::GetBackgroundColor() {
  return background_color_;
}

static jlong JNI_TabListSceneLayer_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  TabListSceneLayer* scene_layer = new TabListSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

}  // namespace android
