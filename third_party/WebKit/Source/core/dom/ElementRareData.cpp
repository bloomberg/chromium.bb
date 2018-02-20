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
#include "core/resize_observer/ResizeObservation.h"
#include "core/resize_observer/ResizeObserver.h"
#include "core/style/ComputedStyle.h"
#include "third_party/WebKit/Source/core/dom/AXObjectCache.h"

namespace blink {

struct SameSizeAsElementRareData : NodeRareData {
  IntSize scroll_offset;
  void* pointers_or_strings[3];
  Member<void*> members[14];
};

ElementRareData::ElementRareData(NodeRenderingData* node_layout_data)
    : NodeRareData(node_layout_data), class_list_(nullptr) {
  is_element_rare_data_ = true;
}

ElementRareData::~ElementRareData() {
  DCHECK(!pseudo_element_data_);
}

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

void ElementRareData::SetComputedStyle(
    scoped_refptr<ComputedStyle> computed_style) {
  computed_style_ = std::move(computed_style);
}

void ElementRareData::ClearComputedStyle() {
  computed_style_ = nullptr;
}

AttrNodeList& ElementRareData::EnsureAttrNodeList() {
  if (!attr_node_list_)
    attr_node_list_ = new AttrNodeList;
  return *attr_node_list_;
}

ElementRareData::ResizeObserverDataMap&
ElementRareData::EnsureResizeObserverData() {
  if (!resize_observer_data_)
    resize_observer_data_ = new HeapHashMap<TraceWrapperMember<ResizeObserver>,
                                            Member<ResizeObservation>>();
  return *resize_observer_data_;
}

void ElementRareData::TraceAfterDispatch(blink::Visitor* visitor) {
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

void ElementRareData::TraceWrappersAfterDispatch(
    const ScriptWrappableVisitor* visitor) const {
  if (attr_node_list_.Get()) {
    for (auto& attr : *attr_node_list_) {
      visitor->TraceWrappers(attr);
    }
  }
  visitor->TraceWrappers(dataset_);
  visitor->TraceWrappers(shadow_);
  visitor->TraceWrappers(class_list_);
  visitor->TraceWrappers(attribute_map_);
  visitor->TraceWrappers(accessible_node_);
  visitor->TraceWrappers(intersection_observer_data_);
  if (resize_observer_data_) {
    for (auto& resize_observer : resize_observer_data_->Keys()) {
      visitor->TraceWrappers(resize_observer);
    }
  }
  NodeRareData::TraceWrappersAfterDispatch(visitor);
}

static_assert(sizeof(ElementRareData) == sizeof(SameSizeAsElementRareData),
              "ElementRareData should stay small");

}  // namespace blink
