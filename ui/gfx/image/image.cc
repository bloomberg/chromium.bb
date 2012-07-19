// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

#if defined(TOOLKIT_GTK)
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <glib-object.h>
#include "ui/gfx/canvas.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/cairo_cached_surface.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#endif

namespace gfx {

namespace internal {

#if defined(TOOLKIT_GTK)
const ImageSkia ImageSkiaFromGdkPixbuf(GdkPixbuf* pixbuf) {
  CHECK(pixbuf);
  gfx::Canvas canvas(gfx::Size(gdk_pixbuf_get_width(pixbuf),
                               gdk_pixbuf_get_height(pixbuf)), false);
  skia::ScopedPlatformPaint scoped_platform_paint(canvas.sk_canvas());
  cairo_t* cr = scoped_platform_paint.GetPlatformSurface();
  gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
  cairo_paint(cr);
  return ImageSkia(SkBitmap(canvas.ExtractBitmap()));
}
#endif

class ImageRepSkia;
class ImageRepGdk;
class ImageRepCairo;
class ImageRepCocoa;

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
  ImageRepSkia* AsImageRepSkia() {
    CHECK_EQ(type_, Image::kImageRepSkia);
    return reinterpret_cast<ImageRepSkia*>(this);
  }

#if defined(TOOLKIT_GTK)
  ImageRepGdk* AsImageRepGdk() {
    CHECK_EQ(type_, Image::kImageRepGdk);
    return reinterpret_cast<ImageRepGdk*>(this);
  }

  ImageRepCairo* AsImageRepCairo() {
    CHECK_EQ(type_, Image::kImageRepCairo);
    return reinterpret_cast<ImageRepCairo*>(this);
  }
#endif

#if defined(OS_MACOSX)
  ImageRepCocoa* AsImageRepCocoa() {
    CHECK_EQ(type_, Image::kImageRepCocoa);
    return reinterpret_cast<ImageRepCocoa*>(this);
  }
#endif

  Image::RepresentationType type() const { return type_; }

 private:
  Image::RepresentationType type_;
};

class ImageRepSkia : public ImageRep {
 public:
  // Takes ownership of |image|.
  explicit ImageRepSkia(ImageSkia* image)
      : ImageRep(Image::kImageRepSkia),
        image_(image) {
  }

  virtual ~ImageRepSkia() {
  }

  ImageSkia* image() { return image_.get(); }

 private:
  scoped_ptr<ImageSkia> image_;

  DISALLOW_COPY_AND_ASSIGN(ImageRepSkia);
};

#if defined(TOOLKIT_GTK)
class ImageRepGdk : public ImageRep {
 public:
  explicit ImageRepGdk(GdkPixbuf* pixbuf)
      : ImageRep(Image::kImageRepGdk),
        pixbuf_(pixbuf) {
    CHECK(pixbuf);
  }

  virtual ~ImageRepGdk() {
    if (pixbuf_) {
      g_object_unref(pixbuf_);
      pixbuf_ = NULL;
    }
  }

  GdkPixbuf* pixbuf() const { return pixbuf_; }

 private:
  GdkPixbuf* pixbuf_;

  DISALLOW_COPY_AND_ASSIGN(ImageRepGdk);
};

// Represents data that lives on the display server instead of in the client.
class ImageRepCairo : public ImageRep {
 public:
  explicit ImageRepCairo(GdkPixbuf* pixbuf)
      : ImageRep(Image::kImageRepCairo),
        cairo_cache_(new CairoCachedSurface) {
    CHECK(pixbuf);
    cairo_cache_->UsePixbuf(pixbuf);
  }

  virtual ~ImageRepCairo() {
    delete cairo_cache_;
  }

  CairoCachedSurface* surface() const { return cairo_cache_; }

 private:
  CairoCachedSurface* cairo_cache_;

  DISALLOW_COPY_AND_ASSIGN(ImageRepCairo);
};
#endif  // defined(TOOLKIT_GTK)

#if defined(OS_MACOSX)
class ImageRepCocoa : public ImageRep {
 public:
  explicit ImageRepCocoa(NSImage* image)
      : ImageRep(Image::kImageRepCocoa),
        image_(image) {
    CHECK(image);
  }

