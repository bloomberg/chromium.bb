// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutThemeAndroid.h"

namespace blink {

scoped_refptr<LayoutTheme> LayoutThemeAndroid::Create() {
  return WTF::AdoptRef(new LayoutThemeAndroid());
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeAndroid::Create()));
  return *layout_theme;
}

LayoutThemeAndroid::~LayoutThemeAndroid() {}

}  // namespace blink
