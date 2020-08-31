// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_java_utils.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "chrome/browser/android/vr/android_vr_utils.h"
#include "chrome/browser/android/vr/ar_jni_headers/ArCoreJavaUtils_jni.h"
#include "chrome/browser/android/vr/arcore_device/arcore_shim.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace vr {

ArCoreJavaUtils::ArCoreJavaUtils() {
  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;
  ScopedJavaLocalRef<jobject> j_arcore_java_utils =
      Java_ArCoreJavaUtils_create(env, (jlong)this);
  if (j_arcore_java_utils.is_null())
    return;
  j_arcore_java_utils_.Reset(j_arcore_java_utils);
}

ArCoreJavaUtils::~ArCoreJavaUtils() {
  JNIEnv* env = AttachCurrentThread();
  Java_ArCoreJavaUtils_onNativeDestroy(env, j_arcore_java_utils_);
}

void ArCoreJavaUtils::RequestArSession(
    int render_process_id,
    int render_frame_id,
    bool use_overlay,
    SurfaceReadyCallback ready_callback,
    SurfaceTouchCallback touch_callback,
    SurfaceDestroyedCallback destroyed_callback) {
  DVLOG(1) << __func__;
  JNIEnv* env = AttachCurrentThread();

  surface_ready_callback_ = std::move(ready_callback);
  surface_touch_callback_ = std::move(touch_callback);
  surface_destroyed_callback_ = std::move(destroyed_callback);

  Java_ArCoreJavaUtils_startSession(
      env, j_arcore_java_utils_,
      GetTabFromRenderer(render_process_id, render_frame_id), use_overlay);
}

void ArCoreJavaUtils::EndSession() {
  DVLOG(1) << __func__;
  JNIEnv* env = AttachCurrentThread();

  Java_ArCoreJavaUtils_endSession(env, j_arcore_java_utils_);
}

void ArCoreJavaUtils::OnDrawingSurfaceReady(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& surface,
    int rotation,
    int width,
    int height) {
  DVLOG(1) << __func__ << ": width=" << width << " height=" << height
           << " rotation=" << rotation;
  gfx::AcceleratedWidget window =
      ANativeWindow_fromSurface(base::android::AttachCurrentThread(), surface);
  display::Display::Rotation display_rotation =
      static_cast<display::Display::Rotation>(rotation);
  surface_ready_callback_.Run(window, display_rotation, {width, height});
}

void ArCoreJavaUtils::OnDrawingSurfaceTouch(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool primary,
    bool touching,
    int32_t pointer_id,
    float x,
    float y) {
  DVLOG(3) << __func__ << ": pointer_id=" << pointer_id
           << " primary=" << primary << " touching=" << touching;
  surface_touch_callback_.Run(primary, touching, pointer_id, {x, y});
}

void ArCoreJavaUtils::OnDrawingSurfaceDestroyed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DVLOG(1) << __func__ << ":::";
  if (surface_destroyed_callback_) {
    std::move(surface_destroyed_callback_).Run();
  }
}

bool ArCoreJavaUtils::EnsureLoaded() {
  DCHECK(vr::IsArCoreSupported());

  JNIEnv* env = AttachCurrentThread();

  // TODO(crbug.com/884780): Allow loading the ARCore shim by name instead of by
  // absolute path.
  ScopedJavaLocalRef<jstring> java_path =
      Java_ArCoreJavaUtils_getArCoreShimLibraryPath(env);

  // Crash in debug builds if `java_path` is a null pointer but handle this
  // situation in release builds. This is done by design - the `java_path` will
  // be null only if there was a regression introduced to our gn/gni files w/o
  // causing a build break. In release builds, this approach will result in the
  // site not being able to request an AR session.
  DCHECK(java_path)
      << "Unable to find path to ARCore SDK library - please ensure that "
         "loadable_modules and secondary_abi_loadable_modules are set "
         "correctly when building";
  if (!java_path) {
    LOG(ERROR) << "Unable to find path to ARCore SDK library";
    return false;
  }

  return LoadArCoreSdk(base::android::ConvertJavaStringToUTF8(env, java_path));
}

ScopedJavaLocalRef<jobject> ArCoreJavaUtils::GetApplicationContext() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ArCoreJavaUtils_getApplicationContext(env);
}

}  // namespace vr
