// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "base/logging.h"
#include "base/stringprintf.h"
#include "ui/gfx/size_base.h"
#include "ui/gfx/size_base_impl.h"

namespace gfx {

template class SizeBase<Size, int>;

Size::Size() : SizeBase<Size, int>(0, 0) {}

Size::Size(int width, int height) : SizeBase<Size, int>(0, 0) {
  set_width(width);
  set_height(height);
}

#if defined(OS_MACOSX)
Size::Size(const CGSize& s) : SizeBase<Size, int>(0, 0) {
  set_width(s.width);
  set_height(s.height);
}

Size& Size::operator=(const CGSize& s) {
  set_width(s.width);
  set_height(s.height);
  return *this;
}
#endif

Size::~Size() {}

#if defined(OS_WIN)
SIZE Size::ToSIZE() const {
  SIZE s;
  s.cx = width();
  s.cy = height();
  return s;
}
#elif defined(OS_MACOSX)
CGSize Size::ToCGSize() const {
  return CGSizeMake(width(), height());
}
#endif

std::string Size::ToString() const {
  return base::StringPrintf("%dx%d", width(), height());
}

}  // namespace gfx
