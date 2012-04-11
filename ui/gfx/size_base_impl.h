// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/size_base.h"

#include "base/logging.h"
#include "base/stringprintf.h"

// This file provides the implementation for SizeBaese template and
// used to instantiate the base class for Size and SizeF classes.
#if !defined(UI_IMPLEMENTATION)
#error "This file is intended for UI implementation only"
#endif

namespace gfx {

template<typename Class, typename Type>
SizeBase<Class, Type>::SizeBase(Type width, Type height) {
  set_width(width);
  set_height(height);
}

template<typename Class, typename Type>
SizeBase<Class, Type>::~SizeBase() {}

template<typename Class, typename Type>
void SizeBase<Class, Type>::set_width(Type width) {
  if (width < 0) {
    NOTREACHED() << "negative width:" << width;
    width = 0;
  }
  width_ = width;
}

template<typename Class, typename Type>
void SizeBase<Class, Type>::set_height(Type height) {
  if (height < 0) {
    NOTREACHED() << "negative height:" << height;
    height = 0;
  }
  height_ = height;
}

}  // namespace gfx
