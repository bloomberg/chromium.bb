// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#include <CoreGraphics/CGGeometry.h>
#endif

#include "base/logging.h"
#include "base/stringprintf.h"

namespace gfx {

Size::Size() : width_(0), height_(0) {}

Size::Size(int width, int height) {
  set_width(width);
  set_height(height);
}

#if defined(OS_MACOSX)
Size::Size(const CGSize& s) {
  set_width(s.width);
  set_height(s.height);
}

Size& Size::operator=(const CGSize& s) {
  set_width(s.width);
  set_height(s.height);
  return *this;
}
#endif

#if defined(OS_WIN)
SIZE Size::ToSIZE() const {
  SIZE s;
  s.cx = width_;
  s.cy = height_;
  return s;
}
#elif defined(OS_MACOSX)
CGSize Size::ToCGSize() const {
  return CGSizeMake(width_, height_);
}
#endif

void Size::set_width(int width) {
  if (width < 0) {
    NOTREACHED() << "negative width:" << width;
    width = 0;
  }
  width_ = width;
}

void Size::set_height(int height) {
  if (height < 0) {
    NOTREACHED() << "negative height:" << height;
    height = 0;
  }
  height_ = height;
}

std::string Size::ToString() const {
  return base::StringPrintf("%dx%d", width_, height_);
}

}  // namespace gfx
