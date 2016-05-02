// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/AppearanceSwitchElement.h"

#include "core/dom/NodeComputedStyle.h"

namespace blink {

inline AppearanceSwitchElement::AppearanceSwitchElement(Document& doc, Mode mode)
    : HTMLDivElement(doc)
    , m_mode(mode)
{
    setHasCustomStyleCallbacks();
}

AppearanceSwitchElement* AppearanceSwitchElement::create(Document& doc, Mode mode)
{
    return new AppearanceSwitchElement(doc, mode);
}

PassRefPtr<ComputedStyle> AppearanceSwitchElement::customStyleForLayoutObject()
{
    // We can't use setInlineStyleProperty() because it updates the DOM tree.
    // We shouldn't do it during style calculation.
    // TODO(tkent): Injecting a CSS variable by host is a better approach?
    Element* host = shadowHost();
    RefPtr<ComputedStyle> style = originalStyleForLayoutObject();
    if (!host || !host->computedStyle())
        return style.release();
    if (m_mode == ShowIfHostHasAppearance) {
        if (host->computedStyle()->hasAppearance())
            return style.release();
    } else {
        if (!host->computedStyle()->hasAppearance())
            return style.release();
    }
    if (style->display() == NONE)
        return style.release();
    RefPtr<ComputedStyle> newStyle = ComputedStyle::clone(*style);
    newStyle->setDisplay(NONE);
    newStyle->setUnique();
    return newStyle.release();
}

} // namespace blink
