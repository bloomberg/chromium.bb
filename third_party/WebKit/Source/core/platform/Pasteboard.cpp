/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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
#include "core/platform/Pasteboard.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include "core/dom/Element.h"
#include "core/fetch/ImageResource.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/platform/chromium/ChromiumDataObject.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/skia/NativeImageSkia.h"
#include "core/rendering/RenderImage.h"
#include "platform/clipboard/ClipboardUtilities.h"
#include "public/platform/Platform.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "weborigin/KURL.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard;
    return pasteboard;
}

Pasteboard::Pasteboard()
    : m_buffer(blink::WebClipboard::BufferStandard)
{
}

bool Pasteboard::isSelectionMode() const
{
    return m_buffer == blink::WebClipboard::BufferSelection;
}

void Pasteboard::setSelectionMode(bool selectionMode)
{
    m_buffer = selectionMode ? blink::WebClipboard::BufferSelection : blink::WebClipboard::BufferStandard;
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption)
{
    // FIXME: add support for smart replace
#if OS(WIN)
    String plainText(text);
    replaceNewlinesWithWindowsStyleNewlines(plainText);
    blink::Platform::current()->clipboard()->writePlainText(plainText);
#else
    blink::Platform::current()->clipboard()->writePlainText(text);
#endif
}

void Pasteboard::writeImage(Node* node, const KURL&, const String& title)
{
    ASSERT(node);

    if (!(node->renderer() && node->renderer()->isImage()))
        return;

    RenderImage* renderer = toRenderImage(node->renderer());
    ImageResource* cachedImage = renderer->cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return;
    Image* image = cachedImage->imageForRenderer(renderer);
    ASSERT(image);

    RefPtr<NativeImageSkia> bitmap = image->nativeImageForCurrentFrame();
    if (!bitmap)
        return;

    // If the image is wrapped in a link, |url| points to the target of the
    // link. This isn't useful to us, so get the actual image URL.
    AtomicString urlString;
    if (node->hasTagName(HTMLNames::imgTag) || node->hasTagName(HTMLNames::inputTag))
        urlString = toElement(node)->getAttribute(HTMLNames::srcAttr);
    else if (node->hasTagName(SVGNames::imageTag))
        urlString = toElement(node)->getAttribute(XLinkNames::hrefAttr);
    else if (node->hasTagName(HTMLNames::embedTag) || node->hasTagName(HTMLNames::objectTag))
        urlString = toElement(node)->imageSourceURL();
    KURL url = urlString.isEmpty() ? KURL() : node->document().completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
    blink::WebImage webImage = bitmap->bitmap();
    blink::Platform::current()->clipboard()->writeImage(webImage, blink::WebURL(url), blink::WebString(title));
}

void Pasteboard::writeDataObject(PassRefPtr<ChromiumDataObject> dataObject)
{
    blink::Platform::current()->clipboard()->writeDataObject(dataObject);
}

bool Pasteboard::canSmartReplace()
{
    return blink::Platform::current()->clipboard()->isFormatAvailable(blink::WebClipboard::FormatSmartPaste, m_buffer);
}

bool Pasteboard::isHTMLAvailable()
{
    return blink::Platform::current()->clipboard()->isFormatAvailable(blink::WebClipboard::FormatHTML, m_buffer);
}

String Pasteboard::plainText()
{
    return blink::Platform::current()->clipboard()->readPlainText(m_buffer);
}

String Pasteboard::readHTML(KURL& url, unsigned& fragmentStart, unsigned& fragmentEnd)
{
    blink::WebURL webURL;
    blink::WebString markup = blink::Platform::current()->clipboard()->readHTML(m_buffer, &webURL, &fragmentStart, &fragmentEnd);
    if (!markup.isEmpty()) {
        url = webURL;
        // fragmentStart and fragmentEnd are populated by WebClipboard::readHTML.
    } else {
        url = KURL();
        fragmentStart = 0;
        fragmentEnd = 0;
    }
    return markup;
}

void Pasteboard::writeHTML(const String& markup, const KURL& documentURL, const String& plainText, bool canSmartCopyOrDelete)
{
    String text = plainText;
#if OS(WIN)
    replaceNewlinesWithWindowsStyleNewlines(text);
#endif
    replaceNBSPWithSpace(text);

    blink::Platform::current()->clipboard()->writeHTML(markup, documentURL, text, canSmartCopyOrDelete);
}

} // namespace WebCore
