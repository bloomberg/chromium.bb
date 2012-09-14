// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

#if !defined(OS_IOS)
#include "ui/gfx/codec/png_codec.h"
#endif

#if defined(TOOLKIT_GTK)
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <glib-object.h>
#include "ui/base/gtk/scoped_gobject.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/cairo_cached_surface.h"
#elif defined(OS_IOS)
#include "base/mac/foundation_util.h"
#include "ui/gfx/image/image_skia_util_ios.h"
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
                               gdk_pixbuf_get_height(pixbuf)),
                     ui::SCALE_FACTOR_100P,
                     false);
  skia::ScopedPlatformPaint scoped_platform_paint(canvas.sk_canvas());
  cairo_t* cr = scoped_platform_paint.GetPlatformSurface();
  gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
  cairo_paint(cr);
  return ImageSkia(canvas.ExtractImageRep());
}

GdkPixbuf* GdkPixbufFromPNG(const std::vector<unsigned char>& png) {
  GdkPixbuf* pixbuf = NULL;
  ui::ScopedGObject<GdkPixbufLoader>::Type loader(gdk_pixbuf_loader_new());

  bool ok = gdk_pixbuf_loader_write(loader.get(),
      reinterpret_cast<const guint8*>(&png.front()), png.size(), NULL);

  // Calling gdk_pixbuf_loader_close forces the data to be parsed by the
  // loader. This must be done before calling gdk_pixbuf_loader_get_pixbuf.
  if (ok)
    ok = gdk_pixbuf_loader_close(loader.get(), NULL);
  if (ok)
    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader.get());

  if (pixbuf) {
    // The pixbuf is owned by the scoped loader which will delete its ref when
    // it goes out of scope. Add a ref so that the pixbuf still exists.
    g_object_ref(pixbuf);
  } else {
    LOG(WARNING) << "Unable to decode PNG.";
    // Return a 16x16 red image to visually show error.
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 16, 16);
    gdk_pixbuf_fill(pixbuf, 0xff0000ff);
  }

  return pixbuf;
}

void PNGFromGdkPixbuf(GdkPixbuf* pixbuf, std::vector<unsigned char>* png) {
  gchar* image = NULL;
  gsize image_size;
  GError* error = NULL;
  CHECK(gdk_pixbuf_save_to_buffer(
      pixbuf, &image, &image_size, "png", &error, NULL));
  png->assign(image, image + image_size);
  g_free(image);
}

#endif // defined(TOOLKIT_GTK)

#if defined(OS_IOS)
void PNGFromUIImage(UIImage* nsimage, std::vector<unsigned char>* png);
UIImage* CreateUIImageFromPNG(const std::vector<unsigned char>& png);
#elif defined(OS_MACOSX)
void PNGFromNSImage(NSImage* nsimage, std::vector<unsigned char>* png);
NSImage* NSImageFromPNG(const std::vector<unsigned char>& png);
#endif // defined(OS_MACOSX)

#if defined(OS_IOS)
ImageSkia* ImageSkiaFromPNG(const std::vector<unsigned char>& png);
void PNGFromImageSkia(const ImageSkia* skia, std::vector<unsigned char>* png);
#else
ImageSkia* ImageSkiaFromPNG(const std::vector<unsigned char>& png) {
  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(&png.front(), png.size(), &bitmap)) {
    LOG(WARNING) << "Unable to decode PNG.";
    // Return a 16x16 red image to visually show error.
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
    bitmap.allocPixels();
    bitmap.eraseRGB(0xff, 0, 0);
  }
  return new ImageSkia(bitmap);
}

void PNGFromImageSkia(const ImageSkia* skia, std::vector<unsigned char>* png) {
  CHECK(gfx::PNGCodec::EncodeBGRASkBitmap(*skia->bitmap(), false, png));
}
#endif

class ImageRepPNG;
class ImageRepSkia;
class ImageRepGdk;
class ImageRepCairo;
class ImageRepCocoa;
class ImageRepCocoaTouch;

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
  ImageRepPNG* AsImageRepPNG() {
    CHECK_EQ(type_, Image::kImageRepPNG);
    return reinterpret_cast<ImageRepPNG*>(this);
  }

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

