// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/resource_manager_impl.h"

#include "base/android/jni_string.h"
#include "base/trace_event/trace_event.h"
#include "jni/ResourceManager_jni.h"
#include "ui/android/resources/ui_resource_android.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/gfx/android/java_bitmap.h"

namespace ui {

// static
ResourceManagerImpl* ResourceManagerImpl::FromJavaObject(jobject jobj) {
  return reinterpret_cast<ResourceManagerImpl*>(
      Java_ResourceManager_getNativePtr(base::android::AttachCurrentThread(),
                                        jobj));
}

ResourceManagerImpl::ResourceManagerImpl(
    ui::UIResourceProvider* ui_resource_provider)
    : ui_resource_provider_(ui_resource_provider) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_ResourceManager_create(
                           env, base::android::GetApplicationContext(),
                           reinterpret_cast<intptr_t>(this)).obj());
  DCHECK(!java_obj_.is_null());
}

ResourceManagerImpl::~ResourceManagerImpl() {
  Java_ResourceManager_destroy(base::android::AttachCurrentThread(),
                               java_obj_.obj());
}

base::android::ScopedJavaLocalRef<jobject>
ResourceManagerImpl::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

ResourceManager::Resource* ResourceManagerImpl::GetResource(
    AndroidResourceType res_type,
    int res_id) {
  DCHECK_GE(res_type, ANDROID_RESOURCE_TYPE_FIRST);
  DCHECK_LE(res_type, ANDROID_RESOURCE_TYPE_LAST);

  Resource* resource = resources_[res_type].Lookup(res_id);

  if (!resource || res_type == ANDROID_RESOURCE_TYPE_DYNAMIC ||
      res_type == ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP) {
    RequestResourceFromJava(res_type, res_id);
    resource = resources_[res_type].Lookup(res_id);
  }

  return resource;
}

void ResourceManagerImpl::PreloadResource(AndroidResourceType res_type,
                                          int res_id) {
  DCHECK_GE(res_type, ANDROID_RESOURCE_TYPE_FIRST);
  DCHECK_LE(res_type, ANDROID_RESOURCE_TYPE_LAST);

  // Don't send out a query if the resource is already loaded.
  if (resources_[res_type].Lookup(res_id))
    return;

  PreloadResourceFromJava(res_type, res_id);
}

void ResourceManagerImpl::OnResourceReady(JNIEnv* env,
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
                                          jint aperture_bottom) {
  DCHECK_GE(res_type, ANDROID_RESOURCE_TYPE_FIRST);
  DCHECK_LE(res_type, ANDROID_RESOURCE_TYPE_LAST);
  TRACE_EVENT2("ui", "ResourceManagerImpl::OnResourceReady",
               "resource_type", res_type,
               "resource_id", res_id);

  Resource* resource = resources_[res_type].Lookup(res_id);
  if (!resource) {
    resource = new Resource();
    resources_[res_type].AddWithID(resource, res_id);
  }

  gfx::JavaBitmap jbitmap(bitmap);
  resource->size = jbitmap.size();
  resource->padding.SetRect(padding_left, padding_top,
                            padding_right - padding_left,
                            padding_bottom - padding_top);
  resource->aperture.SetRect(aperture_left, aperture_top,
                             aperture_right - aperture_left,
                             aperture_bottom - aperture_top);
  resource->ui_resource =
      UIResourceAndroid::CreateFromJavaBitmap(ui_resource_provider_, jbitmap);
}

// static
bool ResourceManagerImpl::RegisterResourceManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ResourceManagerImpl::PreloadResourceFromJava(AndroidResourceType res_type,
                                                  int res_id) {
  TRACE_EVENT2("ui", "ResourceManagerImpl::PreloadResourceFromJava",
               "resource_type", res_type,
               "resource_id", res_id);
  Java_ResourceManager_preloadResource(base::android::AttachCurrentThread(),
                                       java_obj_.obj(), res_type, res_id);
}

void ResourceManagerImpl::RequestResourceFromJava(AndroidResourceType res_type,
                                                  int res_id) {
  TRACE_EVENT2("ui", "ResourceManagerImpl::RequestResourceFromJava",
               "resource_type", res_type,
               "resource_id", res_id);
  Java_ResourceManager_resourceRequested(base::android::AttachCurrentThread(),
                                         java_obj_.obj(), res_type, res_id);
}

}  // namespace ui
