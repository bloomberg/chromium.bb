// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include "base/file_util.h"
#include "base/gfx/png_decoder.h"
#include "base/gfx/png_encoder.h"
#include "base/gfx/rect.h"
#include "base/string_util.h"
#include "printing/native_metafile.h"
#include "skia/ext/platform_device.h"

#if defined(OS_WIN)
#include "base/gfx/gdi_util.h"  // EMF support
#endif

printing::Image::Image(const std::wstring& filename) : ignore_alpha_(true) {
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

bool printing::Image::SaveToPng(const std::wstring& filename) {
  DCHECK(!data_.empty());
  std::vector<unsigned char> compressed;
  bool success = PNGEncoder::Encode(&*data_.begin(),
                                    PNGEncoder::FORMAT_BGRA,
                                    size_.width(),
                                    size_.height(),
                                    row_length_,
                                    true,
                                    &compressed);
  DCHECK(success && compressed.size());
  if (success) {
    int write_bytes = file_util::WriteFile(
        filename,
        reinterpret_cast<char*>(&*compressed.begin()), compressed.size());
    success = (write_bytes == static_cast<int>(compressed.size()));
    DCHECK(success);
  }
  return success;
}

double printing::Image::PercentageDifferent(const Image& rhs) const {
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

bool printing::Image::LoadPng(const std::string& compressed) {
  int w;
  int h;
  bool success = PNGDecoder::Decode(
      reinterpret_cast<const unsigned char*>(compressed.c_str()),
      compressed.size(), PNGDecoder::FORMAT_BGRA, &data_, &w, &h);
  size_.SetSize(w, h);
  row_length_ = size_.width() * sizeof(uint32);
  return success;
}

bool printing::Image::LoadMetafile(const std::string& data) {
  DCHECK(!data.empty());
#if defined(OS_WIN)
  printing::NativeMetafile metafile;
  metafile.CreateFromData(data.data(), data.size());
  gfx::Rect rect(metafile.GetBounds());
  // Create a temporary HDC and bitmap to retrieve the rendered data.
  HDC hdc = CreateCompatibleDC(NULL);
  BITMAPV4HEADER hdr;
  DCHECK_EQ(rect.x(), 0);
  DCHECK_EQ(rect.y(), 0);
  DCHECK_GT(rect.width(), 0);
  DCHECK_GT(rect.height(), 0);
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
#else
  NOTIMPLEMENTED();
  return false;
#endif
}
