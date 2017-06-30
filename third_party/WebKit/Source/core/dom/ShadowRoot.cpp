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
#include "core/css/StyleSheetList.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleSharingDepthScope.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/InsertionPoint.h"
#include "core/dom/ShadowRootRareDataV0.h"
#include "core/dom/SlotAssignment.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/WhitespaceAttacher.h"
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
    : DocumentFragment(0, kCreateShadowRoot),
      TreeScope(*this, document),
      style_sheet_list_(nullptr),
      child_shadow_root_count_(0),
      type_(static_cast<unsigned>(type)),
      registered_with_parent_shadow_root_(false),
      descendant_insertion_points_is_valid_(false),
      delegates_focus_(false) {}

ShadowRoot::~ShadowRoot() {}

ShadowRoot* ShadowRoot::YoungerShadowRoot() const {
  if (GetType() == ShadowRootType::V0 && shadow_root_rare_data_v0_)
    return shadow_root_rare_data_v0_->YoungerShadowRoot();
  return nullptr;
}

ShadowRoot* ShadowRoot::OlderShadowRoot() const {
  if (GetType() == ShadowRootType::V0 && shadow_root_rare_data_v0_)
    return shadow_root_rare_data_v0_->OlderShadowRoot();
  return nullptr;
}

ShadowRoot* ShadowRoot::olderShadowRootForBindings() const {
  ShadowRoot* older = OlderShadowRoot();
  while (older && !older->IsOpenOrV0())
    older = older->OlderShadowRoot();
  DCHECK(!older || older->IsOpenOrV0());
  return older;
}

void ShadowRoot::SetYoungerShadowRoot(ShadowRoot& root) {
  DCHECK_EQ(GetType(), ShadowRootType::V0);
  EnsureShadowRootRareDataV0().SetYoungerShadowRoot(root);
}

void ShadowRoot::SetOlderShadowRoot(ShadowRoot& root) {
  DCHECK_EQ(GetType(), ShadowRootType::V0);
  EnsureShadowRootRareDataV0().SetOlderShadowRoot(root);
}

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

Node* ShadowRoot::cloneNode(bool, ExceptionState& exception_state) {
  exception_state.ThrowDOMException(kNotSupportedError,
                                    "ShadowRoot nodes are not clonable.");
  return nullptr;
}

String ShadowRoot::innerHTML() const {
  return CreateMarkup(this, kChildrenOnly);
}

void ShadowRoot::setInnerHTML(const String& markup,
                              ExceptionState& exception_state) {
  if (DocumentFragment* fragment = CreateFragmentForInnerOuterHTML(
          markup, &host(), kAllowScriptingContent, "innerHTML",
          exception_state))
    ReplaceChildrenWithFragment(this, fragment, exception_state);
}

void ShadowRoot::RecalcStyle(StyleRecalcChange change) {
  // ShadowRoot doesn't support custom callbacks.
  DCHECK(!HasCustomStyleCallbacks());

  StyleSharingDepthScope sharing_scope(*this);

  if (GetStyleChangeType() >= kSubtreeStyleChange) {
    change = kForce;
    if (NeedsAttach())
      SetNeedsReattachLayoutTree();
  }

  // There's no style to update so just calling recalcStyle means we're updated.
  ClearNeedsStyleRecalc();

  RecalcDescendantStyles(change);
  ClearChildNeedsStyleRecalc();
}

void ShadowRoot::RebuildLayoutTree(WhitespaceAttacher& whitespace_attacher) {
  if (!NeedsReattachLayoutTree() && !ChildNeedsReattachLayoutTree()) {
    SkipRebuildLayoutTree(whitespace_attacher);
    return;
  }

  StyleSharingDepthScope sharing_scope(*this);

  ClearNeedsReattachLayoutTree();
  RebuildChildrenLayoutTrees(whitespace_attacher);
  ClearChildNeedsReattachLayoutTree();
}

void ShadowRoot::SkipRebuildLayoutTree(
    WhitespaceAttacher& whitespace_attacher) const {
  // We call this method when neither this, nor our child nodes are marked
  // for re-attachment, but the host has been marked with
  // childNeedsReattachLayoutTree. That happens when ::before or ::after needs
  // re-attachment. In that case, we update nextTextSibling with the first text
  // node sibling not preceeded by any in-flow children to allow for correct
  // whitespace re-attachment if the ::before element display changes.
  DCHECK(GetDocument().InStyleRecalc());
  DCHECK(!GetDocument().ChildNeedsDistributionRecalc());
  DCHECK(!NeedsStyleRecalc());
  DCHECK(!ChildNeedsStyleRecalc());
  DCHECK(!NeedsReattachLayoutTree());
  DCHECK(!ChildNeedsReattachLayoutTree());

  for (Node* sibling = firstChild(); sibling;
       sibling = sibling->nextSibling()) {
    if (sibling->IsTextNode()) {
      whitespace_attacher.DidVisitText(ToText(sibling));
      return;
    }
    LayoutObject* layout_object = sibling->GetLayoutObject();
    if (layout_object && !layout_object->IsFloatingOrOutOfFlowPositioned()) {
      whitespace_attacher.DidVisitElement(ToElement(sibling));
      return;
    }
  }
}

