// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementReactionQueue.h"

#include "core/dom/Element.h"
#include "core/html/custom/CustomElementReaction.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

CustomElementReactionQueue::CustomElementReactionQueue() : index_(0u) {}

CustomElementReactionQueue::~CustomElementReactionQueue() {}

DEFINE_TRACE(CustomElementReactionQueue) {
  visitor->Trace(reactions_);
}

void CustomElementReactionQueue::Add(CustomElementReaction* reaction) {
  reactions_.push_back(reaction);
}

// There is one queue per element, so this could be invoked
// recursively.
void CustomElementReactionQueue::InvokeReactions(Element* element) {
  TRACE_EVENT1("blink", "CustomElementReactionQueue::invokeReactions", "name",
               element->localName().Utf8());
  while (index_ < reactions_.size()) {
    CustomElementReaction* reaction = reactions_[index_];
    reactions_[index_++] = nullptr;
    reaction->Invoke(element);
  }
  // Unlike V0CustomElementsCallbackQueue, reactions are always
  // inserted by steps which bump the global element queue. This
  // means we do not need queue "owner" guards.
  // https://html.spec.whatwg.org/multipage/scripting.html#custom-element-reactions
  Clear();
}

void CustomElementReactionQueue::Clear() {
  index_ = 0;
  reactions_.resize(0);
}

}  // namespace blink
