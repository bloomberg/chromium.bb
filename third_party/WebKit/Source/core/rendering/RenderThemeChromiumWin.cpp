// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderThemeChromiumWin.h"

namespace blink {

PassRefPtr<RenderTheme> RenderThemeChromiumWin::create()
{
    return adoptRef(new RenderThemeChromiumWin());
}

RenderTheme& RenderTheme::theme()
{
    DEFINE_STATIC_REF(RenderTheme, renderTheme, (RenderThemeChromiumWin::create()));
    return *renderTheme;
}

} // namespace blink
