// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutThemeLinux.h"

#include "platform/DataResourceHelper.h"

namespace blink {

RefPtr<LayoutTheme> LayoutThemeLinux::Create() {
  return AdoptRef(new LayoutThemeLinux());
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeLinux::Create()));
  return *layout_theme;
}

String LayoutThemeLinux::ExtraDefaultStyleSheet() {
  return LayoutThemeDefault::ExtraDefaultStyleSheet() +
         GetDataResourceAsASCIIString("themeChromiumLinux.css");
}

}  // namespace blink