void ShadowRoot::AttachLayoutTree(AttachContext& context) {
  StyleSharingDepthScope sharing_scope(*this);
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

  if (!insertion_point->isConnected() || !IsOldest())
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

void ShadowRoot::ChildrenChanged(const ChildrenChange& change) {
  ContainerNode::ChildrenChanged(change);

  if (change.IsChildElementChange()) {
    CheckForSiblingStyleChanges(
        change.type == kElementRemoved ? kSiblingElementRemoved
                                       : kSiblingElementInserted,
        ToElement(change.sibling_changed), change.sibling_before_change,
        change.sibling_after_change);
  }

  if (InsertionPoint* point = ShadowInsertionPointOfYoungerShadowRoot()) {
    if (ShadowRoot* root = point->ContainingShadowRoot())
      root->Owner()->SetNeedsDistributionRecalc();
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

HTMLShadowElement* ShadowRoot::ShadowInsertionPointOfYoungerShadowRoot() const {
  return shadow_root_rare_data_v0_
             ? shadow_root_rare_data_v0_
                   ->ShadowInsertionPointOfYoungerShadowRoot()
             : nullptr;
}

void ShadowRoot::SetShadowInsertionPointOfYoungerShadowRoot(
    HTMLShadowElement* shadow_insertion_point) {
  if (!shadow_root_rare_data_v0_ && !shadow_insertion_point)
    return;
  EnsureShadowRootRareDataV0().SetShadowInsertionPointOfYoungerShadowRoot(
      shadow_insertion_point);
}

void ShadowRoot::DidAddInsertionPoint(InsertionPoint* insertion_point) {
  EnsureShadowRootRareDataV0().DidAddInsertionPoint(insertion_point);
  InvalidateDescendantInsertionPoints();
}

void ShadowRoot::DidRemoveInsertionPoint(InsertionPoint* insertion_point) {
  shadow_root_rare_data_v0_->DidRemoveInsertionPoint(insertion_point);
  InvalidateDescendantInsertionPoints();
}

void ShadowRoot::InvalidateDescendantInsertionPoints() {
  descendant_insertion_points_is_valid_ = false;
  shadow_root_rare_data_v0_->ClearDescendantInsertionPoints();
}

const HeapVector<Member<InsertionPoint>>&
ShadowRoot::DescendantInsertionPoints() {
  DEFINE_STATIC_LOCAL(HeapVector<Member<InsertionPoint>>, empty_list,
                      (new HeapVector<Member<InsertionPoint>>));
  if (shadow_root_rare_data_v0_ && descendant_insertion_points_is_valid_)
    return shadow_root_rare_data_v0_->DescendantInsertionPoints();

  descendant_insertion_points_is_valid_ = true;

  if (!ContainsInsertionPoints())
    return empty_list;

  HeapVector<Member<InsertionPoint>> insertion_points;
  for (InsertionPoint& insertion_point :
       Traversal<InsertionPoint>::DescendantsOf(*this))
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
  EnsureSlotAssignment().ResolveDistribution();
}

DEFINE_TRACE(ShadowRoot) {
  visitor->Trace(shadow_root_rare_data_v0_);
  visitor->Trace(slot_assignment_);
  visitor->Trace(style_sheet_list_);
  TreeScope::Trace(visitor);
  DocumentFragment::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(ShadowRoot) {
  visitor->TraceWrappersWithManualWriteBarrier(style_sheet_list_);
  DocumentFragment::TraceWrappers(visitor);
}

std::ostream& operator<<(std::ostream& ostream, const ShadowRootType& type) {
  switch (type) {
    case ShadowRootType::kUserAgent:
      ostream << "ShadowRootType::UserAgent";
      break;
    case ShadowRootType::V0:
      ostream << "ShadowRootType::V0";
      break;
    case ShadowRootType::kOpen:
      ostream << "ShadowRootType::Open";
      break;
    case ShadowRootType::kClosed:
      ostream << "ShadowRootType::Closed";
      break;
  }
  return ostream;
}

}  // namespace blink
