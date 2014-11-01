/*
 * Copyright (C) 2014 Robert Hogan <robhogan@chromium.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/html/HTMLImageFallbackHelper.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/ElementRareData.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/fetch/ImageResource.h"
#include "core/html/FormDataList.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLStyleElement.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

static bool noImageSourceSpecified(const Element& element)
{
    bool noSrcSpecified = !element.hasAttribute(srcAttr) || element.getAttribute(srcAttr).isNull() || element.getAttribute(srcAttr).isEmpty();
    bool noSrcsetSpecified = !element.hasAttribute(srcsetAttr) || element.getAttribute(srcsetAttr).isNull() || element.getAttribute(srcsetAttr).isEmpty();
    return noSrcSpecified && noSrcsetSpecified;
}

void HTMLImageFallbackHelper::createAltTextShadowTree(Element& element)
{
    ShadowRoot& root = element.ensureUserAgentShadowRoot();
    RefPtr<HTMLStyleElement> style = HTMLStyleElement::create(element.document(), false);
    root.appendChild(style);
    style->setTextContent("#alttext-container { overflow: hidden; border: 1px solid silver; padding: 1px; display: inline-block; box-sizing: border-box; } "
        "#alttext { overflow: hidden; display: block; } "
        "#alttext-image { margin: 0px; }");

    RefPtr<HTMLDivElement> container = HTMLDivElement::create(element.document());
    root.appendChild(container);
    container->setAttribute(idAttr, AtomicString("alttext-container", AtomicString::ConstructFromLiteral));

    RefPtr<HTMLImageElement> brokenImage = HTMLImageElement::create(element.document());
    container->appendChild(brokenImage);
    brokenImage->setIsFallbackImage();
    brokenImage->setAttribute(idAttr, AtomicString("alttext-image", AtomicString::ConstructFromLiteral));
    brokenImage->setAttribute(widthAttr, AtomicString("16", AtomicString::ConstructFromLiteral));
    brokenImage->setAttribute(heightAttr, AtomicString("16", AtomicString::ConstructFromLiteral));
    brokenImage->setAttribute(alignAttr, AtomicString("left", AtomicString::ConstructFromLiteral));
    brokenImage->setAttribute(srcAttr, AtomicString("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAABO0lEQVR4XpWRTytEYRTG3/tHLGQr"
        "NfEBpKQkmc8gG0ulxpRsfQ0TTXbI3kZ2ys4tsyBZjN1N8idlYUYaxJzzOKf7ztG1MfN0ens69/x6"
        "n/eeYPuo7npS9bCOriXDMQOCTVbO3X+6Wp9mICZWAHqquE1qAA8EQRiFcmYMsQKsAHM2DeKV4qj4"
        "8ZHB++bHwcXTQ+Mz6u/rABwSIQO0wKW5wuLU8E5yWzlJS7OFvaUJMGelAFkkYici1NIXB4DQbH1J"
        "Y2ggVoD4N1Kb2AAA149vUmLLxbHaTaNynIJggAz7G7jTyjB5wOv79/LupT47Cu0rcT6S/ai755YU"
        "iMM4cgw4/I3ERAaI3y/PiFnYPHOBtfORLKX38AaBERaJbHFeAOY3Eu8ZOYByi7Pl+hzWN+nwWjVB"
        "15LhYHXr1PWiH0AYTw4BBjO9AAAAAElFTkSuQmCC", AtomicString::ConstructFromLiteral));

    RefPtr<HTMLDivElement> altText = HTMLDivElement::create(element.document());
    container->appendChild(altText);
    altText->setAttribute(idAttr, AtomicString("alttext", AtomicString::ConstructFromLiteral));

    RefPtr<Text> text = Text::create(element.document(), toHTMLElement(element).altText());
    altText->appendChild(text);
}

PassRefPtr<RenderStyle> HTMLImageFallbackHelper::customStyleForAltText(Element& element, PassRefPtr<RenderStyle> newStyle)
{
    // If we have an author shadow root or have not created the UA shadow root yet, bail early. We can't
    // use ensureUserAgentShadowRoot() here because that would alter the DOM tree during style recalc.
    if (element.shadowRoot() || !element.userAgentShadowRoot())
        return newStyle;

    Element* placeHolder = element.userAgentShadowRoot()->getElementById("alttext-container");
    Element* brokenImage = element.userAgentShadowRoot()->getElementById("alttext-image");

    if (element.document().inQuirksMode()) {
        // Mimic the behaviour of the image host by setting symmetric dimensions if only one dimension is specified.
        if (newStyle->width().isSpecifiedOrIntrinsic() && newStyle->height().isAuto())
            newStyle->setHeight(newStyle->width());
        else if (newStyle->height().isSpecifiedOrIntrinsic() && newStyle->width().isAuto())
            newStyle->setWidth(newStyle->height());
        if (newStyle->width().isSpecifiedOrIntrinsic() && newStyle->height().isSpecifiedOrIntrinsic()) {
            placeHolder->setInlineStyleProperty(CSSPropertyVerticalAlign, CSSValueBaseline);
        }
    }

    // If the image has specified dimensions allow the alt-text container expand to fill them.
    if (newStyle->width().isSpecifiedOrIntrinsic() && newStyle->height().isSpecifiedOrIntrinsic()) {
        placeHolder->setInlineStyleProperty(CSSPropertyWidth, 100, CSSPrimitiveValue::CSS_PERCENTAGE);
        placeHolder->setInlineStyleProperty(CSSPropertyHeight, 100, CSSPrimitiveValue::CSS_PERCENTAGE);
    }

    // Make sure the broken image icon appears on the appropriate side of the image for the element's writing direction.
    brokenImage->setInlineStyleProperty(CSSPropertyFloat, AtomicString(newStyle->direction() == LTR ? "left" : "right"));

    // This is an <img> with no attributes, so don't display anything.
    if (noImageSourceSpecified(element) && !newStyle->width().isSpecifiedOrIntrinsic() && !newStyle->height().isSpecifiedOrIntrinsic() && toHTMLElement(element).altText().isEmpty())
        newStyle->setDisplay(NONE);

    // This preserves legacy behaviour originally defined when alt-text was managed by RenderImage.
    if (noImageSourceSpecified(element))
        brokenImage->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
    else
        brokenImage->setInlineStyleProperty(CSSPropertyDisplay, CSSValueInline);

    return newStyle;
}

} // namespace blink

