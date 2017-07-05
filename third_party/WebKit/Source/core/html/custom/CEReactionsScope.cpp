// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CEReactionsScope.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/html/custom/CustomElementReactionStack.h"

namespace blink {

CEReactionsScope* CEReactionsScope::top_of_stack_ = nullptr;

void CEReactionsScope::EnqueueToCurrentQueue(Element* element,
                                             CustomElementReaction* reaction) {
  if (!work_to_do_) {
    work_to_do_ = true;
    CustomElementReactionStack::Current().Push();
  }
  CustomElementReactionStack::Current().EnqueueToCurrentQueue(element,
                                                              reaction);
}

void CEReactionsScope::InvokeReactions() {
  CustomElementReactionStack::Current().PopInvokingReactions();
}

}  // namespace blink
