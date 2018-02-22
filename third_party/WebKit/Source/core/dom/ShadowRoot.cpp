/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#include "core/dom/ShadowRoot.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/string_or_trusted_html.h"
#include "core/css/StyleEngine.h"
#include "core/css/StyleSheetList.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ShadowRootRareDataV0.h"
#include "core/dom/SlotAssignment.h"
#include "core/dom/Text.h"
#include "core/dom/V0InsertionPoint.h"
#include "core/dom/WhitespaceAttacher.h"
#include "core/dom/trustedtypes/TrustedHTML.h"
#include "core/editing/serializers/Serialization.h"
#include "core/html/HTMLShadowElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/layout/LayoutObject.h"
#include "public/platform/Platform.h"

namespace blink {

struct SameSizeAsShadowRoot : public DocumentFragment, public TreeScope {
  char empty_class_fields_due_to_gc_mixin_marker[1];
  Member<void*> willbe_member[3];
  unsigned counters_and_flags[1];
};

static_assert(sizeof(ShadowRoot) == sizeof(SameSizeAsShadowRoot),
              "ShadowRoot should stay small");

ShadowRoot::ShadowRoot(Document& document, ShadowRootType type)
    : DocumentFragment(nullptr, kCreateShadowRoot),
      TreeScope(*this, document),
      style_sheet_list_(nullptr),
      child_shadow_root_count_(0),
      type_(static_cast<unsigned short>(type)),
      registered_with_parent_shadow_root_(false),
      descendant_insertion_points_is_valid_(false),
      delegates_focus_(false),
      unused_(0) {}

ShadowRoot::~ShadowRoot() = default;

SlotAssignment& ShadowRoot::EnsureSlotAssignment() {
  if (!slot_assignment_)
    slot_assignment_ = SlotAssignment::Create(*this);
  return *slot_assignment_;
}

HTMLSlotElement* ShadowRoot::AssignedSlotFor(const Node& node) {
  if (!slot_assignment_)
    return nullptr;
  return slot_assignment_->FindSlot(node);
}

void ShadowRoot::DidAddSlot(HTMLSlotElement& slot) {
  DCHECK(IsV1());
  EnsureSlotAssignment().DidAddSlot(slot);
}

void ShadowRoot::DidChangeHostChildSlotName(const AtomicString& old_value,
                                            const AtomicString& new_value) {
  if (!slot_assignment_)
    return;
  slot_assignment_->DidChangeHostChildSlotName(old_value, new_value);
}

Node* ShadowRoot::Clone(Document&, CloneChildrenFlag) const {
  NOTREACHED() << "ShadowRoot nodes are not clonable.";
  return nullptr;
}

String ShadowRoot::InnerHTMLAsString() const {
  return CreateMarkup(this, kChildrenOnly);
}

void ShadowRoot::innerHTML(StringOrTrustedHTML& result) const {
  result.SetString(InnerHTMLAsString());
}

void ShadowRoot::SetInnerHTMLFromString(const String& markup,
                                        ExceptionState& exception_state) {
  if (DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
          markup, &host(), kAllowScriptingContent, "innerHTML",
          exception_state))
    ReplaceChildrenWithFragment(this, fragment, exception_state);
}

