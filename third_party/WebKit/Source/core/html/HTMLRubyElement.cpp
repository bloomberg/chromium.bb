// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/HTMLRubyElement.h"

#include "HTMLNames.h"
#include "core/rendering/RenderRuby.h"

namespace WebCore {

using namespace HTMLNames;

HTMLRubyElement::HTMLRubyElement(Document& document)
    : HTMLElement(rubyTag, document)
{
}

PassRefPtrWillBeRawPtr<HTMLRubyElement> HTMLRubyElement::create(Document& document)
{
    return adoptRefWillBeRefCountedGarbageCollected(new HTMLRubyElement(document));
}

RenderObject* HTMLRubyElement::createRenderer(RenderStyle* style)
{
    if (style->display() == INLINE)
        return new RenderRubyAsInline(this);
    if (style->display() == BLOCK)
        return new RenderRubyAsBlock(this);
    return RenderObject::createObject(this, style);
}

}
