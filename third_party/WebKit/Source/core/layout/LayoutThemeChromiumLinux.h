// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutThemeChromiumLinux_h
#define LayoutThemeChromiumLinux_h

#include "core/layout/LayoutThemeChromiumDefault.h"

namespace blink {

class LayoutThemeChromiumLinux final : public LayoutThemeChromiumDefault {
public:
    static PassRefPtr<LayoutTheme> create();
    virtual String extraDefaultStyleSheet() override;
};

} // namespace blink

#endif // LayoutThemeChromiumLinux_h
