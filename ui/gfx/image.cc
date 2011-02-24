// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_LINUX)
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/gtk_util.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace gfx {

namespace internal {

#if defined(OS_MACOSX)
// This is a wrapper around gfx::NSImageToSkBitmap() because this cross-platform
// file cannot include the [square brackets] of ObjC.
const SkBitmap* NSImageToSkBitmap(NSImage* image);
#endif

#if defined(OS_LINUX)
const SkBitmap* GdkPixbufToSkBitmap(GdkPixbuf* pixbuf) {
  gfx::CanvasSkia canvas(gdk_pixbuf_get_width(pixbuf),
                         gdk_pixbuf_get_height(pixbuf),
                           false);
  canvas.DrawGdkPixbuf(pixbuf, 0, 0);
  return new SkBitmap(canvas.ExtractBitmap());
}
#endif

class SkBitmapRep;
class GdkPixbufRep;
class NSImageRep;

// An ImageRep is the object that holds the backing memory for an Image. Each
// RepresentationType has an ImageRep subclass that is responsible for freeing
// the memory that the ImageRep holds. When an ImageRep is created, it expects
// to take ownership of the image, without having to retain it or increase its
// reference count.
class ImageRep {
 public:
  explicit ImageRep(Image::RepresentationType rep) : type_(rep) {}

  // Deletes the associated pixels of an ImageRep.
  virtual ~ImageRep() {}

  // Cast helpers ("fake RTTI").
  SkBitmapRep* AsSkBitmapRep() {
    CHECK_EQ(type_, Image::kSkBitmapRep);
    return reinterpret_cast<SkBitmapRep*>(this);
  }

#if defined(OS_LINUX)
  GdkPixbufRep* AsGdkPixbufRep() {
    CHECK_EQ(type_, Image::kGdkPixbufRep);
    return reinterpret_cast<GdkPixbufRep*>(this);
  }
#endif

#if defined(OS_MACOSX)
  NSImageRep* AsNSImageRep() {
    CHECK_EQ(type_, Image::kNSImageRep);
    return reinterpret_cast<NSImageRep*>(this);
  }
#endif

  Image::RepresentationType type() const { return type_; }

 private:
  Image::RepresentationType type_;
};

class SkBitmapRep : public ImageRep {
 public:
  explicit SkBitmapRep(const SkBitmap* bitmap)
      : ImageRep(Image::kSkBitmapRep),
        bitmap_(bitmap) {
    CHECK(bitmap);
  }

  virtual ~SkBitmapRep() {
    delete bitmap_;
    bitmap_ = NULL;
  }

  const SkBitmap* bitmap() const { return bitmap_; }

 private:
  const SkBitmap* bitmap_;

  DISALLOW_COPY_AND_ASSIGN(SkBitmapRep);
};

#if defined(OS_LINUX)
class GdkPixbufRep : public ImageRep {
 public:
  explicit GdkPixbufRep(GdkPixbuf* pixbuf)
      : ImageRep(Image::kGdkPixbufRep),
        pixbuf_(pixbuf) {
    CHECK(pixbuf);
  }

  virtual ~GdkPixbufRep() {
    if (pixbuf_) {
      g_object_unref(pixbuf_);
      pixbuf_ = NULL;
    }
  }

  GdkPixbuf* pixbuf() const { return pixbuf_; }

 private:
  GdkPixbuf* pixbuf_;

  DISALLOW_COPY_AND_ASSIGN(GdkPixbufRep);
};
#endif

#if defined(OS_MACOSX)
class NSImageRep : public ImageRep {
 public:
  explicit NSImageRep(NSImage* image)
      : ImageRep(Image::kNSImageRep),
        image_(image) {
    CHECK(image);
  }

  virtual ~NSImageRep() {
    base::mac::NSObjectRelease(image_);
    image_ = nil;
  }

  NSImage* image() const { return image_; }

 private:
  NSImage* image_;

  DISALLOW_COPY_AND_ASSIGN(NSImageRep);
};
#endif

}  // namespace internal

Image::Image(const SkBitmap* bitmap)
    : default_representation_(Image::kSkBitmapRep) {
  internal::SkBitmapRep* rep = new internal::SkBitmapRep(bitmap);
  AddRepresentation(rep);
}

#if defined(OS_LINUX)
Image::Image(GdkPixbuf* pixbuf)
    : default_representation_(Image::kGdkPixbufRep) {
  internal::GdkPixbufRep* rep = new internal::GdkPixbufRep(pixbuf);
  AddRepresentation(rep);
}
#endif

#if defined(OS_MACOSX)
Image::Image(NSImage* image) : default_representation_(Image::kNSImageRep) {
  internal::NSImageRep* rep = new internal::NSImageRep(image);
  AddRepresentation(rep);
}
#endif

Image::~Image() {
  for (RepresentationMap::iterator it = representations_.begin();
       it != representations_.end(); ++it) {
    delete it->second;
  }
  representations_.clear();
}

Image::operator const SkBitmap*() {
  internal::ImageRep* rep = GetRepresentation(Image::kSkBitmapRep);
  return rep->AsSkBitmapRep()->bitmap();
}

Image::operator const SkBitmap&() {
  return *(this->operator const SkBitmap*());
}

#if defined(OS_LINUX)
Image::operator GdkPixbuf*() {
  internal::ImageRep* rep = GetRepresentation(Image::kGdkPixbufRep);
  return rep->AsGdkPixbufRep()->pixbuf();
}
#endif

#if defined(OS_MACOSX)
Image::operator NSImage*() {
  internal::ImageRep* rep = GetRepresentation(Image::kNSImageRep);
  return rep->AsNSImageRep()->image();
}
#endif

bool Image::HasRepresentation(RepresentationType type) {
  return representations_.count(type) != 0;
}

internal::ImageRep* Image::DefaultRepresentation() {
  RepresentationMap::iterator it =
      representations_.find(default_representation_);
  DCHECK(it != representations_.end());
  return it->second;
}

internal::ImageRep* Image::GetRepresentation(RepresentationType rep_type) {
  // If the requested rep is the default, return it.
  internal::ImageRep* default_rep = DefaultRepresentation();
  if (rep_type == default_representation_)
    return default_rep;

  // Check to see if the representation already exists.
  RepresentationMap::iterator it = representations_.find(rep_type);
  if (it != representations_.end())
    return it->second;

  // At this point, the requested rep does not exist, so it must be converted
  // from the default rep.

  // Handle native-to-Skia conversion.
  if (rep_type == Image::kSkBitmapRep) {
    internal::SkBitmapRep* rep = NULL;
#if defined(OS_LINUX)
    if (default_representation_ == Image::kGdkPixbufRep) {
      internal::GdkPixbufRep* pixbuf_rep = default_rep->AsGdkPixbufRep();
      rep = new internal::SkBitmapRep(
          internal::GdkPixbufToSkBitmap(pixbuf_rep->pixbuf()));
    }
#elif defined(OS_MACOSX)
    if (default_representation_ == Image::kNSImageRep) {
      internal::NSImageRep* nsimage_rep = default_rep->AsNSImageRep();
      rep = new internal::SkBitmapRep(
          internal::NSImageToSkBitmap(nsimage_rep->image()));
    }
#endif
    if (rep) {
      AddRepresentation(rep);
      return rep;
    }
    NOTREACHED();
  }

  // Handle Skia-to-native conversions.
  if (default_rep->type() == Image::kSkBitmapRep) {
    internal::SkBitmapRep* skia_rep = default_rep->AsSkBitmapRep();
    internal::ImageRep* native_rep = NULL;
#if defined(OS_LINUX)
    if (rep_type == Image::kGdkPixbufRep) {
      GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(skia_rep->bitmap());
      native_rep = new internal::GdkPixbufRep(pixbuf);
    }
#elif defined(OS_MACOSX)
    if (rep_type == Image::kNSImageRep) {
      NSImage* image = gfx::SkBitmapToNSImage(*(skia_rep->bitmap()));
      base::mac::NSObjectRetain(image);
      native_rep = new internal::NSImageRep(image);
    }
#endif
    if (native_rep) {
      AddRepresentation(native_rep);
      return native_rep;
    }
    NOTREACHED();
  }

  // Something went seriously wrong...
  return NULL;
}

void Image::AddRepresentation(internal::ImageRep* rep) {
  representations_.insert(std::make_pair(rep->type(), rep));
}

}  // namespace gfx
