// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RenderThemeChromiumWin_h
#define RenderThemeChromiumWin_h

#include "core/rendering/RenderThemeChromiumDefault.h"

namespace blink {

class RenderThemeChromiumWin final : public RenderThemeChromiumDefault {
public:
    static PassRefPtr<RenderTheme> create();
};

} // namespace blink

#endif // RenderThemeChromiumWin_h
