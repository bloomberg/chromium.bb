// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/LayoutThemeChromiumWin.h"

namespace blink {

PassRefPtr<LayoutTheme> LayoutThemeChromiumWin::create()
{
    return adoptRef(new LayoutThemeChromiumWin());
}

LayoutTheme& LayoutTheme::theme()
{
    DEFINE_STATIC_REF(LayoutTheme, layoutTheme, (LayoutThemeChromiumWin::create()));
    return *layoutTheme;
}

} // namespace blink
