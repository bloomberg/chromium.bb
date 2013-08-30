/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/html/ime/InputMethodContext.h"

#include "core/editing/InputMethodController.h"
#include "core/html/ime/Composition.h"
#include "core/page/Frame.h"

namespace WebCore {

PassOwnPtr<InputMethodContext> InputMethodContext::create(HTMLElement* element)
{
    return adoptPtr(new InputMethodContext(element));
}

InputMethodContext::InputMethodContext(HTMLElement* element)
    : m_composition(0)
    , m_element(element)
{
    ScriptWrappable::init(this);
}

InputMethodContext::~InputMethodContext()
{
}

Composition* InputMethodContext::composition() const
{
    // FIXME: Implement this. This should lazily update the composition object
    // here.
    return m_composition.get();
}

String InputMethodContext::locale() const
{
    // FIXME: Implement this.
    return emptyString();
}

HTMLElement* InputMethodContext::target() const
{
    return m_element;
}

void InputMethodContext::confirmComposition()
{
    Frame* frame = m_element->document().frame();
    if (!frame)
        return;

    const Element* element = frame->document()->focusedElement();
    if (!element || !element->isHTMLElement() || m_element != toHTMLElement(element))
        return;

    frame->inputMethodController().confirmCompositionAndResetState();
}

void InputMethodContext::setCaretRectangle(Node* anchor, int x, int y, int w, int h)
{
    // FIXME: Implement this.
}

void InputMethodContext::setExclusionRectangle(Node* anchor, int x, int y, int w, int h)
{
    // FIXME: Implement this.
}

} // namespace WebCore
