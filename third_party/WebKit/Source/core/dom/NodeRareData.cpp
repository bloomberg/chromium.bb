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

#include "core/dom/NodeRareData.h"

#include "core/dom/Element.h"
#include "core/dom/ElementRareData.h"
#include "core/page/Page.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/heap/Handle.h"

namespace blink {

struct SameSizeAsNodeRareData {
  void* pointer_;
  Member<void*> willbe_member_[2];
  unsigned bitfields_;
};

static_assert(sizeof(NodeRareData) == sizeof(SameSizeAsNodeRareData),
              "NodeRareData should stay small");

void NodeRareData::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(mutation_observer_data_);
  // Do not keep empty NodeListsNodeData objects around.
  if (node_lists_ && node_lists_->IsEmpty())
    node_lists_.Clear();
  else
    visitor->Trace(node_lists_);
}

void NodeRareData::Trace(blink::Visitor* visitor) {
  if (is_element_rare_data_)
    static_cast<ElementRareData*>(this)->TraceAfterDispatch(visitor);
  else
    TraceAfterDispatch(visitor);
}

void NodeRareData::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  if (is_element_rare_data_)
    static_cast<const ElementRareData*>(this)->TraceWrappersAfterDispatch(
        visitor);
  else
    TraceWrappersAfterDispatch(visitor);
}

void NodeRareData::TraceWrappersAfterDispatch(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(node_lists_);
  visitor->TraceWrappers(mutation_observer_data_);
}

void NodeRareData::FinalizeGarbageCollectedObject() {
  if (is_element_rare_data_)
    static_cast<ElementRareData*>(this)->~ElementRareData();
  else
    this->~NodeRareData();
}

void NodeRareData::IncrementConnectedSubframeCount() {
  SECURITY_CHECK((connected_frame_count_ + 1) <= Page::kMaxNumberOfFrames);
  ++connected_frame_count_;
}

// Ensure the 10 bits reserved for the m_connectedFrameCount cannot overflow
static_assert(Page::kMaxNumberOfFrames <
                  (1 << NodeRareData::kConnectedFrameCountBits),
              "Frame limit should fit in rare data count");

}  // namespace blink
