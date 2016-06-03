// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CEReactionsScope.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/custom/CustomElementReactionStack.h"
#include "core/frame/FrameHost.h"

namespace blink {

CEReactionsScope* CEReactionsScope::s_topOfStack = nullptr;

void CEReactionsScope::enqueue(
    Element* element,
    CustomElementReaction* reaction)
{
    if (!m_frameHost.get()) {
        m_frameHost = element->document().frameHost();
        m_frameHost->customElementReactionStack().push();
    }
    m_frameHost->customElementReactionStack().enqueue(element, reaction);
}

void CEReactionsScope::invokeReactions()
{
    m_frameHost->customElementReactionStack().popInvokingReactions();
}

} // namespace blink
