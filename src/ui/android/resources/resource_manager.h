// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_RESOURCE_MANAGER_H_
#define UI_ANDROID_RESOURCES_RESOURCE_MANAGER_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/android/jni_android.h"
#include "cc/resources/scoped_ui_resource.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/resource.h"
#include "ui/android/ui_android_export.h"

namespace ui {

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

// The ResourceManager serves as a cache for resources obtained through Android
// APIs and consumed by the compositor.
class UI_ANDROID_EXPORT ResourceManager {
 public:
  // Obtain a handle to the Java ResourceManager counterpart.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;

  // Return a handle to the resource specified by |res_type| and |res_id|.
  // If the resource has not been loaded, loading will be performed
  // synchronously, blocking until the load completes.
  // If load fails, a null handle will be returned and it is up to the caller
  // to react appropriately.
  virtual Resource* GetResource(AndroidResourceType res_type, int res_id) = 0;

  // Return a handle to a static resource specified by |res_id| that has a tint
  // of |tint_color| applied to it.
  virtual Resource* GetStaticResourceWithTint(int res_id,
                                              SkColor tint_color) = 0;

  // Remove tints that were unused in the current frame being built. This
  // function takes a set |used_tints| and removes all the tints not in the set
  // from the cache.
  virtual void RemoveUnusedTints(const std::unordered_set<int>& used_tints) = 0;

  // Trigger asynchronous loading of the resource specified by |res_type| and
  // |res_id|, if it has not yet been loaded.
  virtual void PreloadResource(AndroidResourceType res_type, int res_id) = 0;

  // Convenience wrapper method.
  cc::UIResourceId GetUIResourceId(AndroidResourceType res_type, int res_id) {
    Resource* resource = GetResource(res_type, res_id);
    return resource && resource->ui_resource() ? resource->ui_resource()->id()
                                               : 0;
  }

 protected:
  virtual ~ResourceManager() {}
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_RESOURCE_MANAGER_H_
