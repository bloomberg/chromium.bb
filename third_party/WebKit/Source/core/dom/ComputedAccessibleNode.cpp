// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ComputedAccessibleNode.h"

#include <stdint.h>

#include "core/dom/DOMException.h"
#include "platform/bindings/ScriptState.h"
#include "third_party/WebKit/Source/bindings/core/v8/ScriptPromiseResolver.h"
#include "third_party/WebKit/Source/core/frame/LocalFrame.h"
#include "third_party/WebKit/Source/core/frame/WebLocalFrameImpl.h"
#include "third_party/WebKit/Source/platform/wtf/text/WTFString.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"

namespace blink {

ComputedAccessibleNode* ComputedAccessibleNode::Create(Element* element) {
  return new ComputedAccessibleNode(element);
}

ComputedAccessibleNode::ComputedAccessibleNode(Element* element)
    : element_(element) {
  DCHECK(RuntimeEnabledFeatures::AccessibilityObjectModelEnabled());
  AXObjectCache* cache = element->GetDocument().GetOrCreateAXObjectCache();
  DCHECK(cache);
  cache_ = cache;

  LocalFrame* local_frame = element->ownerDocument()->GetFrame();
  WebFrameClient* client = WebLocalFrameImpl::FromFrame(local_frame)->Client();
  tree_ = client->GetOrCreateWebComputedAXTree();
}

ComputedAccessibleNode::~ComputedAccessibleNode() {}

ScriptPromise ComputedAccessibleNode::ComputeAccessibleProperties(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  // TODO(meredithl): Post this task asynchronously, with a callback into
  // this->OnSnapshotResponse.
  if (!tree_->ComputeAccessibilityTree()) {
    // TODO(meredithl): Change this exception to something relevant to AOM.
    resolver->Reject(DOMException::Create(kUnknownError));
  } else {
    OnSnapshotResponse(resolver);
  }

  return promise;
}

ScriptPromise ComputedAccessibleNode::ensureUpToDate(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  // TODO(meredithl): Post this task asynchronously, with a callback into
  // this->OnSnapshotResponse.
  if (!tree_->ComputeAccessibilityTree()) {
    // TODO(meredithl): Change this exception to something relevant to AOM.
    resolver->Reject(DOMException::Create(kUnknownError));
  } else {
    OnUpdateResponse(resolver);
  }
  return promise;
}

int32_t ComputedAccessibleNode::GetIntAttribute(WebAOMIntAttribute attr,
                                                bool& is_null) const {
  int32_t out = 0;
  is_null = true;
  if (tree_->GetIntAttributeForAXNode(cache_->GetAXID(element_), attr, &out)) {
    is_null = false;
  }
  return out;
}

const String ComputedAccessibleNode::GetStringAttribute(
    WebAOMStringAttribute attr) const {
  WebString out;
  if (tree_->GetStringAttributeForAXNode(cache_->GetAXID(element_), attr,
                                         &out)) {
    return out;
  }
  return String();
}

bool ComputedAccessibleNode::GetBoolAttribute(WebAOMBoolAttribute attr,
                                              bool& is_null) const {
  bool out;
  is_null = true;
  if (tree_->GetBoolAttributeForAXNode(cache_->GetAXID(element_), attr, &out)) {
    is_null = false;
  }
  return out;
}

const String ComputedAccessibleNode::autocomplete() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_AUTOCOMPLETE);
}

const String ComputedAccessibleNode::keyShortcuts() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_KEY_SHORTCUTS);
}
const String ComputedAccessibleNode::name() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_NAME);
}
const String ComputedAccessibleNode::placeholder() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_PLACEHOLDER);
}

const String ComputedAccessibleNode::role() const {
  WebString out;
  if (tree_->GetRoleForAXNode(cache_->GetAXID(element_), &out)) {
    return out;
  }
  return String();
}

const String ComputedAccessibleNode::roleDescription() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_ROLE_DESCRIPTION);
}

const String ComputedAccessibleNode::valueText() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_VALUE_TEXT);
}

int32_t ComputedAccessibleNode::colCount(bool& is_null) const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_COLUMN_COUNT, is_null);
}

int32_t ComputedAccessibleNode::colIndex(bool& is_null) const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_COLUMN_INDEX, is_null);
}

int32_t ComputedAccessibleNode::colSpan(bool& is_null) const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_COLUMN_SPAN, is_null);
}

int32_t ComputedAccessibleNode::level(bool& is_null) const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_HIERARCHICAL_LEVEL,
                         is_null);
}

int32_t ComputedAccessibleNode::posInSet(bool& is_null) const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_POS_IN_SET, is_null);
}

int32_t ComputedAccessibleNode::rowCount(bool& is_null) const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_ROW_COUNT, is_null);
}

int32_t ComputedAccessibleNode::rowIndex(bool& is_null) const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_ROW_INDEX, is_null);
}

int32_t ComputedAccessibleNode::rowSpan(bool& is_null) const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_ROW_SPAN, is_null);
}

int32_t ComputedAccessibleNode::setSize(bool& is_null) const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_SET_SIZE, is_null);
}

ComputedAccessibleNode* ComputedAccessibleNode::GetRelationFromCache(
    AXID axid) const {
  Element* element = cache_->GetElementFromAXID(axid);
  if (!element)
    return nullptr;
  return element->GetComputedAccessibleNode();
}

ComputedAccessibleNode* ComputedAccessibleNode::parent() const {
  int32_t axid;
  if (!tree_->GetParentIdForAXNode(cache_->GetAXID(element_), &axid)) {
    return nullptr;
  }
  return GetRelationFromCache(axid);
}

ComputedAccessibleNode* ComputedAccessibleNode::firstChild() const {
  int32_t axid;
  if (!tree_->GetFirstChildIdForAXNode(cache_->GetAXID(element_), &axid)) {
    return nullptr;
  }
  return GetRelationFromCache(axid);
}

ComputedAccessibleNode* ComputedAccessibleNode::lastChild() const {
  int32_t axid;
  if (!tree_->GetLastChildIdForAXNode(cache_->GetAXID(element_), &axid)) {
    return nullptr;
  }
  return GetRelationFromCache(axid);
}

ComputedAccessibleNode* ComputedAccessibleNode::previousSibling() const {
  int32_t axid;
  if (!tree_->GetPreviousSiblingIdForAXNode(cache_->GetAXID(element_), &axid)) {
    return nullptr;
  }
  return GetRelationFromCache(axid);
}

ComputedAccessibleNode* ComputedAccessibleNode::nextSibling() const {
  int32_t axid;
  if (!tree_->GetNextSiblingIdForAXNode(cache_->GetAXID(element_), &axid)) {
    return nullptr;
  }
  return GetRelationFromCache(axid);
}

bool ComputedAccessibleNode::atomic(bool& is_null) const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_ATOMIC, is_null);
}

bool ComputedAccessibleNode::busy(bool& is_null) const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_BUSY, is_null);
}

bool ComputedAccessibleNode::modal(bool& is_null) const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_MODAL, is_null);
}

void ComputedAccessibleNode::OnSnapshotResponse(
    ScriptPromiseResolver* resolver) {
  resolver->Resolve(this);
}

void ComputedAccessibleNode::OnUpdateResponse(ScriptPromiseResolver* resolve) {
  resolve->Resolve();
}

void ComputedAccessibleNode::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(element_);
  visitor->Trace(cache_);
}

}  // namespace blink