  virtual ~ImageRepCocoa() {
    base::mac::NSObjectRelease(image_);
    image_ = nil;
  }

  NSImage* image() const { return image_; }

 private:
  NSImage* image_;

  DISALLOW_COPY_AND_ASSIGN(ImageRepCocoa);
};
#endif  // defined(OS_MACOSX)

// The Storage class acts similarly to the pixels in a SkBitmap: the Image
// class holds a refptr instance of Storage, which in turn holds all the
// ImageReps. This way, the Image can be cheaply copied.
class ImageStorage : public base::RefCounted<ImageStorage> {
 public:
  ImageStorage(gfx::Image::RepresentationType default_type)
      : default_representation_type_(default_type) {
  }

  gfx::Image::RepresentationType default_representation_type() {
    return default_representation_type_;
  }
  gfx::Image::RepresentationMap& representations() { return representations_; }

 private:
  ~ImageStorage() {
    for (gfx::Image::RepresentationMap::iterator it = representations_.begin();
         it != representations_.end();
         ++it) {
      delete it->second;
    }
    representations_.clear();
  }

  // The type of image that was passed to the constructor. This key will always
  // exist in the |representations_| map.
  gfx::Image::RepresentationType default_representation_type_;

  // All the representations of an Image. Size will always be at least one, with
  // more for any converted representations.
  gfx::Image::RepresentationMap representations_;

  friend class base::RefCounted<ImageStorage>;
};

}  // namespace internal

Image::Image() {
  // |storage_| is NULL for empty Images.
}

Image::Image(const ImageSkia& image)
    : storage_(new internal::ImageStorage(Image::kImageRepSkia)) {
  internal::ImageRepSkia* rep = new internal::ImageRepSkia(
      new ImageSkia(image));
  AddRepresentation(rep);
}

Image::Image(const ImageSkiaRep& image_skia_rep)
    : storage_(new internal::ImageStorage(Image::kImageRepSkia)) {
  internal::ImageRepSkia* rep =
      new internal::ImageRepSkia(new ImageSkia(image_skia_rep));
  AddRepresentation(rep);
}

Image::Image(const SkBitmap& bitmap)
    : storage_(new internal::ImageStorage(Image::kImageRepSkia)) {
  internal::ImageRepSkia* rep =
      new internal::ImageRepSkia(new ImageSkia(bitmap));
  AddRepresentation(rep);
}

#if defined(TOOLKIT_GTK)
Image::Image(GdkPixbuf* pixbuf)
    : storage_(new internal::ImageStorage(Image::kImageRepGdk)) {
  internal::ImageRepGdk* rep = new internal::ImageRepGdk(pixbuf);
  AddRepresentation(rep);
}
#endif

#if defined(OS_MACOSX)
Image::Image(NSImage* image)
    : storage_(new internal::ImageStorage(Image::kImageRepCocoa)) {
  internal::ImageRepCocoa* rep = new internal::ImageRepCocoa(image);
  AddRepresentation(rep);
}
#endif

Image::Image(const Image& other) : storage_(other.storage_) {
}

Image& Image::operator=(const Image& other) {
  storage_ = other.storage_;
  return *this;
}

Image::~Image() {
}

const SkBitmap* Image::ToSkBitmap() const {
  // Possibly create and cache an intermediate ImageRepSkia.
  return ToImageSkia()->bitmap();
}

const ImageSkia* Image::ToImageSkia() const {
  internal::ImageRep* rep = GetRepresentation(kImageRepSkia, false);
  if (!rep) {
#if defined(TOOLKIT_GTK)
    internal::ImageRepGdk* native_rep =
        GetRepresentation(kImageRepGdk, true)->AsImageRepGdk();
    rep = new internal::ImageRepSkia(new ImageSkia(
        internal::ImageSkiaFromGdkPixbuf(native_rep->pixbuf())));
#elif defined(OS_MACOSX)
    internal::ImageRepCocoa* native_rep =
        GetRepresentation(kImageRepCocoa, true)->AsImageRepCocoa();
    rep = new internal::ImageRepSkia(new ImageSkia(
        ImageSkiaFromNSImage(native_rep->image())));
#endif
    CHECK(rep);
    AddRepresentation(rep);
  }
  return rep->AsImageRepSkia()->image();
}

