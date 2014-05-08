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

#include "core/dom/NodeRenderStyle.h"
#include "core/events/Event.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/FrameHost.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/rendering/RenderImage.h"
#include "platform/graphics/Image.h"

namespace WebCore {

using namespace HTMLNames;

PasswordGeneratorButtonElement::PasswordGeneratorButtonElement(Document& document)
    : HTMLDivElement(document)
    , m_isInHoverState(false)
{
    setHasCustomStyleCallbacks();
}

PassRefPtrWillBeRawPtr<PasswordGeneratorButtonElement> PasswordGeneratorButtonElement::create(Document& document)
{
    RefPtrWillBeRawPtr<PasswordGeneratorButtonElement> element = adoptRefWillBeRefCountedGarbageCollected(new PasswordGeneratorButtonElement(document));
    element->setAttribute(idAttr, ShadowElementNames::passwordGenerator());
    return element.release();
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
    style->setUnique();
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
    ASSERT(document().isActive());
    RefPtr<HTMLInputElement> input = hostInput();
    if (!input || input->isDisabledOrReadOnly() || !event->isMouseEvent()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    RefPtr<PasswordGeneratorButtonElement> protector(this);
    if (event->type() == EventTypeNames::click) {
        document().frameHost()->chrome().client().openPasswordGenerator(input.get());
        event->setDefaultHandled();
    }

    if (event->type() == EventTypeNames::mouseover) {
        m_isInHoverState = true;
        updateImage();
    }

    if (event->type() == EventTypeNames::mouseout) {
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
