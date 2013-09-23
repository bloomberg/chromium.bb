/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/html/shadow/PasswordGeneratorButtonElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "core/events/Event.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/fetch/ImageResource.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/shadow/HTMLShadowElement.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/platform/graphics/Image.h"
#include "core/rendering/RenderImage.h"

namespace WebCore {

using namespace HTMLNames;

// FIXME: This class is only used in Chromium and has no layout tests.

PasswordGeneratorButtonElement::PasswordGeneratorButtonElement(Document& document)
    : HTMLDivElement(HTMLNames::divTag, document)
    , m_isInHoverState(false)
{
    setHasCustomStyleCallbacks();
}

static void getDecorationRootAndDecoratedRoot(HTMLInputElement* input, ShadowRoot*& decorationRoot, ShadowRoot*& decoratedRoot)
{
    ShadowRoot* existingRoot = input->youngestShadowRoot();
    ShadowRoot* newRoot = 0;
    while (existingRoot->childNodeCount() == 1 && existingRoot->firstChild()->hasTagName(shadowTag)) {
        newRoot = existingRoot;
        existingRoot = existingRoot->olderShadowRoot();
        ASSERT(existingRoot);
    }
    if (newRoot) {
        newRoot->removeChild(newRoot->firstChild());
    } else {
        // FIXME: This interacts really badly with author shadow roots because now
        // we can interleave user agent and author shadow roots on the element meaning
        // input.shadowRoot may be inaccessible if the browser has decided to decorate
        // the input.
        newRoot = input->ensureShadow()->addShadowRoot(input, ShadowRoot::UserAgentShadowRoot);
    }
    decorationRoot = newRoot;
    decoratedRoot = existingRoot;
}

void PasswordGeneratorButtonElement::decorate(HTMLInputElement* input)
{
    ASSERT(input);
    ShadowRoot* existingRoot;
    ShadowRoot* decorationRoot;
    getDecorationRootAndDecoratedRoot(input, decorationRoot, existingRoot);
    ASSERT(decorationRoot);
    ASSERT(existingRoot);
    RefPtr<HTMLDivElement> box = HTMLDivElement::create(input->document());
    decorationRoot->appendChild(box);
    box->setInlineStyleProperty(CSSPropertyDisplay, CSSValueFlex);
    box->setInlineStyleProperty(CSSPropertyAlignItems, CSSValueCenter);
    ASSERT(existingRoot->childNodeCount() == 1);
    toHTMLElement(existingRoot->firstChild())->setInlineStyleProperty(CSSPropertyFlexGrow, 1.0, CSSPrimitiveValue::CSS_NUMBER);
    box->appendChild(HTMLShadowElement::create(HTMLNames::shadowTag, input->document()));
    setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
    box->appendChild(this);
}

inline HTMLInputElement* PasswordGeneratorButtonElement::hostInput()
{
    // PasswordGeneratorButtonElement is created only by C++ code, and it is always
    // in <input> shadow.
    return toHTMLInputElement(shadowHost());
}

void PasswordGeneratorButtonElement::updateImage()
{
    if (!renderer() || !renderer()->isImage())
        return;
    RenderImageResource* resource = toRenderImage(renderer())->imageResource();
    ImageResource* image = m_isInHoverState ? imageForHoverState() : imageForNormalState();
    ASSERT(image);
    resource->setImageResource(image);
}

PassRefPtr<RenderStyle> PasswordGeneratorButtonElement::customStyleForRenderer()
{
    RefPtr<RenderStyle> originalStyle = originalStyleForRenderer();
    RefPtr<RenderStyle> style = RenderStyle::clone(originalStyle.get());
    RenderStyle* inputStyle = hostInput()->renderStyle();
    ASSERT(inputStyle);
    style->setWidth(Length(inputStyle->fontSize(), Fixed));
    style->setHeight(Length(inputStyle->fontSize(), Fixed));
    updateImage();
    return style.release();
}

RenderObject* PasswordGeneratorButtonElement::createRenderer(RenderStyle*)
{
    RenderImage* image = new RenderImage(this);
    image->setImageResource(RenderImageResource::create());
    return image;
}

void PasswordGeneratorButtonElement::attach(const AttachContext& context)
{
    HTMLDivElement::attach(context);
    updateImage();
}

ImageResource* PasswordGeneratorButtonElement::imageForNormalState()
{
    if (!m_cachedImageForNormalState) {
        RefPtr<Image> image = Image::loadPlatformResource("generatePassword");
        m_cachedImageForNormalState = new ImageResource(image.get());
    }
    return m_cachedImageForNormalState.get();
}

ImageResource* PasswordGeneratorButtonElement::imageForHoverState()
{
    if (!m_cachedImageForHoverState) {
        RefPtr<Image> image = Image::loadPlatformResource("generatePasswordHover");
        m_cachedImageForHoverState = new ImageResource(image.get());
    }
    return m_cachedImageForHoverState.get();
}

void PasswordGeneratorButtonElement::defaultEventHandler(Event* event)
{
    RefPtr<HTMLInputElement> input = hostInput();
    if (!input || input->isDisabledOrReadOnly() || !event->isMouseEvent()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    RefPtr<PasswordGeneratorButtonElement> protector(this);
    if (event->type() == eventNames().clickEvent) {
        if (Page* page = document().page())
            page->chrome().client().openPasswordGenerator(input.get());
        event->setDefaultHandled();
    }

    if (event->type() == eventNames().mouseoverEvent) {
        m_isInHoverState = true;
        updateImage();
    }

    if (event->type() == eventNames().mouseoutEvent) {
        m_isInHoverState = false;
        updateImage();
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

bool PasswordGeneratorButtonElement::willRespondToMouseMoveEvents()
{
    const HTMLInputElement* input = hostInput();
    if (!input->isDisabledOrReadOnly())
        return true;

    return HTMLDivElement::willRespondToMouseMoveEvents();
}

bool PasswordGeneratorButtonElement::willRespondToMouseClickEvents()
{
    const HTMLInputElement* input = hostInput();
    if (!input->isDisabledOrReadOnly())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}

} // namespace WebCore
