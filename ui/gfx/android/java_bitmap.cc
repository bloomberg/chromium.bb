// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/java_bitmap.h"

#include <android/bitmap.h>

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "jni/BitmapHelper_jni.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/size.h"

using base::android::AttachCurrentThread;

namespace gfx {

JavaBitmap::JavaBitmap(jobject bitmap)
    : bitmap_(bitmap),
      pixels_(NULL) {
  int err = AndroidBitmap_lockPixels(AttachCurrentThread(), bitmap_, &pixels_);
  DCHECK(!err);
  DCHECK(pixels_);

  AndroidBitmapInfo info;
  err = AndroidBitmap_getInfo(AttachCurrentThread(), bitmap_, &info);
  DCHECK(!err);
  size_ = gfx::Size(info.width, info.height);
  format_ = info.format;
  stride_ = info.stride;
}

JavaBitmap::~JavaBitmap() {
  int err = AndroidBitmap_unlockPixels(AttachCurrentThread(), bitmap_);
  DCHECK(!err);
}

// static
bool JavaBitmap::RegisterJavaBitmap(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static int SkBitmapConfigToBitmapFormat(SkBitmap::Config bitmap_config) {
  switch (bitmap_config) {
    case SkBitmap::kA8_Config:
      return BITMAP_FORMAT_ALPHA_8;
    case SkBitmap::kARGB_4444_Config:
      return BITMAP_FORMAT_ARGB_4444;
    case SkBitmap::kARGB_8888_Config:
      return BITMAP_FORMAT_ARGB_8888;
    case SkBitmap::kRGB_565_Config:
      return BITMAP_FORMAT_RGB_565;
    case SkBitmap::kNo_Config:
    default:
      NOTREACHED();
      return BITMAP_FORMAT_NO_CONFIG;
  }
}

ScopedJavaLocalRef<jobject> CreateJavaBitmap(int width,
                                             int height,
                                             SkBitmap::Config bitmap_config) {
  int java_bitmap_config = SkBitmapConfigToBitmapFormat(bitmap_config);
  return Java_BitmapHelper_createBitmap(
      AttachCurrentThread(), width, height, java_bitmap_config);
}

ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(const SkBitmap* skbitmap) {
  DCHECK(skbitmap);
  SkBitmap::Config bitmap_config = skbitmap->getConfig();
  DCHECK((bitmap_config == SkBitmap::kRGB_565_Config) ||
         (bitmap_config == SkBitmap::kARGB_8888_Config));
  ScopedJavaLocalRef<jobject> jbitmap = CreateJavaBitmap(
      skbitmap->width(), skbitmap->height(), bitmap_config);
  SkAutoLockPixels src_lock(*skbitmap);
  JavaBitmap dst_lock(jbitmap.obj());
  void* src_pixels = skbitmap->getPixels();
  void* dst_pixels = dst_lock.pixels();
  memcpy(dst_pixels, src_pixels, skbitmap->getSize());

  return jbitmap;
}

SkBitmap CreateSkBitmapFromJavaBitmap(JavaBitmap& jbitmap) {
  DCHECK_EQ(jbitmap.format(), ANDROID_BITMAP_FORMAT_RGBA_8888);

  gfx::Size src_size = jbitmap.size();

  SkBitmap skbitmap;
  skbitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     src_size.width(),
                     src_size.height(),
                     jbitmap.stride());
  if (!skbitmap.allocPixels()) {
    LOG(FATAL) << " Failed to allocate bitmap of size " << src_size.width()
               << "x" << src_size.height() << " stride=" << jbitmap.stride();
  }
  SkAutoLockPixels dst_lock(skbitmap);
  void* src_pixels = jbitmap.pixels();
  void* dst_pixels = skbitmap.getPixels();
  CHECK(src_pixels);

  memcpy(dst_pixels, src_pixels, skbitmap.getSize());

  return skbitmap;
}

SkBitmap CreateSkBitmapFromResource(const char* name, gfx::Size size) {
  DCHECK(!size.IsEmpty());
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jname(env, env->NewStringUTF(name));
  ScopedJavaLocalRef<jobject> jobj(Java_BitmapHelper_decodeDrawableResource(
      env, jname.obj(), size.width(), size.height()));
  if (jobj.is_null())
    return SkBitmap();

  JavaBitmap jbitmap(jobj.obj());
  SkBitmap bitmap = CreateSkBitmapFromJavaBitmap(jbitmap);
  return skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BOX, size.width(), size.height());
}

SkBitmap::Config ConvertToSkiaConfig(jobject bitmap_config) {
  int jbitmap_config = Java_BitmapHelper_getBitmapFormatForConfig(
      AttachCurrentThread(), bitmap_config);
  switch (jbitmap_config) {
    case BITMAP_FORMAT_ALPHA_8:
      return SkBitmap::kA8_Config;
    case BITMAP_FORMAT_ARGB_4444:
      return SkBitmap::kARGB_4444_Config;
    case BITMAP_FORMAT_ARGB_8888:
      return SkBitmap::kARGB_8888_Config;
    case BITMAP_FORMAT_RGB_565:
      return SkBitmap::kRGB_565_Config;
    case BITMAP_FORMAT_NO_CONFIG:
    default:
      return SkBitmap::kNo_Config;
  }
}

}  //  namespace gfx