#if defined(TOOLKIT_GTK)
GdkPixbuf* Image::ToGdkPixbuf() const {
  internal::ImageRep* rep = GetRepresentation(kImageRepGdk, false);
  if (!rep) {
    internal::ImageRepSkia* skia_rep =
        GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
    rep = new internal::ImageRepGdk(gfx::GdkPixbufFromSkBitmap(
        *skia_rep->image()->bitmap()));
    CHECK(rep);
    AddRepresentation(rep);
  }
  return rep->AsImageRepGdk()->pixbuf();
}

CairoCachedSurface* const Image::ToCairo() const {
  internal::ImageRep* rep = GetRepresentation(kImageRepCairo, false);
  if (!rep) {
    // Handle any-to-Cairo conversion. This may create and cache an intermediate
    // pixbuf before sending the data to the display server.
    rep = new internal::ImageRepCairo(ToGdkPixbuf());
    CHECK(rep);
    AddRepresentation(rep);
  }
  return rep->AsImageRepCairo()->surface();
}
#endif

#if defined(OS_MACOSX)
NSImage* Image::ToNSImage() const {
  internal::ImageRep* rep = GetRepresentation(kImageRepCocoa, false);
  if (!rep) {
    internal::ImageRepSkia* skia_rep =
        GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
    NSImage* image = NSImageFromImageSkia(*skia_rep->image());
    base::mac::NSObjectRetain(image);
    rep = new internal::ImageRepCocoa(image);
    CHECK(rep);
    AddRepresentation(rep);
  }
  return rep->AsImageRepCocoa()->image();
}
#endif

ImageSkia* Image::CopyImageSkia() const {
  return new ImageSkia(*ToImageSkia());
}

SkBitmap* Image::CopySkBitmap() const {
  return new SkBitmap(*ToSkBitmap());
}

#if defined(TOOLKIT_GTK)
GdkPixbuf* Image::CopyGdkPixbuf() const {
  GdkPixbuf* pixbuf = ToGdkPixbuf();
  g_object_ref(pixbuf);
  return pixbuf;
}
#endif

#if defined(OS_MACOSX)
NSImage* Image::CopyNSImage() const {
  NSImage* image = ToNSImage();
  base::mac::NSObjectRetain(image);
  return image;
}
#endif

#if defined(OS_MACOSX)
Image::operator NSImage*() const {
  return ToNSImage();
}
#endif

bool Image::HasRepresentation(RepresentationType type) const {
  return storage_.get() && storage_->representations().count(type) != 0;
}

size_t Image::RepresentationCount() const {
  if (!storage_.get())
    return 0;

  return storage_->representations().size();
}

bool Image::IsEmpty() const {
  return RepresentationCount() == 0;
}

void Image::SwapRepresentations(gfx::Image* other) {
  storage_.swap(other->storage_);
}

Image::RepresentationType Image::DefaultRepresentationType() const {
  CHECK(storage_.get());
  RepresentationType default_type = storage_->default_representation_type();
  // The conversions above assume that the default representation type is never
  // kImageRepCairo.
  DCHECK_NE(default_type, kImageRepCairo);
  return default_type;
}

internal::ImageRep* Image::GetRepresentation(
    RepresentationType rep_type, bool must_exist) const {
  CHECK(storage_.get());
  RepresentationMap::iterator it = storage_->representations().find(rep_type);
  if (it == storage_->representations().end()) {
    CHECK(!must_exist);
    return NULL;
  }
  return it->second;
}

void Image::AddRepresentation(internal::ImageRep* rep) const {
  CHECK(storage_.get());
  storage_->representations().insert(std::make_pair(rep->type(), rep));
}

}  // namespace gfx
