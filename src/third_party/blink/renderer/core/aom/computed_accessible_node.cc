// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/aom/computed_accessible_node.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

class ComputedAccessibleNodePromiseResolver::RequestAnimationFrameCallback final
    : public FrameRequestCallbackCollection::FrameCallback {
 public:
  explicit RequestAnimationFrameCallback(
      ComputedAccessibleNodePromiseResolver* resolver)
      : resolver_(resolver) {}

  void Invoke(double) override {
    resolver_->continue_callback_request_id_ = 0;
    resolver_->UpdateTreeAndResolve();
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(resolver_);
    FrameRequestCallbackCollection::FrameCallback::Trace(visitor);
  }

 private:
  Member<ComputedAccessibleNodePromiseResolver> resolver_;

  DISALLOW_COPY_AND_ASSIGN(RequestAnimationFrameCallback);
};

ComputedAccessibleNodePromiseResolver::ComputedAccessibleNodePromiseResolver(
    ScriptState* script_state,
    Element& element)
    : element_(element),
      resolver_(MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      resolve_with_node_(false),
      ax_context_(std::make_unique<AXContext>(element_->GetDocument())) {}

ScriptPromise ComputedAccessibleNodePromiseResolver::Promise() {
  return resolver_->Promise();
}

void ComputedAccessibleNodePromiseResolver::Trace(Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(resolver_);
}

void ComputedAccessibleNodePromiseResolver::ComputeAccessibleNode() {
  resolve_with_node_ = true;
  EnsureUpToDate();
}

void ComputedAccessibleNodePromiseResolver::EnsureUpToDate() {
  DCHECK(RuntimeEnabledFeatures::AccessibilityObjectModelEnabled());
  if (continue_callback_request_id_)
    return;
  // TODO(aboxhall): Trigger a call when lifecycle is next at kPrePaintClean.
  RequestAnimationFrameCallback* callback =
      MakeGarbageCollected<RequestAnimationFrameCallback>(this);
  continue_callback_request_id_ =
      element_->GetDocument().RequestAnimationFrame(callback);
}

void ComputedAccessibleNodePromiseResolver::UpdateTreeAndResolve() {
  LocalFrame* local_frame = element_->ownerDocument()->GetFrame();
  if (!local_frame) {
    resolver_->Resolve();
    return;
  }
  WebLocalFrameClient* client =
      WebLocalFrameImpl::FromFrame(local_frame)->Client();
  WebComputedAXTree* tree = client->GetOrCreateWebComputedAXTree();
  tree->ComputeAccessibilityTree();

  if (!resolve_with_node_) {
    resolver_->Resolve();
    return;
  }

  Document& document = element_->GetDocument();
  document.View()->UpdateLifecycleToCompositingCleanPlusScrolling(
      DocumentUpdateReason::kAccessibility);
  AXObjectCache& cache = ax_context_->GetAXObjectCache();
  AXID ax_id = cache.GetAXID(element_);

  ComputedAccessibleNode* accessible_node =
      document.GetOrCreateComputedAccessibleNode(ax_id, tree);
  resolver_->Resolve(accessible_node);
}

// ComputedAccessibleNode ------------------------------------------------------

ComputedAccessibleNode::ComputedAccessibleNode(AXID ax_id,
                                               WebComputedAXTree* tree,
                                               Document* document)
    : ax_id_(ax_id),
      tree_(tree),
      document_(document),
      ax_context_(std::make_unique<AXContext>(*document)) {}

base::Optional<bool> ComputedAccessibleNode::atomic() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_ATOMIC);
}

base::Optional<bool> ComputedAccessibleNode::busy() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_BUSY);
}

base::Optional<bool> ComputedAccessibleNode::disabled() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_DISABLED);
}

base::Optional<bool> ComputedAccessibleNode::readOnly() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_READONLY);
}

base::Optional<bool> ComputedAccessibleNode::expanded() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_EXPANDED);
}

base::Optional<bool> ComputedAccessibleNode::modal() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_MODAL);
}

base::Optional<bool> ComputedAccessibleNode::multiline() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_MULTILINE);
}

base::Optional<bool> ComputedAccessibleNode::multiselectable() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_MULTISELECTABLE);
}

base::Optional<bool> ComputedAccessibleNode::required() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_REQUIRED);
}

base::Optional<bool> ComputedAccessibleNode::selected() const {
  return GetBoolAttribute(WebAOMBoolAttribute::AOM_ATTR_SELECTED);
}

base::Optional<int32_t> ComputedAccessibleNode::colCount() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_COLUMN_COUNT);
}

base::Optional<int32_t> ComputedAccessibleNode::colIndex() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_COLUMN_INDEX);
}

base::Optional<int32_t> ComputedAccessibleNode::colSpan() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_COLUMN_SPAN);
}

base::Optional<int32_t> ComputedAccessibleNode::level() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_HIERARCHICAL_LEVEL);
}

