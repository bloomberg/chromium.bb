// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementSandbox.h"

namespace blink {

HTMLIFrameElementSandbox::HTMLIFrameElementSandbox(DOMSettableTokenListObserver* observer)
    : DOMSettableTokenList(observer)
{
}

HTMLIFrameElementSandbox::~HTMLIFrameElementSandbox()
{
}

using SandboxSupportedTokens = HashSet<AtomicString>;

static SandboxSupportedTokens& supportedTokens()
{
    DEFINE_STATIC_LOCAL(SandboxSupportedTokens, supportedValues, ());
    if (supportedValues.isEmpty()) {
        supportedValues.add("allow-forms");
        supportedValues.add("allow-modals");
        supportedValues.add("allow-pointer-lock");
        supportedValues.add("allow-popups");
        supportedValues.add("allow-popups-to-escape-sandbox");
        supportedValues.add("allow-same-origin");
        supportedValues.add("allow-scripts");
        supportedValues.add("allow-top-navigation");
    }

    return supportedValues;
}

bool HTMLIFrameElementSandbox::validateTokenValue(const AtomicString& tokenValue, ExceptionState&) const
{
    return supportedTokens().contains(tokenValue);
}

}
