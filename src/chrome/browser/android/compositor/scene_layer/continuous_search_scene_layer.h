// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTINUOUS_SEARCH_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTINUOUS_SEARCH_SCENE_LAYER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/layouts/scene_layer.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace cc {
class Layer;
class UIResourceLayer;
}  // namespace cc

namespace android {

class ContinuousSearchSceneLayer : public SceneLayer {
 public:
  ContinuousSearchSceneLayer(JNIEnv* env,
                             const base::android::JavaRef<jobject>& jobj);

  ContinuousSearchSceneLayer(const ContinuousSearchSceneLayer&) = delete;
  ContinuousSearchSceneLayer& operator=(const ContinuousSearchSceneLayer&) =
      delete;

  ~ContinuousSearchSceneLayer() override;

  // Update the compositor version of the view.
  void UpdateContinuousSearchLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jresource_manager,
      jint view_resource_id,
      jint y_offset,
      jboolean shadow_visible,
      jint shadow_height);

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  SkColor GetBackgroundColor() override;
  bool ShouldShowBackground() override;

 private:
  bool should_show_background_;
  SkColor background_color_;
  scoped_refptr<cc::Layer> view_container_;
  scoped_refptr<cc::UIResourceLayer> view_layer_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTINUOUS_SEARCH_SCENE_LAYER_H_
