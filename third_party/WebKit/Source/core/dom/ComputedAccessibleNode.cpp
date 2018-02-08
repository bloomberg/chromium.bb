// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ComputedAccessibleNode.h"

#include <stdint.h>

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "build/build_config.h"
#include "core/dom/DOMException.h"
#include "core/frame/LocalFrame.h"
#include "platform/bindings/ScriptState.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"
#include "public/web/WebFrameClient.h"
#include "third_party/WebKit/Source/core/frame/WebLocalFrameImpl.h"

namespace blink {

namespace {
#if (defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN))
const double kIdleTaskStartTimeoutDelayMs = 1000.0;
#else
const double kIdleTaskStartTimeoutDelayMs = 4000.0;  // For ChromeOS, Mobile
#endif
}

ComputedAccessibleNodePromiseResolver*
ComputedAccessibleNodePromiseResolver::Create(ScriptState* script_state,
                                              Element& element) {
  return new ComputedAccessibleNodePromiseResolver(script_state, element);
}

ComputedAccessibleNodePromiseResolver::ComputedAccessibleNodePromiseResolver(
    ScriptState* script_state,
    Element& element)
    : idle_task_status_(kIdleTaskNotStarted),
      element_(element),
      resolver_(ScriptPromiseResolver::Create(script_state)),
      context_(ExecutionContext::From(script_state)) {}

ScriptPromise ComputedAccessibleNodePromiseResolver::Promise() {
  return resolver_->Promise();
}

void ComputedAccessibleNodePromiseResolver::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(resolver_);
  visitor->Trace(context_);
}

void ComputedAccessibleNodePromiseResolver::ComputeAccessibleNode() {
  DCHECK(RuntimeEnabledFeatures::AccessibilityObjectModelEnabled());
  DCHECK(Platform::Current()->CurrentThread()->Scheduler());
  idle_task_status_ = kIdleTaskNotStarted;
  Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
      FROM_HERE,
      WTF::Bind(&ComputedAccessibleNodePromiseResolver::IdleTaskFired,
                WrapPersistent(this)));

  // We post the below task to check if the above idle task isn't late.
  // There's no risk of concurrency as both tasks are on the same thread.
  context_->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostDelayedTask(
          FROM_HERE,
          WTF::Bind(
              &ComputedAccessibleNodePromiseResolver::IdleTaskStartTimeoutEvent,
              WrapPersistent(this)),
          TimeDelta::FromMillisecondsD(kIdleTaskStartTimeoutDelayMs));
}

void ComputedAccessibleNodePromiseResolver::IdleTaskFired(
    double deadline_seconds) {
  UpdateTreeAndResolve();
}

void ComputedAccessibleNodePromiseResolver::UpdateTreeAndResolve() {
  idle_task_status_ = kIdleTaskStarted;
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
  idle_task_status_ = kIdleTaskCompleted;
}

void ComputedAccessibleNodePromiseResolver::IdleTaskStartTimeoutEvent() {
  if (idle_task_status_ != kIdleTaskNotStarted)
    return;

  // If the idle task does not start after a delay threshold, just start it
  // manually.
  UpdateTreeAndResolve();
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

void ComputedAccessibleNode::OnSnapshotResponse(
    ScriptPromiseResolver* resolver) {
  resolver->Resolve(this);
}

void ComputedAccessibleNode::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
