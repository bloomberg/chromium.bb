// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/HTMLPictureElement.h"

#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLPictureElement::HTMLPictureElement(Document& document)
    : HTMLElement(pictureTag, document)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLPictureElement> HTMLPictureElement::create(Document& document)
{
    return adoptRef(new HTMLPictureElement(document));
}

} // namespace
