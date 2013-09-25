/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/HTMLDialogElement.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/FrameView.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/style/RenderStyle.h"

namespace WebCore {

using namespace HTMLNames;

static bool needsCenteredPositioning(const RenderStyle* style)
{
    return style->position() == AbsolutePosition && style->hasAutoTopAndBottom();
}

HTMLDialogElement::HTMLDialogElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_topIsValid(false)
    , m_top(0)
    , m_returnValue("")
{
    ASSERT(hasTagName(dialogTag));
    setHasCustomStyleCallbacks();
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLDialogElement> HTMLDialogElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLDialogElement(tagName, document));
}

void HTMLDialogElement::close(const String& returnValue, ExceptionState& es)
{
    if (!fastHasAttribute(openAttr)) {
        es.throwUninformativeAndGenericDOMException(InvalidStateError);
        return;
    }
    closeDialog(returnValue);
}

void HTMLDialogElement::closeDialog(const String& returnValue)
{
    setBooleanAttribute(openAttr, false);
    document().removeFromTopLayer(this);
    m_topIsValid = false;

    if (!returnValue.isNull())
        m_returnValue = returnValue;

    dispatchEvent(Event::create(eventNames().closeEvent));
}

PassRefPtr<RenderStyle> HTMLDialogElement::customStyleForRenderer()
{
    RefPtr<RenderStyle> originalStyle = originalStyleForRenderer();
    RefPtr<RenderStyle> style = RenderStyle::clone(originalStyle.get());

    // Override top to remain centered after style recalcs.
    if (needsCenteredPositioning(style.get()) && m_topIsValid)
        style->setTop(Length(m_top.toInt(), WebCore::Fixed));

    return style.release();
}

void HTMLDialogElement::reposition()
{
    // Layout because we need to know our ancestors' positions and our own height.
    document().updateLayoutIgnorePendingStylesheets();

    RenderBox* box = renderBox();
    if (!box || !needsCenteredPositioning(box->style()))
        return;

    // Set up dialog's position to be safe-centered in the viewport.
    // FIXME: Figure out what to do in vertical writing mode.
    FrameView* frameView = document().view();
    int scrollTop = frameView->scrollOffset().height();
    int visibleHeight = frameView->visibleContentRect(ScrollableArea::IncludeScrollbars).height();
    m_top = scrollTop;
    if (box->height() < visibleHeight)
        m_top += (visibleHeight - box->height()) / 2;
    m_topIsValid = true;

    setNeedsStyleRecalc(LocalStyleChange);
}

void HTMLDialogElement::show()
{
    if (fastHasAttribute(openAttr))
        return;
    setBooleanAttribute(openAttr, true);
    reposition();
}

void HTMLDialogElement::showModal(ExceptionState& es)
{
    if (fastHasAttribute(openAttr) || !inDocument()) {
        es.throwUninformativeAndGenericDOMException(InvalidStateError);
        return;
    }
    document().addToTopLayer(this);
    setBooleanAttribute(openAttr, true);
    reposition();
}

bool HTMLDialogElement::isPresentationAttribute(const QualifiedName& name) const
{
    // FIXME: Workaround for <https://bugs.webkit.org/show_bug.cgi?id=91058>: modifying an attribute for which there is an attribute selector
    // in html.css sometimes does not trigger a style recalc.
    if (name == openAttr)
        return true;

    return HTMLElement::isPresentationAttribute(name);
}

void HTMLDialogElement::defaultEventHandler(Event* event)
{
    if (event->type() == eventNames().cancelEvent) {
        closeDialog();
        event->setDefaultHandled();
        return;
    }
    HTMLElement::defaultEventHandler(event);
}

bool HTMLDialogElement::shouldBeReparentedUnderRenderView(const RenderStyle* style) const
{
    if (style && style->position() == AbsolutePosition)
        return true;
    return Element::shouldBeReparentedUnderRenderView(style);
}

} // namespace WebCore
