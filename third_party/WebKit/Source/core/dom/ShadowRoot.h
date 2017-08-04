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

#include "core/CoreExport.h"
#include "core/css/StyleSheetList.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Element.h"
#include "core/dom/TreeScope.h"
#include "platform/bindings/ScriptWrappableVisitor.h"

namespace blink {

class Document;
class ElementShadow;
class ExceptionState;
class HTMLShadowElement;
class V0InsertionPoint;
class ShadowRootRareDataV0;
class SlotAssignment;
class WhitespaceAttacher;

enum class ShadowRootType { kUserAgent, V0, kOpen, kClosed };

class CORE_EXPORT ShadowRoot final : public DocumentFragment, public TreeScope {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ShadowRoot);

 public:
  // FIXME: Current implementation does not work well if a shadow root is
  // dynamically created.  So multiple shadow subtrees in several elements are
  // prohibited.
  // See https://github.com/w3c/webcomponents/issues/102 and
  // http://crbug.com/234020
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
    return (GetType() == ShadowRootType::V0 ||
            GetType() == ShadowRootType::kOpen)
               ? "open"
               : "closed";
  };

  bool IsOpenOrV0() const {
    return GetType() == ShadowRootType::V0 ||
           GetType() == ShadowRootType::kOpen;
  }
  bool IsV1() const {
    return GetType() == ShadowRootType::kOpen ||
           GetType() == ShadowRootType::kClosed;
  }

  void AttachLayoutTree(AttachContext&) override;
  void DetachLayoutTree(const AttachContext& = AttachContext()) override;

  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;

  // For V0
  ShadowRoot* YoungerShadowRoot() const;
  ShadowRoot* OlderShadowRoot() const;
  ShadowRoot* olderShadowRootForBindings() const;
  void SetYoungerShadowRoot(ShadowRoot&);
  void SetOlderShadowRoot(ShadowRoot&);
  bool IsYoungest() const { return !YoungerShadowRoot(); }
  bool IsOldest() const { return !OlderShadowRoot(); }
  bool ContainsShadowElements() const;
  bool ContainsContentElements() const;
  bool ContainsInsertionPoints() const {
    return ContainsShadowElements() || ContainsContentElements();
  }
  unsigned DescendantShadowElementCount() const;
  HTMLShadowElement* ShadowInsertionPointOfYoungerShadowRoot() const;
  void SetShadowInsertionPointOfYoungerShadowRoot(HTMLShadowElement*);
  void DidAddInsertionPoint(V0InsertionPoint*);
  void DidRemoveInsertionPoint(V0InsertionPoint*);
  const HeapVector<Member<V0InsertionPoint>>& DescendantInsertionPoints();

  // For Internals, don't use this.
  unsigned ChildShadowRootCount() const { return child_shadow_root_count_; }

  void RecalcStyle(StyleRecalcChange);
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

  String innerHTML() const;
  void setInnerHTML(const String&, ExceptionState& = ASSERT_NO_EXCEPTION);

  Node* cloneNode(bool, ExceptionState&) override;

  void SetDelegatesFocus(bool flag) { delegates_focus_ = flag; }
  bool delegatesFocus() const { return delegates_focus_; }

  bool ContainsShadowRoots() const { return child_shadow_root_count_; }

  StyleSheetList& StyleSheets();
  void SetStyleSheets(StyleSheetList* style_sheet_list) {
    style_sheet_list_ = style_sheet_list;
    ScriptWrappableVisitor::WriteBarrier(style_sheet_list_);
  }

  DECLARE_VIRTUAL_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

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
  void SkipRebuildLayoutTree(WhitespaceAttacher&) const;

  Member<ShadowRootRareDataV0> shadow_root_rare_data_v0_;
  Member<StyleSheetList> style_sheet_list_;
  Member<SlotAssignment> slot_assignment_;
  unsigned child_shadow_root_count_ : 13;
  unsigned type_ : 2;
  unsigned registered_with_parent_shadow_root_ : 1;
  unsigned descendant_insertion_points_is_valid_ : 1;
  unsigned delegates_focus_ : 1;
};

inline Element* ShadowRoot::ActiveElement() const {
  return AdjustedFocusedElement();
}

inline ShadowRoot* Element::ShadowRootIfV1() const {
  ShadowRoot* root = this->GetShadowRoot();
  if (root && root->IsV1())
    return root;
  return nullptr;
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
