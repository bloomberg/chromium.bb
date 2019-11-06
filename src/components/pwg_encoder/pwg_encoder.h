// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PWG_ENCODER_PWG_ENCODER_H_
#define COMPONENTS_PWG_ENCODER_PWG_ENCODER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"

namespace pwg_encoder {

class BitmapImage;

struct PwgHeaderInfo {
  PwgHeaderInfo()
      : dpi(300, 300),
        total_pages(1),
        flipx(false),
        flipy(false),
        color_space(SRGB),
        duplex(false),
        tumble(false) {}
  enum ColorSpace { SGRAY = 18, SRGB = 19 };
  gfx::Size dpi;
  uint32_t total_pages;
  bool flipx;
  bool flipy;
  ColorSpace color_space;
  bool duplex;
  bool tumble;
};

class PwgEncoder {
 public:
  static std::string GetDocumentHeader();

  // Given an image, create a PWG of the image and put the compressed image data
  // in the returned string, or return an empty string on failure.
  static std::string EncodePage(const BitmapImage& image,
                                const PwgHeaderInfo& pwg_header_info);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PwgEncoder);
};

}  // namespace pwg_encoder

#endif  // COMPONENTS_PWG_ENCODER_PWG_ENCODER_H_
