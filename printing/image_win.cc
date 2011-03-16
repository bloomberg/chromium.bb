// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include "skia/ext/platform_device.h"
#include "ui/gfx/gdi_util.h"  // EMF support
#include "ui/gfx/rect.h"

namespace {

// A simple class which temporarily overrides system settings.
// The bitmap image rendered via the PlayEnhMetaFile() function depends on
// some system settings.
// As a workaround for such dependency, this class saves the system settings
// and changes them. This class also restore the saved settings in its
// destructor.
class DisableFontSmoothing {
 public:
  explicit DisableFontSmoothing() : enable_again_(false) {
    BOOL enabled;
    if (SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &enabled, 0) &&
        enabled) {
      if (SystemParametersInfo(SPI_SETFONTSMOOTHING, FALSE, NULL, 0))
        enable_again_ = true;
    }
  }

  ~DisableFontSmoothing() {
    if (enable_again_) {
      BOOL result = SystemParametersInfo(SPI_SETFONTSMOOTHING, TRUE, NULL, 0);
      DCHECK(result);
    }
  }

 private:
  bool enable_again_;

  DISALLOW_COPY_AND_ASSIGN(DisableFontSmoothing);
};

}  // namespace

namespace printing {

bool Image::LoadMetafile(const NativeMetafile& metafile) {
    gfx::Rect rect(metafile.GetPageBounds(1));
  DisableFontSmoothing disable_in_this_scope;

  // Create a temporary HDC and bitmap to retrieve the rendered data.
  HDC hdc = CreateCompatibleDC(NULL);
  BITMAPV4HEADER hdr;
  DCHECK_EQ(rect.x(), 0);
  DCHECK_EQ(rect.y(), 0);
  DCHECK_GE(rect.width(), 0);  // Metafile could be empty.
  DCHECK_GE(rect.height(), 0);

  if (rect.width() < 1 || rect.height() < 1)
    return false;

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

}  // namespace printing
