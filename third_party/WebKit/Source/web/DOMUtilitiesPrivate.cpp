/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMUtilitiesPrivate.h"

#include "HTMLNames.h"
#include "core/dom/Element.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTableElement.h"

using namespace WebCore;
using namespace WebCore::HTMLNames;

namespace WebKit {

bool elementHasLegalLinkAttribute(const Element* element, const QualifiedName& attrName)
{
    if (attrName == srcAttr)
        return element->hasTagName(imgTag) || element->hasTagName(scriptTag) || element->hasTagName(iframeTag) || element->hasTagName(frameTag) || (element->hasTagName(inputTag) && toHTMLInputElement(element)->isImageButton());
    if (attrName == hrefAttr)
        return element->hasTagName(linkTag) || isHTMLAnchorElement(element) || isHTMLAreaElement(element);
    if (attrName == actionAttr)
        return element->hasTagName(formTag);
    if (attrName == backgroundAttr)
        return element->hasTagName(bodyTag) || isHTMLTableElement(element) || element->hasTagName(trTag) || element->hasTagName(tdTag);
    if (attrName == citeAttr)
        return element->hasTagName(blockquoteTag) || element->hasTagName(qTag) || element->hasTagName(delTag) || element->hasTagName(insTag);
    if (attrName == classidAttr || attrName == dataAttr)
        return element->hasTagName(objectTag);
    if (attrName == codebaseAttr)
        return element->hasTagName(objectTag) || element->hasTagName(appletTag);
    return false;
}

} // namespace WebKit
