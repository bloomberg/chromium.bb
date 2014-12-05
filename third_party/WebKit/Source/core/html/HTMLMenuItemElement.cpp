// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/HTMLMenuItemElement.h"

#include "core/HTMLNames.h"
#include "core/events/Event.h"

namespace blink {

using namespace HTMLNames;

inline HTMLMenuItemElement::HTMLMenuItemElement(Document& document)
    : HTMLElement(HTMLNames::menuitemTag, document)
{
}

void HTMLMenuItemElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::click) {
        if (equalIgnoringCase(fastGetAttribute(typeAttr), "checkbox")) {
            if (fastHasAttribute(checkedAttr))
                removeAttribute(checkedAttr);
            else
                setAttribute(checkedAttr, "checked");
        }
        event->setDefaultHandled();
    }
}

DEFINE_NODE_FACTORY(HTMLMenuItemElement)

} // namespace blink
