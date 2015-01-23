// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/LayoutThemeChromiumLinux.h"

#include "platform/PlatformResourceLoader.h"

namespace blink {

PassRefPtr<LayoutTheme> LayoutThemeChromiumLinux::create()
{
    return adoptRef(new LayoutThemeChromiumLinux());
}

LayoutTheme& LayoutTheme::theme()
{
    DEFINE_STATIC_REF(LayoutTheme, layoutTheme, (LayoutThemeChromiumLinux::create()));
    return *layoutTheme;
}

String LayoutThemeChromiumLinux::extraDefaultStyleSheet()
{
    return LayoutThemeChromiumDefault::extraDefaultStyleSheet() +
        loadResourceAsASCIIString("themeChromiumLinux.css");
}

} // namespace blink
