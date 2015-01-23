// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutThemeChromiumWin_h
#define LayoutThemeChromiumWin_h

#include "core/layout/LayoutThemeChromiumDefault.h"

namespace blink {

class LayoutThemeChromiumWin final : public LayoutThemeChromiumDefault {
public:
    static PassRefPtr<LayoutTheme> create();
};

} // namespace blink

#endif // LayoutThemeChromiumWin_h