#if defined(OS_IOS)
  ImageRepCocoaTouch* AsImageRepCocoaTouch() {
    CHECK_EQ(type_, Image::kImageRepCocoaTouch);
    return reinterpret_cast<ImageRepCocoaTouch*>(this);
  }
#elif defined(OS_MACOSX)
  ImageRepCocoa* AsImageRepCocoa() {
    CHECK_EQ(type_, Image::kImageRepCocoa);
    return reinterpret_cast<ImageRepCocoa*>(this);
  }
#endif

  Image::RepresentationType type() const { return type_; }

 private:
  Image::RepresentationType type_;
};

class ImageRepPNG : public ImageRep {
 public:
  ImageRepPNG(const unsigned char* input, size_t input_size)
      : ImageRep(Image::kImageRepPNG),
        image_(input, input + input_size) {
  }
  ImageRepPNG() : ImageRep(Image::kImageRepPNG) {
  }

  virtual ~ImageRepPNG() {
  }

  std::vector<unsigned char>* image() { return &image_; }

 private:
  std::vector<unsigned char> image_;

  DISALLOW_COPY_AND_ASSIGN(ImageRepPNG);
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

#if defined(OS_IOS)
class ImageRepCocoaTouch : public ImageRep {
 public:
  explicit ImageRepCocoaTouch(UIImage* image)
      : ImageRep(Image::kImageRepCocoaTouch),
        image_(image) {
    CHECK(image);
  }

  virtual ~ImageRepCocoaTouch() {
    base::mac::NSObjectRelease(image_);
    image_ = nil;
  }

  UIImage* image() const { return image_; }

 private:
  UIImage* image_;

  DISALLOW_COPY_AND_ASSIGN(ImageRepCocoaTouch);
};
#elif defined(OS_MACOSX)
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
      : default_representation_type_(default_type),
        representations_deleter_(&representations_) {
  }

  gfx::Image::RepresentationType default_representation_type() {
    return default_representation_type_;
  }
  gfx::Image::RepresentationMap& representations() { return representations_; }

 private:
  friend class base::RefCounted<ImageStorage>;

  ~ImageStorage() {}

  // The type of image that was passed to the constructor. This key will always
  // exist in the |representations_| map.
  gfx::Image::RepresentationType default_representation_type_;

  // All the representations of an Image. Size will always be at least one, with
  // more for any converted representations.
  gfx::Image::RepresentationMap representations_;

  STLValueDeleter<Image::RepresentationMap> representations_deleter_;

  DISALLOW_COPY_AND_ASSIGN(ImageStorage);
};

}  // namespace internal

Image::Image() {
  // |storage_| is NULL for empty Images.
}

Image::Image(const unsigned char* png, size_t input_size)
    : storage_(new internal::ImageStorage(Image::kImageRepPNG)) {
  internal::ImageRepPNG* rep = new internal::ImageRepPNG(png, input_size);
  AddRepresentation(rep);
}

Image::Image(const ImageSkia& image) {
  if (!image.isNull()) {
    storage_ = new internal::ImageStorage(Image::kImageRepSkia);
    internal::ImageRepSkia* rep = new internal::ImageRepSkia(
        new ImageSkia(image));
    AddRepresentation(rep);
  }
}

Image::Image(const SkBitmap& bitmap) {
  if (!bitmap.empty()) {
    storage_ = new internal::ImageStorage(Image::kImageRepSkia);
    internal::ImageRepSkia* rep =
        new internal::ImageRepSkia(new ImageSkia(bitmap));
    AddRepresentation(rep);
  }
}

#if defined(TOOLKIT_GTK)
Image::Image(GdkPixbuf* pixbuf) {
  if (pixbuf) {
    storage_ = new internal::ImageStorage(Image::kImageRepGdk);
    internal::ImageRepGdk* rep = new internal::ImageRepGdk(pixbuf);
    AddRepresentation(rep);
  }
}
#endif

