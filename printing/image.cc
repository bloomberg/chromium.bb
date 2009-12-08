// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include "app/gfx/codec/png_codec.h"
#include "base/file_util.h"
#include "base/gfx/rect.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "skia/ext/platform_device.h"

#if defined(OS_WIN)
#include "app/gfx/gdi_util.h"  // EMF support
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#include "base/scoped_cftyperef.h"
#endif

namespace {

// A simple class which temporarily overrides system settings.
// The bitmap image rendered via the PlayEnhMetaFile() function depends on
// some system settings.
// As a workaround for such dependency, this class saves the system settings
// and changes them. This class also restore the saved settings in its
// destructor.
class DisableFontSmoothing {
 public:
  explicit DisableFontSmoothing(bool disable) : enable_again_(false) {
    if (disable) {
#if defined(OS_WIN)
      BOOL enabled;
      if (SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &enabled, 0) &&
          enabled) {
        if (SystemParametersInfo(SPI_SETFONTSMOOTHING, FALSE, NULL, 0))
          enable_again_ = true;
      }
#endif
    }
  }

  ~DisableFontSmoothing() {
    if (enable_again_) {
#if defined(OS_WIN)
      BOOL result = SystemParametersInfo(SPI_SETFONTSMOOTHING, TRUE, NULL, 0);
      DCHECK(result);
#endif
    }
  }

 private:
  bool enable_again_;

  DISALLOW_COPY_AND_ASSIGN(DisableFontSmoothing);
};

}  // namespace

namespace printing {

Image::Image(const std::wstring& filename)
    : row_length_(0),
      ignore_alpha_(true) {
  std::string data;
  file_util::ReadFileToString(filename, &data);
  std::wstring ext = file_util::GetFileExtensionFromPath(filename);
  bool success = false;
  if (LowerCaseEqualsASCII(ext, "png")) {
    success = LoadPng(data);
  } else if (LowerCaseEqualsASCII(ext, "emf")) {
    success = LoadMetafile(data);
  } else {
    DCHECK(false);
  }
  if (!success) {
    size_.SetSize(0, 0);
    row_length_ = 0;
    data_.clear();
  }
}

Image::Image(const NativeMetafile& metafile)
    : row_length_(0),
      ignore_alpha_(true) {
  LoadMetafile(metafile);
}

Image::Image(const Image& image)
    : size_(image.size_),
      row_length_(image.row_length_),
      data_(image.data_),
      ignore_alpha_(image.ignore_alpha_) {
}

std::string Image::checksum() const {
  MD5Digest digest;
  MD5Sum(&data_[0], data_.size(), &digest);
  return HexEncode(&digest, sizeof(digest));
}

bool Image::SaveToPng(const FilePath& filepath) const {
  DCHECK(!data_.empty());
  std::vector<unsigned char> compressed;
  bool success = gfx::PNGCodec::Encode(&*data_.begin(),
                                       gfx::PNGCodec::FORMAT_BGRA,
                                       size_.width(),
                                       size_.height(),
                                       row_length_,
                                       true,
                                       &compressed);
  DCHECK(success && compressed.size());
  if (success) {
    int write_bytes = file_util::WriteFile(
        filepath,
        reinterpret_cast<char*>(&*compressed.begin()), compressed.size());
    success = (write_bytes == static_cast<int>(compressed.size()));
    DCHECK(success);
  }
  return success;
}

double Image::PercentageDifferent(const Image& rhs) const {
  if (size_.width() == 0 || size_.height() == 0 ||
    rhs.size_.width() == 0 || rhs.size_.height() == 0)
    return 100.;

  int width = std::min(size_.width(), rhs.size_.width());
  int height = std::min(size_.height(), rhs.size_.height());
  // Compute pixels different in the overlap
  int pixels_different = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      uint32 lhs_pixel = pixel_at(x, y);
      uint32 rhs_pixel = rhs.pixel_at(x, y);
      if (lhs_pixel != rhs_pixel)
        ++pixels_different;
    }

    // Look for extra right lhs pixels. They should be white.
    for (int x = width; x < size_.width(); ++x) {
      uint32 lhs_pixel = pixel_at(x, y);
      if (lhs_pixel != Color(SK_ColorWHITE))
        ++pixels_different;
    }

