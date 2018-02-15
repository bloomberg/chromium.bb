// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ComputedAccessibleNode.h"

#include <stdint.h>

#include "core/dom/DOMException.h"
#include "core/frame/LocalFrame.h"
#include "platform/bindings/ScriptState.h"
#include "platform/heap/Handle.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "public/platform/Platform.h"
#include "public/web/WebFrameClient.h"
#include "third_party/WebKit/Source/core/frame/WebLocalFrameImpl.h"

namespace blink {

ComputedAccessibleNodePromiseResolver*
ComputedAccessibleNodePromiseResolver::Create(ScriptState* script_state,
                                              Element& element) {
  return new ComputedAccessibleNodePromiseResolver(script_state, element);
}

ComputedAccessibleNodePromiseResolver::ComputedAccessibleNodePromiseResolver(
    ScriptState* script_state,
    Element& element)
    : element_(element),
      resolver_(ScriptPromiseResolver::Create(script_state)) {}

ScriptPromise ComputedAccessibleNodePromiseResolver::Promise() {
  return resolver_->Promise();
}

void ComputedAccessibleNodePromiseResolver::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(resolver_);
}

void ComputedAccessibleNodePromiseResolver::ComputeAccessibleNode() {
  DCHECK(RuntimeEnabledFeatures::AccessibilityObjectModelEnabled());
  DCHECK(Platform::Current()->CurrentThread()->Scheduler());
  Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
      FROM_HERE,
      WTF::Bind(&ComputedAccessibleNodePromiseResolver::IdleTaskFired,
                WrapPersistent(this)));
}

void ComputedAccessibleNodePromiseResolver::IdleTaskFired(
    double deadline_seconds) {
  UpdateTreeAndResolve();
}

void ComputedAccessibleNodePromiseResolver::UpdateTreeAndResolve() {
  Document& document = element_->GetDocument();
  document.View()->UpdateLifecycleToCompositingCleanPlusScrolling();
  AXObjectCache* cache = element_->GetDocument().GetOrCreateAXObjectCache();
  DCHECK(cache);
  AXID ax_id = cache->GetAXID(element_);

  LocalFrame* local_frame = element_->ownerDocument()->GetFrame();
  WebFrameClient* client = WebLocalFrameImpl::FromFrame(local_frame)->Client();
  WebComputedAXTree* tree = client->GetOrCreateWebComputedAXTree();
  tree->ComputeAccessibilityTree();

  ComputedAccessibleNode* accessible_node =
      ComputedAccessibleNode::Create(ax_id, tree);
  resolver_->Resolve(accessible_node);
}

ComputedAccessibleNode* ComputedAccessibleNode::Create(
    AXID ax_id,
    WebComputedAXTree* tree) {
  // TODO(meredithl): Change to GetOrCreate and check cache for existing node
  // with this ID.
  return new ComputedAccessibleNode(ax_id, tree);
}

ComputedAccessibleNode::ComputedAccessibleNode(AXID ax_id,
                                               WebComputedAXTree* tree)
    : ax_id_(ax_id), tree_(tree) {}

ComputedAccessibleNode::~ComputedAccessibleNode() {}

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
  if (tree_->GetIntAttributeForAXNode(ax_id_, attr, &out)) {
    is_null = false;
  }
  return out;
}

const String ComputedAccessibleNode::GetStringAttribute(
    WebAOMStringAttribute attr) const {
  WebString out;
  if (tree_->GetStringAttributeForAXNode(ax_id_, attr, &out)) {
    return out;
  }
  return String();
}

bool ComputedAccessibleNode::GetBoolAttribute(WebAOMBoolAttribute attr,
                                              bool& is_null) const {
  bool out;
  is_null = true;
  if (tree_->GetBoolAttributeForAXNode(ax_id_, attr, &out)) {
    is_null = false;
  }
  return out;
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
  if (tree_->GetRoleForAXNode(ax_id_, &out)) {
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

ComputedAccessibleNode* ComputedAccessibleNode::parent() const {
  int32_t parent_ax_id;
  if (!tree_->GetParentIdForAXNode(ax_id_, &parent_ax_id)) {
    return nullptr;
  }
  return ComputedAccessibleNode::Create(parent_ax_id, tree_);
}

ComputedAccessibleNode* ComputedAccessibleNode::firstChild() const {
  int32_t child_ax_id;
  if (!tree_->GetFirstChildIdForAXNode(ax_id_, &child_ax_id)) {
    return nullptr;
  }
  return ComputedAccessibleNode::Create(child_ax_id, tree_);
}

ComputedAccessibleNode* ComputedAccessibleNode::lastChild() const {
  int32_t child_ax_id;
  if (!tree_->GetLastChildIdForAXNode(ax_id_, &child_ax_id)) {
    return nullptr;
  }
  return ComputedAccessibleNode::Create(child_ax_id, tree_);
}

ComputedAccessibleNode* ComputedAccessibleNode::previousSibling() const {
  int32_t sibling_ax_id;
  if (!tree_->GetPreviousSiblingIdForAXNode(ax_id_, &sibling_ax_id)) {
    return nullptr;
  }
  return ComputedAccessibleNode::Create(sibling_ax_id, tree_);
}

ComputedAccessibleNode* ComputedAccessibleNode::nextSibling() const {
  int32_t sibling_ax_id;
  if (!tree_->GetNextSiblingIdForAXNode(ax_id_, &sibling_ax_id)) {
    return nullptr;
  }
  return ComputedAccessibleNode::Create(sibling_ax_id, tree_);
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
}

}  // namespace blink