base::Optional<int32_t> ComputedAccessibleNode::posInSet() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_POS_IN_SET);
}

base::Optional<int32_t> ComputedAccessibleNode::rowCount() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_ROW_COUNT);
}

base::Optional<int32_t> ComputedAccessibleNode::rowIndex() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_ROW_INDEX);
}

base::Optional<int32_t> ComputedAccessibleNode::rowSpan() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_ROW_SPAN);
}

base::Optional<int32_t> ComputedAccessibleNode::setSize() const {
  return GetIntAttribute(WebAOMIntAttribute::AOM_ATTR_SET_SIZE);
}

base::Optional<float> ComputedAccessibleNode::valueMax() const {
  return GetFloatAttribute(WebAOMFloatAttribute::AOM_ATTR_VALUE_MAX);
}

base::Optional<float> ComputedAccessibleNode::valueMin() const {
  return GetFloatAttribute(WebAOMFloatAttribute::AOM_ATTR_VALUE_MIN);
}

base::Optional<float> ComputedAccessibleNode::valueNow() const {
  return GetFloatAttribute(WebAOMFloatAttribute::AOM_ATTR_VALUE_NOW);
}

ScriptPromise ComputedAccessibleNode::ensureUpToDate(
    ScriptState* script_state) {
  AXObjectCache* cache = document_->ExistingAXObjectCache();
  DCHECK(cache);
  Element* element = cache->GetElementFromAXID(ax_id_);
  auto* resolver = MakeGarbageCollected<ComputedAccessibleNodePromiseResolver>(
      script_state, *element);
  ScriptPromise promise = resolver->Promise();
  resolver->EnsureUpToDate();
  return promise;
}

const String ComputedAccessibleNode::autocomplete() const {
  return GetStringAttribute(WebAOMStringAttribute::AOM_ATTR_AUTOCOMPLETE);
}

const String ComputedAccessibleNode::checked() const {
  WebString out;
  if (tree_->GetCheckedStateForAXNode(ax_id_, &out)) {
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

ComputedAccessibleNode* ComputedAccessibleNode::parent() const {
  int32_t parent_ax_id;
  if (!tree_->GetParentIdForAXNode(ax_id_, &parent_ax_id)) {
    return nullptr;
  }
  return document_->GetOrCreateComputedAccessibleNode(parent_ax_id, tree_);
}

ComputedAccessibleNode* ComputedAccessibleNode::firstChild() const {
  int32_t child_ax_id;
  if (!tree_->GetFirstChildIdForAXNode(ax_id_, &child_ax_id)) {
    return nullptr;
  }
  return document_->GetOrCreateComputedAccessibleNode(child_ax_id, tree_);
}

ComputedAccessibleNode* ComputedAccessibleNode::lastChild() const {
  int32_t child_ax_id;
  if (!tree_->GetLastChildIdForAXNode(ax_id_, &child_ax_id)) {
    return nullptr;
  }
  return document_->GetOrCreateComputedAccessibleNode(child_ax_id, tree_);
}

ComputedAccessibleNode* ComputedAccessibleNode::previousSibling() const {
  int32_t sibling_ax_id;
  if (!tree_->GetPreviousSiblingIdForAXNode(ax_id_, &sibling_ax_id)) {
    return nullptr;
  }
  return document_->GetOrCreateComputedAccessibleNode(sibling_ax_id, tree_);
}

ComputedAccessibleNode* ComputedAccessibleNode::nextSibling() const {
  int32_t sibling_ax_id;
  if (!tree_->GetNextSiblingIdForAXNode(ax_id_, &sibling_ax_id)) {
    return nullptr;
  }
  return document_->GetOrCreateComputedAccessibleNode(sibling_ax_id, tree_);
}

base::Optional<bool> ComputedAccessibleNode::GetBoolAttribute(
    WebAOMBoolAttribute attr) const {
  bool value;
  if (tree_->GetBoolAttributeForAXNode(ax_id_, attr, &value))
    return value;
  return base::nullopt;
}

base::Optional<int32_t> ComputedAccessibleNode::GetIntAttribute(
    WebAOMIntAttribute attr) const {
  int32_t value;
  if (tree_->GetIntAttributeForAXNode(ax_id_, attr, &value))
    return value;
  return base::nullopt;
}

base::Optional<float> ComputedAccessibleNode::GetFloatAttribute(
    WebAOMFloatAttribute attr) const {
  float value;
  if (tree_->GetFloatAttributeForAXNode(ax_id_, attr, &value))
    return value;
  return base::nullopt;
}

const String ComputedAccessibleNode::GetStringAttribute(
    WebAOMStringAttribute attr) const {
  WebString out;
  if (tree_->GetStringAttributeForAXNode(ax_id_, attr, &out)) {
    return out;
  }
  return String();
}

void ComputedAccessibleNode::Trace(Visitor* visitor) {
  visitor->Trace(document_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
