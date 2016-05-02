// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AppearanceSwitchElement_h
#define AppearanceSwitchElement_h

#include "core/html/HTMLDivElement.h"

namespace blink {

class AppearanceSwitchElement final : public HTMLDivElement {
public:
    enum Mode {
        ShowIfHostHasAppearance,
        ShowIfHostHasNoAppearance,
    };
    static AppearanceSwitchElement* create(Document&, Mode);

private:
    AppearanceSwitchElement(Document&, Mode);
    PassRefPtr<ComputedStyle> customStyleForLayoutObject() override;

    const Mode m_mode;
};

} // namespace blink
#endif // AppearanceSwitchElement_h
