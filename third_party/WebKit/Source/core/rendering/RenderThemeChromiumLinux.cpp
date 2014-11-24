// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderThemeChromiumLinux.h"

#include "core/UserAgentStyleSheets.h"

namespace blink {

PassRefPtr<RenderTheme> RenderThemeChromiumLinux::create()
{
    return adoptRef(new RenderThemeChromiumLinux());
}

RenderTheme& RenderTheme::theme()
{
    DEFINE_STATIC_REF(RenderTheme, renderTheme, (RenderThemeChromiumLinux::create()));
    return *renderTheme;
}

String RenderThemeChromiumLinux::extraDefaultStyleSheet()
{
    return RenderThemeChromiumDefault::extraDefaultStyleSheet() +
        String(themeChromiumLinuxCss, sizeof(themeChromiumLinuxCss));
}

} // namespace blink
