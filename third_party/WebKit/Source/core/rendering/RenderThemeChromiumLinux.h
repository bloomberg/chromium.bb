// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RenderThemeChromiumLinux_h
#define RenderThemeChromiumLinux_h

#include "core/rendering/RenderThemeChromiumDefault.h"

namespace blink {

class RenderThemeChromiumLinux final : public RenderThemeChromiumDefault {
public:
    static PassRefPtr<RenderTheme> create();
    virtual String extraDefaultStyleSheet() override;
};

} // namespace blink

#endif // RenderThemeChromiumLinux_h
