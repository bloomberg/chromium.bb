// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_JAVA_BITMAP_H_
#define UI_GFX_ANDROID_JAVA_BITMAP_H_

#include "base/android/scoped_java_ref.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace gfx {

// This class wraps a JNI AndroidBitmap object to make it easier to use. It
// handles locking and unlocking of the underlying pixels, along with wrapping
// various JNI methods.
UI_EXPORT class JavaBitmap {
 public:
  explicit JavaBitmap(jobject bitmap);
  ~JavaBitmap();

  void* pixels() { return pixels_; }
  gfx::Size Size() const;
  // Formats are in android/bitmap.h; e.g. ANDROID_BITMAP_FORMAT_RGBA_8888
  int Format() const;
  uint32_t Stride() const;

 private:
  jobject bitmap_;
  void* pixels_;

  DISALLOW_COPY_AND_ASSIGN(JavaBitmap);
};

base::android::ScopedJavaLocalRef<jobject> CreateJavaBitmap(
    const gfx::Size& size);

base::android::ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(
    const SkBitmap* skbitmap);

}  // namespace gfx

#endif  // UI_GFX_ANDROID_JAVA_BITMAP_H_
