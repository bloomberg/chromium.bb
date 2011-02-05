// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GDI_UTIL_H_
#define UI_GFX_GDI_UTIL_H_
#pragma once

#include <vector>
#include <windows.h>

#include "ui/gfx/rect.h"

namespace gfx {

// Creates a BITMAPINFOHEADER structure given the bitmap's size.
void CreateBitmapHeader(int width, int height, BITMAPINFOHEADER* hdr);

// Creates a BITMAPINFOHEADER structure given the bitmap's size and
// color depth in bits per pixel.
void CreateBitmapHeaderWithColorDepth(int width, int height, int color_depth,
                                      BITMAPINFOHEADER* hdr);

// Creates a BITMAPV4HEADER structure given the bitmap's size.  You probably
// only need to use BMP V4 if you need transparency (alpha channel). This
// function sets the AlphaMask to 0xff000000.
void CreateBitmapV4Header(int width, int height, BITMAPV4HEADER* hdr);

// Creates a monochrome bitmap header.
void CreateMonochromeBitmapHeader(int width, int height, BITMAPINFOHEADER* hdr);

// Modify the given hrgn by subtracting the given rectangles.
void SubtractRectanglesFromRegion(HRGN hrgn,
                                  const std::vector<gfx::Rect>& cutouts);

}  // namespace gfx

#endif  // UI_GFX_GDI_UTIL_H_
