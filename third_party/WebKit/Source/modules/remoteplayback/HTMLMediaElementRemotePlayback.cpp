// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/HTMLMediaElementRemotePlayback.h"

#include "core/dom/QualifiedName.h"
#include "core/html/HTMLMediaElement.h"

namespace blink {

// static
bool HTMLMediaElementRemotePlayback::fastHasAttribute(const QualifiedName& name, const HTMLMediaElement& element)
{
    ASSERT(name == HTMLNames::disableremoteplaybackAttr);
    return element.fastHasAttribute(name);
}

// static
void HTMLMediaElementRemotePlayback::setBooleanAttribute(const QualifiedName& name, HTMLMediaElement& element, bool value)
{
    ASSERT(name == HTMLNames::disableremoteplaybackAttr);
    element.setBooleanAttribute(name, value);
}

} // namespace blink
