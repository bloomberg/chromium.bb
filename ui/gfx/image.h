// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_H_
#define UI_GFX_IMAGE_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"  // Forward-declares GdkPixbuf and NSImage.

class SkBitmap;

namespace {
class ImageTest;
}

namespace gfx {

namespace internal {
class ImageRep;
}

// An Image wraps an image any flavor, be it platform-native GdkBitmap/NSImage,
// or a SkBitmap. This also provides easy conversion to other image types
// through operator overloading. It will cache the converted representations
// internally to prevent double-conversion.
//
// The lifetime of both the initial representation and any converted ones are
// tied to the lifetime of the Image object itself.
class Image {
 public:
  enum RepresentationType {
    kGdkPixbufRep,
    kNSImageRep,
    kSkBitmapRep,
  };

  // Creates a new image with the default representation. The object will take
  // ownership of the image.
  explicit Image(const SkBitmap* bitmap);
#if defined(OS_LINUX)
  // Does not increase |pixbuf|'s reference count; expects to take ownership.
  explicit Image(GdkPixbuf* pixbuf);
#elif defined(OS_MACOSX)
  // Does not retain |image|; expects to take ownership.
  explicit Image(NSImage* image);
#endif

  // Deletes the image and all of its cached representations.
  ~Image();

  // Conversion handlers.
  operator const SkBitmap*();
  operator const SkBitmap&();
#if defined(OS_LINUX)
  operator GdkPixbuf*();
#elif defined(OS_MACOSX)
  operator NSImage*();
#endif

  // Inspects the representations map to see if the given type exists.
  bool HasRepresentation(RepresentationType type);

 private:
  friend class ::ImageTest;

  // Returns the ImageRep for the default representation.
  internal::ImageRep* DefaultRepresentation();

  // Returns a ImageRep for the given representation type, converting and
  // caching if necessary.
  internal::ImageRep* GetRepresentation(RepresentationType rep);

  // Stores a representation into the map.
  void AddRepresentation(internal::ImageRep* rep);

  // The type of image that was passed to the constructor. This key will always
  // exist in the |representations_| map.
  RepresentationType default_representation_;

  typedef std::map<RepresentationType, internal::ImageRep*> RepresentationMap;
  // All the representations of an Image. Size will always be at least one, with
  // more for any converted representations.
  RepresentationMap representations_;

  DISALLOW_COPY_AND_ASSIGN(Image);
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_H_
