// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCOPED_IMAGE_H_
#define UI_GFX_SCOPED_IMAGE_H_
#pragma once

#include "base/basictypes.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_LINUX)
#include <glib-object.h>
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace gfx {

namespace internal {

// ScopedImage is class that encapsulates one of the three platform-specific
// images used: SkBitmap, NSImage, and GdkPixbuf. This is the abstract interface
// that all ScopedImages respond to. This wrapper expects to own the image it
// holds, unless it is Release()ed or Free()ed.
//
// This class is abstract and callers should use the specialized versions below,
// which are not in the internal namespace.
template <class ImageType>
class ScopedImage {
 public:
  virtual ~ScopedImage() {}

  // Frees the actual image that this boxes.
  virtual void Free() = 0;

  // Returns the image that this boxes.
  ImageType* Get() {
    return image_;
  }

  // Frees the current image and sets a new one.
  void Set(ImageType* new_image) {
    Free();
    image_ = new_image;
  }

  // Returns the image this boxes and relinquishes ownership.
  ImageType* Release() {
    ImageType* tmp = image_;
    image_ = NULL;
    return tmp;
  }

 protected:
  explicit ScopedImage(ImageType* image) : image_(image) {}
  ImageType* image_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedImage);
};

}  // namespace internal

// Generic template.
template <class ImageType = gfx::NativeImageType>
class ScopedImage : public gfx::internal::ScopedImage<ImageType> {
 public:
  explicit ScopedImage(gfx::NativeImage image)
      : gfx::internal::ScopedImage<ImageType>(image) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedImage<ImageType>);
};

// Specialization for SkBitmap on all platforms.
template <>
class ScopedImage<SkBitmap> : public gfx::internal::ScopedImage<SkBitmap> {
 public:
  explicit ScopedImage(SkBitmap* image)
      : gfx::internal::ScopedImage<SkBitmap>(image) {}
  virtual ~ScopedImage() {
    Free();
  }

  virtual void Free() {
    delete image_;
    image_ = NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedImage);
};

// Specialization for the NSImage type on Mac OS X.
#if defined(OS_MACOSX)
template <>
class ScopedImage<NSImage> : public gfx::internal::ScopedImage<NSImage> {
 public:
  explicit ScopedImage(NSImage* image)
      : gfx::internal::ScopedImage<NSImage>(image) {}
  virtual ~ScopedImage() {
    Free();
  }

  virtual void Free() {
    base::mac::NSObjectRelease(image_);
    image_ = NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedImage);
};
#endif  // defined(OS_MACOSX)

// Specialization for the GdkPixbuf type on Linux.
#if defined(OS_LINUX)
template <>
class ScopedImage<GdkPixbuf> : public gfx::internal::ScopedImage<GdkPixbuf> {
 public:
  explicit ScopedImage(GdkPixbuf* image)
      : gfx::internal::ScopedImage<GdkPixbuf>(image) {}
  virtual ~ScopedImage() {
    Free();
  }

  virtual void Free() {
    if (image_) {
      g_object_unref(image_);
      image_ = NULL;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedImage);
};
#endif  // defined(OS_LINUX)

// Typedef ScopedNativeImage to the default template argument. This allows for
// easy exchange between gfx::NativeImage and a gfx::ScopedNativeImage.
typedef ScopedImage<> ScopedNativeImage;

}  // namespace gfx

#endif  // UI_GFX_SCOPED_IMAGE_H_