    // Look for extra right rhs pixels. They should be white.
    for (int x = width; x < rhs.size_.width(); ++x) {
      uint32 rhs_pixel = rhs.pixel_at(x, y);
      if (rhs_pixel != Color(SK_ColorWHITE))
        ++pixels_different;
    }
  }

  // Look for extra bottom lhs pixels. They should be white.
  for (int y = height; y < size_.height(); ++y) {
    for (int x = 0; x < size_.width(); ++x) {
      uint32 lhs_pixel = pixel_at(x, y);
      if (lhs_pixel != Color(SK_ColorWHITE))
        ++pixels_different;
    }
  }

  // Look for extra bottom rhs pixels. They should be white.
  for (int y = height; y < rhs.size_.height(); ++y) {
    for (int x = 0; x < rhs.size_.width(); ++x) {
      uint32 rhs_pixel = rhs.pixel_at(x, y);
      if (rhs_pixel != Color(SK_ColorWHITE))
        ++pixels_different;
    }
  }

  // Like the WebKit ImageDiff tool, we define percentage different in terms
  // of the size of the 'actual' bitmap.
  double total_pixels = static_cast<double>(size_.width()) *
      static_cast<double>(height);
  return static_cast<double>(pixels_different) / total_pixels * 100.;
}

bool Image::LoadPng(const std::string& compressed) {
  int w;
  int h;
  bool success = gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(compressed.c_str()),
      compressed.size(), gfx::PNGCodec::FORMAT_BGRA, &data_, &w, &h);
  size_.SetSize(w, h);
  row_length_ = size_.width() * sizeof(uint32);
  return success;
}

bool Image::LoadMetafile(const std::string& data) {
  DCHECK(!data.empty());
#if defined(OS_WIN) || defined(OS_MACOSX)
  NativeMetafile metafile;
  metafile.CreateFromData(data.data(), data.size());
  return LoadMetafile(metafile);
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

bool Image::LoadMetafile(const NativeMetafile& metafile) {
#if defined(OS_WIN)
  gfx::Rect rect(metafile.GetBounds());
  DisableFontSmoothing disable_in_this_scope(true);
  // Create a temporary HDC and bitmap to retrieve the rendered data.
  HDC hdc = CreateCompatibleDC(NULL);
  BITMAPV4HEADER hdr;
  DCHECK_EQ(rect.x(), 0);
  DCHECK_EQ(rect.y(), 0);
  DCHECK_GE(rect.width(), 0);  // Metafile could be empty.
  DCHECK_GE(rect.height(), 0);
  if (rect.width() > 0 && rect.height() > 0) {
    size_ = rect.size();
    gfx::CreateBitmapV4Header(rect.width(), rect.height(), &hdr);
    void* bits;
    HBITMAP bitmap = CreateDIBSection(hdc,
                                      reinterpret_cast<BITMAPINFO*>(&hdr), 0,
                                      &bits, NULL, 0);
    DCHECK(bitmap);
    DCHECK(SelectObject(hdc, bitmap));
    skia::PlatformDevice::InitializeDC(hdc);
    bool success = metafile.Playback(hdc, NULL);
    row_length_ = size_.width() * sizeof(uint32);
    size_t bytes = row_length_ * size_.height();
    DCHECK(bytes);
    data_.resize(bytes);
    memcpy(&*data_.begin(), bits, bytes);
    DeleteDC(hdc);
    DeleteObject(bitmap);
    return success;
  }
#elif defined(OS_MACOSX)
  // The printing system uses single-page metafiles (page indexes are 1-based).
  const unsigned int page_number = 1;
  gfx::Rect rect(metafile.GetPageBounds(page_number));
  if (rect.width() > 0 && rect.height() > 0) {
    size_ = rect.size();
    row_length_ = size_.width() * sizeof(uint32);
    size_t bytes = row_length_ * size_.height();
    DCHECK(bytes);
    data_.resize(bytes);
    scoped_cftyperef<CGColorSpaceRef> color_space(
        CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
    scoped_cftyperef<CGContextRef> bitmap_context(
        CGBitmapContextCreate(&*data_.begin(), size_.width(), size_.height(),
                              8, row_length_, color_space,
                              kCGImageAlphaPremultipliedLast));
    DCHECK(bitmap_context.get());
    metafile.RenderPage(page_number, bitmap_context,
                        CGRectMake(0, 0, size_.width(), size_.height()));
  }
#else
  NOTIMPLEMENTED();
#endif

  return false;
}

}  // namespace printing
