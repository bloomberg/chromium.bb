// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_JAVA_BITMAP_H_
#define UI_GFX_ANDROID_JAVA_BITMAP_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace gfx {

// This class wraps a JNI AndroidBitmap object to make it easier to use. It
// handles locking and unlocking of the underlying pixels, along with wrapping
// various JNI methods.
class GFX_EXPORT JavaBitmap {
 public:
  explicit JavaBitmap(jobject bitmap);
  ~JavaBitmap();

  inline void* pixels() { return pixels_; }
  inline const gfx::Size& size() const { return size_; }
  // Formats are in android/bitmap.h; e.g. ANDROID_BITMAP_FORMAT_RGBA_8888
  inline int format() const { return format_; }
  inline uint32_t stride() const { return stride_; }

  // Registers methods with JNI and returns true if succeeded.
  static bool RegisterJavaBitmap(JNIEnv* env);

 private:
  jobject bitmap_;
  void* pixels_;
  gfx::Size size_;
  int format_;
  uint32_t stride_;

  DISALLOW_COPY_AND_ASSIGN(JavaBitmap);
};

GFX_EXPORT base::android::ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(
    const SkBitmap* skbitmap);

GFX_EXPORT SkBitmap CreateSkBitmapFromJavaBitmap(JavaBitmap& jbitmap);

// If the resource loads successfully, it will be resized to |size|.
// Note: If the source resource is smaller than |size|, quality may suffer.
GFX_EXPORT SkBitmap CreateSkBitmapFromResource(const char* name,
                                               gfx::Size size);

}  // namespace gfx

#endif  // UI_GFX_ANDROID_JAVA_BITMAP_H_
