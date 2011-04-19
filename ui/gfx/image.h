// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An Image wraps an image any flavor, be it platform-native GdkBitmap/NSImage,
// or a SkBitmap. This also provides easy conversion to other image types
// through operator overloading. It will cache the converted representations
// internally to prevent double-conversion.
//
// The lifetime of both the initial representation and any converted ones are
// tied to the lifetime of the Image object itself.

#ifndef UI_GFX_IMAGE_H_
#define UI_GFX_IMAGE_H_
#pragma once

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"  // Forward-declares GdkPixbuf and NSImage.

class SkBitmap;

namespace {
class ImageTest;
class ImageMacTest;
}

namespace gfx {

namespace internal {
class ImageRep;
}

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
  // To create an Image that supports multiple resolutions pass a vector
  // of bitmaps, one for each resolution.
  explicit Image(const std::vector<const SkBitmap*>& bitmaps);

#if defined(OS_LINUX)
  // Does not increase |pixbuf|'s reference count; expects to take ownership.
  explicit Image(GdkPixbuf* pixbuf);
#elif defined(OS_MACOSX)
  // Does not retain |image|; expects to take ownership.
  // A single NSImage object can contain multiple bitmaps so there's no reason
  // to pass a vector of these.
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

  // Gets the number of bitmaps in this image. This may cause a conversion
  // to a bitmap representation. Note, this function and GetSkBitmapAtIndex()
  // are primarily meant to be used by the theme provider.
  size_t GetNumberOfSkBitmaps();

  // Gets the bitmap at the given index. This may cause a conversion
  // to a bitmap representation. Note, the internal ordering of bitmaps is not
  // guaranteed.
  const SkBitmap* GetSkBitmapAtIndex(size_t index);

  // Inspects the representations map to see if the given type exists.
  bool HasRepresentation(RepresentationType type);

  // Swaps this image's internal representations with |other|.
  void SwapRepresentations(gfx::Image* other);

 private:
  typedef std::map<RepresentationType, internal::ImageRep*> RepresentationMap;

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

  // All the representations of an Image. Size will always be at least one, with
  // more for any converted representations.
  RepresentationMap representations_;

  friend class ::ImageTest;
  friend class ::ImageMacTest;
  DISALLOW_COPY_AND_ASSIGN(Image);
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_H_
