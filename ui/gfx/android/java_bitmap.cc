// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/java_bitmap.h"

#include <android/bitmap.h>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

using base::android::AttachCurrentThread;
using base::android::GetClass;
using base::android::GetMethodID;
using base::android::GetStaticFieldID;
using base::android::GetStaticMethodID;
using base::android::ScopedJavaLocalRef;

namespace {
static jclass g_AndroidBitmap_clazz = NULL;
static jmethodID g_AndroidBitmap_createBitmap_method = NULL;
static jobject g_BitmapConfig_ARGB8888 = NULL;
}  // anonymous namespace

namespace gfx {

JavaBitmap::JavaBitmap(jobject bitmap)
    : bitmap_(bitmap),
      pixels_(NULL) {
  int err = AndroidBitmap_lockPixels(AttachCurrentThread(), bitmap_, &pixels_);
  DCHECK(!err);
  DCHECK(pixels_);
}

JavaBitmap::~JavaBitmap() {
  int err = AndroidBitmap_unlockPixels(AttachCurrentThread(), bitmap_);
  DCHECK(!err);
}

gfx::Size JavaBitmap::Size() const {
  AndroidBitmapInfo info;
  int err = AndroidBitmap_getInfo(AttachCurrentThread(), bitmap_, &info);
  DCHECK(!err);
  return gfx::Size(info.width, info.height);
}

int JavaBitmap::Format() const {
  AndroidBitmapInfo info;
  int err = AndroidBitmap_getInfo(AttachCurrentThread(), bitmap_, &info);
  DCHECK(!err);
  return info.format;
}

uint32_t JavaBitmap::Stride() const {
  AndroidBitmapInfo info;
  int err = AndroidBitmap_getInfo(AttachCurrentThread(), bitmap_, &info);
  DCHECK(!err);
  return info.stride;
}

void RegisterBitmapAndroid(JNIEnv* env) {
  g_AndroidBitmap_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetUnscopedClass(env, "android/graphics/Bitmap")));
  ScopedJavaLocalRef<jclass> bitmapConfig_clazz = base::android::GetClass(
      env, "android/graphics/Bitmap$Config");
  g_AndroidBitmap_createBitmap_method = GetStaticMethodID(env,
      g_AndroidBitmap_clazz, "createBitmap",
      "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
  jfieldID argb_8888_id = GetStaticFieldID(env, bitmapConfig_clazz, "ARGB_8888",
      "Landroid/graphics/Bitmap$Config;");
  g_BitmapConfig_ARGB8888 = reinterpret_cast<jobject>(env->NewGlobalRef(
      env->GetStaticObjectField(bitmapConfig_clazz.obj(), argb_8888_id)));
}

ScopedJavaLocalRef<jobject> CreateJavaBitmap(const gfx::Size& size) {
  DCHECK(g_AndroidBitmap_clazz);
  JNIEnv* env = AttachCurrentThread();
  jobject bitmap_object = env->CallStaticObjectMethod(g_AndroidBitmap_clazz,
        g_AndroidBitmap_createBitmap_method, size.width(), size.height(),
        g_BitmapConfig_ARGB8888);
  return ScopedJavaLocalRef<jobject>(env, bitmap_object);
}

ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(const SkBitmap* skbitmap) {
  DCHECK(skbitmap);
  DCHECK_EQ(skbitmap->bytesPerPixel(), 4);

  ScopedJavaLocalRef<jobject> jbitmap =
      CreateJavaBitmap(gfx::Size(skbitmap->width(), skbitmap->height()));
  SkAutoLockPixels src_lock(*skbitmap);
  JavaBitmap dst_lock(jbitmap.obj());
  void* src_pixels = skbitmap->getPixels();
  void* dst_pixels = dst_lock.pixels();
  memcpy(dst_pixels, src_pixels, skbitmap->getSize());

  return jbitmap;
}

}  //  namespace gfx