void ShadowRoot::setInnerHTML(const StringOrTrustedHTML& stringOrHtml,
                              ExceptionState& exception_state) {
  DCHECK(stringOrHtml.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());

  if (stringOrHtml.IsString() && GetDocument().RequireTrustedTypes()) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedHTML` assignment.");
    return;
  }

  String html = stringOrHtml.IsString()
                    ? stringOrHtml.GetAsString()
                    : stringOrHtml.GetAsTrustedHTML()->toString();

  SetInnerHTMLFromString(html, exception_state);
}

void ShadowRoot::RecalcStyle(StyleRecalcChange change) {
  // ShadowRoot doesn't support custom callbacks.
  DCHECK(!HasCustomStyleCallbacks());

  if (GetStyleChangeType() >= kSubtreeStyleChange) {
    change = kForce;
    if (NeedsAttach())
      SetNeedsReattachLayoutTree();
  }

  // There's no style to update so just calling recalcStyle means we're updated.
  ClearNeedsStyleRecalc();

  if (change >= kUpdatePseudoElements || ChildNeedsStyleRecalc())
    RecalcDescendantStyles(change);
  ClearChildNeedsStyleRecalc();
}

void ShadowRoot::RecalcStylesForReattach() {
  // ShadowRoot doesn't support custom callbacks.
  DCHECK(!HasCustomStyleCallbacks());
  RecalcDescendantStylesForReattach();
}

void ShadowRoot::RebuildLayoutTree(WhitespaceAttacher& whitespace_attacher) {
  ClearNeedsReattachLayoutTree();
  RebuildChildrenLayoutTrees(whitespace_attacher);
  ClearChildNeedsReattachLayoutTree();
}

void ShadowRoot::AttachLayoutTree(AttachContext& context) {
  DocumentFragment::AttachLayoutTree(context);
}

void ShadowRoot::DetachLayoutTree(const AttachContext& context) {
  if (context.clear_invalidation) {
    GetDocument().GetStyleEngine().GetStyleInvalidator().ClearInvalidation(
        *this);
  }
  DocumentFragment::DetachLayoutTree(context);
}

Node::InsertionNotificationRequest ShadowRoot::InsertedInto(
    ContainerNode* insertion_point) {
  DocumentFragment::InsertedInto(insertion_point);

  if (!insertion_point->isConnected())
    return kInsertionDone;

  // FIXME: When parsing <video controls>, insertedInto() is called many times
  // without invoking removedFrom.  For now, we check
  // m_registeredWithParentShadowroot. We would like to
  // DCHECK(!m_registeredShadowRoot) here.
  // https://bugs.webkit.org/show_bug.cig?id=101316
  if (registered_with_parent_shadow_root_)
    return kInsertionDone;

  if (ShadowRoot* root = host().ContainingShadowRoot()) {
    root->AddChildShadowRoot();
    registered_with_parent_shadow_root_ = true;
  }

  return kInsertionDone;
}

void ShadowRoot::RemovedFrom(ContainerNode* insertion_point) {
  if (insertion_point->isConnected()) {
    GetDocument().GetStyleEngine().ShadowRootRemovedFromDocument(this);
    if (registered_with_parent_shadow_root_) {
      ShadowRoot* root = host().ContainingShadowRoot();
      if (!root)
        root = insertion_point->ContainingShadowRoot();
      if (root)
        root->RemoveChildShadowRoot();
      registered_with_parent_shadow_root_ = false;
    }
    if (NeedsStyleInvalidation()) {
      GetDocument().GetStyleEngine().GetStyleInvalidator().ClearInvalidation(
          *this);
    }
  }

  DocumentFragment::RemovedFrom(insertion_point);
}

void ShadowRoot::SetNeedsAssignmentRecalc() {
  DCHECK(RuntimeEnabledFeatures::IncrementalShadowDOMEnabled());
  DCHECK(IsV1());
  if (!slot_assignment_)
    return;
  return slot_assignment_->SetNeedsAssignmentRecalc();
}

void ShadowRoot::ChildrenChanged(const ChildrenChange& change) {
  ContainerNode::ChildrenChanged(change);

  if (change.IsChildElementChange()) {
    CheckForSiblingStyleChanges(
        change.type == kElementRemoved ? kSiblingElementRemoved
                                       : kSiblingElementInserted,
        ToElement(change.sibling_changed), change.sibling_before_change,
        change.sibling_after_change);
  }
}

ShadowRootRareDataV0& ShadowRoot::EnsureShadowRootRareDataV0() {
  if (shadow_root_rare_data_v0_)
    return *shadow_root_rare_data_v0_;

  shadow_root_rare_data_v0_ = new ShadowRootRareDataV0;
  return *shadow_root_rare_data_v0_;
}

bool ShadowRoot::ContainsShadowElements() const {
  return shadow_root_rare_data_v0_
             ? shadow_root_rare_data_v0_->ContainsShadowElements()
             : false;
}

bool ShadowRoot::ContainsContentElements() const {
  return shadow_root_rare_data_v0_
             ? shadow_root_rare_data_v0_->ContainsContentElements()
             : false;
}

unsigned ShadowRoot::DescendantShadowElementCount() const {
  return shadow_root_rare_data_v0_
             ? shadow_root_rare_data_v0_->DescendantShadowElementCount()
             : 0;
}

void ShadowRoot::DidAddInsertionPoint(V0InsertionPoint* insertion_point) {
  EnsureShadowRootRareDataV0().DidAddInsertionPoint(insertion_point);
  InvalidateDescendantInsertionPoints();
}

void ShadowRoot::DidRemoveInsertionPoint(V0InsertionPoint* insertion_point) {
  shadow_root_rare_data_v0_->DidRemoveInsertionPoint(insertion_point);
  InvalidateDescendantInsertionPoints();
}

void ShadowRoot::InvalidateDescendantInsertionPoints() {
  descendant_insertion_points_is_valid_ = false;
  shadow_root_rare_data_v0_->ClearDescendantInsertionPoints();
}

const HeapVector<Member<V0InsertionPoint>>&
ShadowRoot::DescendantInsertionPoints() {
  DEFINE_STATIC_LOCAL(HeapVector<Member<V0InsertionPoint>>, empty_list,
                      (new HeapVector<Member<V0InsertionPoint>>));
  if (shadow_root_rare_data_v0_ && descendant_insertion_points_is_valid_)
    return shadow_root_rare_data_v0_->DescendantInsertionPoints();

  descendant_insertion_points_is_valid_ = true;

  if (!ContainsInsertionPoints())
    return empty_list;

  HeapVector<Member<V0InsertionPoint>> insertion_points;
  for (V0InsertionPoint& insertion_point :
       Traversal<V0InsertionPoint>::DescendantsOf(*this))
    insertion_points.push_back(&insertion_point);

  EnsureShadowRootRareDataV0().SetDescendantInsertionPoints(insertion_points);

  return shadow_root_rare_data_v0_->DescendantInsertionPoints();
}

StyleSheetList& ShadowRoot::StyleSheets() {
  if (!style_sheet_list_)
    SetStyleSheets(StyleSheetList::Create(this));
  return *style_sheet_list_;
}

void ShadowRoot::DistributeV1() {
  EnsureSlotAssignment().RecalcDistribution();
}

void ShadowRoot::Trace(blink::Visitor* visitor) {
  visitor->Trace(shadow_root_rare_data_v0_);
  visitor->Trace(slot_assignment_);
  visitor->Trace(style_sheet_list_);
  TreeScope::Trace(visitor);
  DocumentFragment::Trace(visitor);
}

void ShadowRoot::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(style_sheet_list_);
  DocumentFragment::TraceWrappers(visitor);
}

std::ostream& operator<<(std::ostream& ostream, const ShadowRootType& type) {
  switch (type) {
    case ShadowRootType::kUserAgent:
      ostream << "UserAgent";
      break;
    case ShadowRootType::V0:
      ostream << "V0";
      break;
    case ShadowRootType::kOpen:
      ostream << "Open";
      break;
    case ShadowRootType::kClosed:
      ostream << "Closed";
      break;
  }
  return ostream;
}

}  // namespace blink
