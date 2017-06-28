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

#include "core/dom/ElementRareData.h"

#include "core/css/cssom/InlineStylePropertyMap.h"
#include "core/dom/ResizeObservation.h"
#include "core/dom/ResizeObserver.h"

namespace blink {

struct SameSizeAsElementRareData : NodeRareData {
  IntSize scroll_offset;
  AtomicString nonce;
  void* pointers[1];
  Member<void*> members[14];
};

CSSStyleDeclaration& ElementRareData::EnsureInlineCSSStyleDeclaration(
    Element* owner_element) {
  if (!cssom_wrapper_)
    cssom_wrapper_ = new InlineCSSStyleDeclaration(owner_element);
  return *cssom_wrapper_;
}

InlineStylePropertyMap& ElementRareData::EnsureInlineStylePropertyMap(
    Element* owner_element) {
  if (!cssom_map_wrapper_) {
    cssom_map_wrapper_ = new InlineStylePropertyMap(owner_element);
  }
  return *cssom_map_wrapper_;
}

AttrNodeList& ElementRareData::EnsureAttrNodeList() {
  if (!attr_node_list_)
    attr_node_list_ = new AttrNodeList;
  return *attr_node_list_;
}

ElementRareData::ResizeObserverDataMap&
ElementRareData::EnsureResizeObserverData() {
  if (!resize_observer_data_)
    resize_observer_data_ =
        new HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>();
  return *resize_observer_data_;
}

DEFINE_TRACE_AFTER_DISPATCH(ElementRareData) {
  visitor->Trace(dataset_);
  visitor->Trace(class_list_);
  visitor->Trace(shadow_);
  visitor->Trace(attribute_map_);
  visitor->Trace(attr_node_list_);
  visitor->Trace(element_animations_);
  visitor->Trace(cssom_wrapper_);
  visitor->Trace(cssom_map_wrapper_);
  visitor->Trace(pseudo_element_data_);
  visitor->Trace(accessible_node_);
  visitor->Trace(v0_custom_element_definition_);
  visitor->Trace(custom_element_definition_);
  visitor->Trace(intersection_observer_data_);
  visitor->Trace(resize_observer_data_);
  NodeRareData::TraceAfterDispatch(visitor);
}

DEFINE_TRACE_WRAPPERS_AFTER_DISPATCH(ElementRareData) {
  if (attr_node_list_.Get()) {
    for (auto& attr : *attr_node_list_) {
      visitor->TraceWrappersWithManualWriteBarrier(attr);
    }
  }
  visitor->TraceWrappersWithManualWriteBarrier(shadow_);
  visitor->TraceWrappersWithManualWriteBarrier(attribute_map_);
  visitor->TraceWrappersWithManualWriteBarrier(dataset_);
  visitor->TraceWrappersWithManualWriteBarrier(class_list_);
  visitor->TraceWrappersWithManualWriteBarrier(accessible_node_);
  visitor->TraceWrappersWithManualWriteBarrier(intersection_observer_data_);
  NodeRareData::TraceWrappersAfterDispatch(visitor);
}

static_assert(sizeof(ElementRareData) == sizeof(SameSizeAsElementRareData),
              "ElementRareData should stay small");

}  // namespace blink
