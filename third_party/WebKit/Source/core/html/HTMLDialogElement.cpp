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

#include "core/css/StyleResolver.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/FrameView.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/style/RenderStyle.h"

namespace WebCore {

using namespace HTMLNames;

HTMLDialogElement::HTMLDialogElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
    , m_topIsValid(false)
    , m_top(0)
{
    ASSERT(hasTagName(dialogTag));
    setHasCustomStyleCallbacks();
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLDialogElement> HTMLDialogElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLDialogElement(tagName, document));
}

void HTMLDialogElement::close(ExceptionCode& ec)
{
    if (!fastHasAttribute(openAttr)) {
        ec = INVALID_STATE_ERR;
        return;
    }
    setBooleanAttribute(openAttr, false);
    document()->removeFromTopLayer(this);
    m_topIsValid = false;
}

static bool needsCenteredPositioning(const RenderStyle* style)
{
    return style->position() == AbsolutePosition && style->hasAutoTopAndBottom();
}

PassRefPtr<RenderStyle> HTMLDialogElement::customStyleForRenderer()
{
    RefPtr<RenderStyle> originalStyle = document()->styleResolver()->styleForElement(this);
    RefPtr<RenderStyle> style = RenderStyle::clone(originalStyle.get());

    // Override top to remain centered after style recalcs.
    if (needsCenteredPositioning(style.get()) && m_topIsValid)
        style->setTop(Length(m_top.toInt(), WebCore::Fixed));

    return style.release();
}

void HTMLDialogElement::positionAndReattach()
{
    // Layout because we need to know our ancestors' positions and our own height.
    document()->updateLayoutIgnorePendingStylesheets();

    RenderBox* box = renderBox();
    if (!box || !needsCenteredPositioning(box->style()))
        return;

    // Set up dialog's position to be safe-centered in the viewport.
    // FIXME: Figure out what to do in vertical writing mode.
    FrameView* frameView = document()->view();
    int scrollTop = frameView->scrollOffset().height();
    FloatPoint absolutePoint(0, scrollTop);
    int visibleHeight = frameView->visibleContentRect(ScrollableArea::IncludeScrollbars).height();
    if (box->height() < visibleHeight)
        absolutePoint.move(0, (visibleHeight - box->height()) / 2);
    FloatPoint localPoint = box->containingBlock()->absoluteToLocal(absolutePoint);

    m_top = localPoint.y();
    m_topIsValid = true;

    // FIXME: It's inefficient to reattach here. We could do better by mutating style directly and forcing another layout.
    reattach();
}

void HTMLDialogElement::show()
{
    if (fastHasAttribute(openAttr))
        return;
    setBooleanAttribute(openAttr, true);
    positionAndReattach();
}

void HTMLDialogElement::showModal(ExceptionCode& ec)
{
    if (fastHasAttribute(openAttr) || !inDocument()) {
        ec = INVALID_STATE_ERR;
        return;
    }
    document()->addToTopLayer(this);
    setBooleanAttribute(openAttr, true);
    positionAndReattach();
}

bool HTMLDialogElement::isPresentationAttribute(const QualifiedName& name) const
{
    // FIXME: Workaround for <https://bugs.webkit.org/show_bug.cgi?id=91058>: modifying an attribute for which there is an attribute selector
    // in html.css sometimes does not trigger a style recalc.
    if (name == openAttr)
        return true;

    return HTMLElement::isPresentationAttribute(name);
}

} // namespace WebCore
