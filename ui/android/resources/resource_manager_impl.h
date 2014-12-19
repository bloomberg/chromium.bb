// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_RESOURCE_MANAGER_IMPL_H_
#define UI_ANDROID_RESOURCES_RESOURCE_MANAGER_IMPL_H_

#include "base/id_map.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class UI_ANDROID_EXPORT ResourceManagerImpl : public ResourceManager {
 public:
  static ResourceManagerImpl* FromJavaObject(jobject jobj);

  explicit ResourceManagerImpl(ui::UIResourceProvider* ui_resource_provider);
  ~ResourceManagerImpl() override;

  // ResourceManager implementation.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  Resource* GetResource(AndroidResourceType res_type, int res_id) override;
  void PreloadResource(AndroidResourceType res_type, int res_id) override;

  // Called from Java
  // ----------------------------------------------------------
  void OnResourceReady(JNIEnv* env,
                       jobject jobj,
                       jint res_type,
                       jint res_id,
                       jobject bitmap,
                       jint padding_left,
                       jint padding_top,
                       jint padding_right,
                       jint padding_bottom,
                       jint aperture_left,
                       jint aperture_top,
                       jint aperture_right,
                       jint aperture_bottom);

  static bool RegisterResourceManager(JNIEnv* env);

 private:
  friend class TestResourceManagerImpl;

  // Start loading the resource. virtual for testing.
  virtual void PreloadResourceFromJava(AndroidResourceType res_type,
                                       int res_id);
  virtual void RequestResourceFromJava(AndroidResourceType res_type,
                                       int res_id);

  typedef IDMap<Resource, IDMapOwnPointer> ResourceMap;

  ui::UIResourceProvider* const ui_resource_provider_;
  ResourceMap resources_[ANDROID_RESOURCE_TYPE_COUNT];

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(ResourceManagerImpl);
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_RESOURCE_MANAGER_IMPL_H_
