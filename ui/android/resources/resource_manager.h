// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_RESOURCE_MANAGER_H_
#define UI_ANDROID_RESOURCES_RESOURCE_MANAGER_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/id_map.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/android/ui_android_export.h"
#include "ui/base/android/system_ui_resource_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class UIResourceAndroid;
class UIResourceProvider;

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.resources
enum AndroidResourceType {
  ANDROID_RESOURCE_TYPE_STATIC = 0,
  ANDROID_RESOURCE_TYPE_DYNAMIC,
  ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP,
  ANDROID_RESOURCE_TYPE_SYSTEM,

  ANDROID_RESOURCE_TYPE_COUNT,
  ANDROID_RESOURCE_TYPE_FIRST = ANDROID_RESOURCE_TYPE_STATIC,
  ANDROID_RESOURCE_TYPE_LAST = ANDROID_RESOURCE_TYPE_SYSTEM,
};

// TODO(jdduke): Make ResourceManager a pure interface, crbug/426939.
class UI_ANDROID_EXPORT ResourceManager {
 public:
  struct Resource {
   public:
    Resource();
    ~Resource();
    gfx::Rect Border(const gfx::Size& bounds);
    gfx::Rect Border(const gfx::Size& bounds, const gfx::InsetsF& scale);

    scoped_ptr<UIResourceAndroid> ui_resource;
    gfx::Size size;
    gfx::Rect padding;
    gfx::Rect aperture;
  };

  static ResourceManager* FromJavaObject(jobject jobj);

  explicit ResourceManager(ui::UIResourceProvider* ui_resource_provider);
  virtual ~ResourceManager();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject(JNIEnv* env);

  virtual cc::UIResourceId GetUIResourceId(AndroidResourceType res_type,
                                           int res_id);
  ResourceManager::Resource* GetResource(AndroidResourceType res_type,
                                         int res_id);
  virtual void PreloadResource(AndroidResourceType res_type, int res_id);

  // Called from Java ----------------------------------------------------------
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
  friend class TestResourceManager;

  // Start loading the resource. virtual for testing.
  virtual void PreloadResourceFromJava(AndroidResourceType res_type,
                                       int res_id);
  virtual void RequestResourceFromJava(AndroidResourceType res_type,
                                       int res_id);

  typedef IDMap<Resource, IDMapOwnPointer> ResourceMap;

  ui::UIResourceProvider* ui_resource_provider_;
  ResourceMap resources_[ANDROID_RESOURCE_TYPE_COUNT];

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(ResourceManager);
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_RESOURCE_MANAGER_H_