#if defined(OS_IOS)
Image::Image(UIImage* image)
    : storage_(new internal::ImageStorage(Image::kImageRepCocoaTouch)) {
  if (image) {
    internal::ImageRepCocoaTouch* rep = new internal::ImageRepCocoaTouch(image);
    AddRepresentation(rep);
  }
}
#elif defined(OS_MACOSX)
Image::Image(NSImage* image) {
  if (image) {
    storage_ = new internal::ImageStorage(Image::kImageRepCocoa);
    internal::ImageRepCocoa* rep = new internal::ImageRepCocoa(image);
    AddRepresentation(rep);
  }
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

const std::vector<unsigned char>* Image::ToImagePNG() const {
  internal::ImageRep* rep = GetRepresentation(kImageRepPNG, false);
  if (!rep) {
    internal::ImageRepPNG* png_rep = new internal::ImageRepPNG();
    switch (DefaultRepresentationType()) {
#if defined(TOOLKIT_GTK)
      case kImageRepGdk: {
        internal::ImageRepGdk* gdk_rep =
            GetRepresentation(kImageRepGdk, true)->AsImageRepGdk();
        internal::PNGFromGdkPixbuf(gdk_rep->pixbuf(), png_rep->image());
        break;
      }
#elif defined(OS_IOS)
      case kImageRepCocoaTouch: {
        internal::ImageRepCocoaTouch* cocoa_touch_rep =
            GetRepresentation(kImageRepCocoaTouch, true)
                ->AsImageRepCocoaTouch();
        internal::PNGFromUIImage(cocoa_touch_rep->image(), png_rep->image());
        break;
      }
#elif defined(OS_MACOSX)
      case kImageRepCocoa: {
        internal::ImageRepCocoa* cocoa_rep =
            GetRepresentation(kImageRepCocoa, true)->AsImageRepCocoa();
        internal::PNGFromNSImage(cocoa_rep->image(), png_rep->image());
        break;
      }
#endif
      case kImageRepSkia: {
        internal::ImageRepSkia* skia_rep =
            GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
        internal::PNGFromImageSkia(skia_rep->image(), png_rep->image());
        break;
      }
      default:
        NOTREACHED();
    }
    rep = png_rep;
    CHECK(rep);
    AddRepresentation(rep);
  }
  return rep->AsImageRepPNG()->image();
}

const SkBitmap* Image::ToSkBitmap() const {
  // Possibly create and cache an intermediate ImageRepSkia.
  return ToImageSkia()->bitmap();
}

const ImageSkia* Image::ToImageSkia() const {
  internal::ImageRep* rep = GetRepresentation(kImageRepSkia, false);
  if (!rep) {
    switch (DefaultRepresentationType()) {
      case kImageRepPNG: {
        internal::ImageRepPNG* png_rep =
            GetRepresentation(kImageRepPNG, true)->AsImageRepPNG();
        rep = new internal::ImageRepSkia(
            internal::ImageSkiaFromPNG(*png_rep->image()));
        break;
      }
#if defined(TOOLKIT_GTK)
      case kImageRepGdk: {
        internal::ImageRepGdk* native_rep =
            GetRepresentation(kImageRepGdk, true)->AsImageRepGdk();
        rep = new internal::ImageRepSkia(new ImageSkia(
            internal::ImageSkiaFromGdkPixbuf(native_rep->pixbuf())));
        break;
      }
#elif defined(OS_IOS)
      case kImageRepCocoaTouch: {
        internal::ImageRepCocoaTouch* native_rep =
            GetRepresentation(kImageRepCocoaTouch, true)
                ->AsImageRepCocoaTouch();
        rep = new internal::ImageRepSkia(new ImageSkia(
            ImageSkiaFromUIImage(native_rep->image())));
        break;
      }
#elif defined(OS_MACOSX)
      case kImageRepCocoa: {
        internal::ImageRepCocoa* native_rep =
            GetRepresentation(kImageRepCocoa, true)->AsImageRepCocoa();
        rep = new internal::ImageRepSkia(new ImageSkia(
            ImageSkiaFromNSImage(native_rep->image())));
        break;
      }
#endif
      default:
        NOTREACHED();
    }
    CHECK(rep);
    AddRepresentation(rep);
  }
  return rep->AsImageRepSkia()->image();
}

#if defined(TOOLKIT_GTK)
GdkPixbuf* Image::ToGdkPixbuf() const {
  internal::ImageRep* rep = GetRepresentation(kImageRepGdk, false);
  if (!rep) {
    switch (DefaultRepresentationType()) {
      case kImageRepPNG: {
        internal::ImageRepPNG* png_rep =
            GetRepresentation(kImageRepPNG, true)->AsImageRepPNG();
        rep = new internal::ImageRepGdk(internal::GdkPixbufFromPNG(
            *png_rep->image()));
        break;
      }
      case kImageRepSkia: {
        internal::ImageRepSkia* skia_rep =
            GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
        rep = new internal::ImageRepGdk(gfx::GdkPixbufFromSkBitmap(
            *skia_rep->image()->bitmap()));
        break;
      }
      default:
        NOTREACHED();
    }
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

#if defined(OS_IOS)
UIImage* Image::ToUIImage() const {
  internal::ImageRep* rep = GetRepresentation(kImageRepCocoaTouch, false);
  if (!rep) {
    switch (DefaultRepresentationType()) {
      case kImageRepPNG: {
        internal::ImageRepPNG* png_rep =
            GetRepresentation(kImageRepPNG, true)->AsImageRepPNG();
        rep = new internal::ImageRepCocoaTouch(internal::CreateUIImageFromPNG(
            *png_rep->image()));
        break;
      }
      case kImageRepSkia: {
        internal::ImageRepSkia* skia_rep =
            GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
        UIImage* image = UIImageFromImageSkia(*skia_rep->image());
        base::mac::NSObjectRetain(image);
        rep = new internal::ImageRepCocoaTouch(image);
        break;
      }
      default:
        NOTREACHED();
    }
    CHECK(rep);
    AddRepresentation(rep);
  }
  return rep->AsImageRepCocoaTouch()->image();
}
#elif defined(OS_MACOSX)
NSImage* Image::ToNSImage() const {
  internal::ImageRep* rep = GetRepresentation(kImageRepCocoa, false);
  if (!rep) {
    switch (DefaultRepresentationType()) {
      case kImageRepPNG: {
        internal::ImageRepPNG* png_rep =
            GetRepresentation(kImageRepPNG, true)->AsImageRepPNG();
        rep = new internal::ImageRepCocoa(internal::NSImageFromPNG(
            *png_rep->image()));
        break;
      }
      case kImageRepSkia: {
        internal::ImageRepSkia* skia_rep =
            GetRepresentation(kImageRepSkia, true)->AsImageRepSkia();
        NSImage* image = NSImageFromImageSkia(*skia_rep->image());
        base::mac::NSObjectRetain(image);
        rep = new internal::ImageRepCocoa(image);
        break;
      }
      default:
        NOTREACHED();
    }
    CHECK(rep);
    AddRepresentation(rep);
  }
  return rep->AsImageRepCocoa()->image();
}
#endif

std::vector<unsigned char>* Image::CopyImagePNG() const {
  return new std::vector<unsigned char>(*ToImagePNG());
}

SkBitmap Image::AsBitmap() const {
  return IsEmpty() ? SkBitmap() : *ToSkBitmap();
}

ImageSkia Image::AsImageSkia() const {
  return IsEmpty() ? ImageSkia() : *ToImageSkia();
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
NSImage* Image::AsNSImage() const {
  return IsEmpty() ? nil : ToNSImage();
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

#if defined(OS_IOS)
UIImage* Image::CopyUIImage() const {
  UIImage* image = ToUIImage();
  base::mac::NSObjectRetain(image);
  return image;
}
#elif defined(OS_MACOSX)
NSImage* Image::CopyNSImage() const {
  NSImage* image = ToNSImage();
  base::mac::NSObjectRetain(image);
  return image;
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
