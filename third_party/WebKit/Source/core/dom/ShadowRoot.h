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

#ifndef ShadowRoot_h
#define ShadowRoot_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/CoreExport.h"
#include "core/css/StyleSheetList.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Element.h"
#include "core/dom/TreeScope.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

class Document;
class ElementShadow;
class ExceptionState;
class ShadowRootRareDataV0;
class SlotAssignment;
class StringOrTrustedHTML;
class V0InsertionPoint;
class WhitespaceAttacher;

enum class ShadowRootType { V0, kOpen, kClosed, kUserAgent };

class CORE_EXPORT ShadowRoot final : public DocumentFragment, public TreeScope {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ShadowRoot);

 public:
  static ShadowRoot* Create(Document& document, ShadowRootType type) {
    return new ShadowRoot(document, type);
  }

  // Disambiguate between Node and TreeScope hierarchies; TreeScope's
  // implementation is simpler.
  using TreeScope::GetDocument;
  using TreeScope::getElementById;

  // Make protected methods from base class public here.
  using TreeScope::SetDocument;
  using TreeScope::SetParentTreeScope;

  Element& host() const {
    DCHECK(ParentOrShadowHostNode());
    return *ToElement(ParentOrShadowHostNode());
  }
  ElementShadow* Owner() const { return host().Shadow(); }
  ShadowRootType GetType() const { return static_cast<ShadowRootType>(type_); }
  String mode() const {
    switch (GetType()) {
      case ShadowRootType::kUserAgent:
        // UA ShadowRoot should not be exposed to the Web.
        NOTREACHED();
        return "";
      case ShadowRootType::V0:
        // v0 ShadowRoot shouldn't support |mode|, however, we must return
        // something. Return "open" here for a historical reason.
        return "open";
      case ShadowRootType::kOpen:
        return "open";
      case ShadowRootType::kClosed:
        return "closed";
      default:
        NOTREACHED();
        return "";
    }
  }

  bool IsOpenOrV0() const {
    return GetType() == ShadowRootType::V0 ||
           GetType() == ShadowRootType::kOpen;
  }
  bool IsV1() const {
    return GetType() == ShadowRootType::kOpen ||
           GetType() == ShadowRootType::kClosed ||
           GetType() == ShadowRootType::kUserAgent;
  }
  bool IsUserAgent() const { return GetType() == ShadowRootType::kUserAgent; }

  void AttachLayoutTree(AttachContext&) override;
  void DetachLayoutTree(const AttachContext& = AttachContext()) override;

  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;

  void SetNeedsAssignmentRecalc();

  // For V0
  bool ContainsShadowElements() const;
  bool ContainsContentElements() const;
  bool ContainsInsertionPoints() const {
    return ContainsShadowElements() || ContainsContentElements();
  }
  unsigned DescendantShadowElementCount() const;
  void DidAddInsertionPoint(V0InsertionPoint*);
  void DidRemoveInsertionPoint(V0InsertionPoint*);
  const HeapVector<Member<V0InsertionPoint>>& DescendantInsertionPoints();

  // For Internals, don't use this.
  unsigned ChildShadowRootCount() const { return child_shadow_root_count_; }

  void RecalcStyle(StyleRecalcChange);
  void RecalcStylesForReattach();
  void RebuildLayoutTree(WhitespaceAttacher&);

  void RegisterScopedHTMLStyleChild();
  void UnregisterScopedHTMLStyleChild();

  SlotAssignment& GetSlotAssignment() {
    DCHECK(slot_assignment_);
    return *slot_assignment_;
  }

  HTMLSlotElement* AssignedSlotFor(const Node&);
  void DidAddSlot(HTMLSlotElement&);
  void DidChangeHostChildSlotName(const AtomicString& old_value,
                                  const AtomicString& new_value);

  void DistributeV1();

  Element* ActiveElement() const;

  String InnerHTMLAsString() const;
  void SetInnerHTMLFromString(const String&,
                              ExceptionState& = ASSERT_NO_EXCEPTION);

  // TrustedTypes variants of the above.
  // TODO(mkwst): Write a spec for these bits. https://crbug.com/739170
  void innerHTML(StringOrTrustedHTML&) const;
  void setInnerHTML(const StringOrTrustedHTML&, ExceptionState&);

  Node* Clone(Document&, CloneChildrenFlag) const override;

  void SetDelegatesFocus(bool flag) { delegates_focus_ = flag; }
  bool delegatesFocus() const { return delegates_focus_; }

  bool ContainsShadowRoots() const { return child_shadow_root_count_; }

  StyleSheetList& StyleSheets();
  void SetStyleSheets(StyleSheetList* style_sheet_list) {
    style_sheet_list_ = style_sheet_list;
  }

  virtual void Trace(blink::Visitor*);
  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  ShadowRoot(Document&, ShadowRootType);
  ~ShadowRoot() override;

  void ChildrenChanged(const ChildrenChange&) override;

  ShadowRootRareDataV0& EnsureShadowRootRareDataV0();
  SlotAssignment& EnsureSlotAssignment();

  void AddChildShadowRoot() { ++child_shadow_root_count_; }
  void RemoveChildShadowRoot() {
    DCHECK_GT(child_shadow_root_count_, 0u);
    --child_shadow_root_count_;
  }
  void InvalidateDescendantInsertionPoints();

  Member<ShadowRootRareDataV0> shadow_root_rare_data_v0_;
  TraceWrapperMember<StyleSheetList> style_sheet_list_;
  Member<SlotAssignment> slot_assignment_;
  unsigned short child_shadow_root_count_;
  unsigned short type_ : 2;
  unsigned short registered_with_parent_shadow_root_ : 1;
  unsigned short descendant_insertion_points_is_valid_ : 1;
  unsigned short delegates_focus_ : 1;
  unsigned short unused_ : 11;
};

inline Element* ShadowRoot::ActiveElement() const {
  return AdjustedFocusedElement();
}

inline bool Node::IsInUserAgentShadowRoot() const {
  return ContainingShadowRoot() && ContainingShadowRoot()->IsUserAgent();
}

DEFINE_NODE_TYPE_CASTS(ShadowRoot, IsShadowRoot());
DEFINE_TYPE_CASTS(ShadowRoot,
                  TreeScope,
                  treeScope,
                  treeScope->RootNode().IsShadowRoot(),
                  treeScope.RootNode().IsShadowRoot());
DEFINE_TYPE_CASTS(TreeScope, ShadowRoot, shadowRoot, true, true);

CORE_EXPORT std::ostream& operator<<(std::ostream&, const ShadowRootType&);

}  // namespace blink

#endif  // ShadowRoot_h
