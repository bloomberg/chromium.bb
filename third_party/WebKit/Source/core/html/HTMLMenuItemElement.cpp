// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/HTMLMenuItemElement.h"

#include "core/HTMLNames.h"

namespace blink {

inline HTMLMenuItemElement::HTMLMenuItemElement(Document& document)
    : HTMLElement(HTMLNames::menuitemTag, document)
{
}

DEFINE_NODE_FACTORY(HTMLMenuItemElement)

} // namespace blink
