/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nuanti Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/page/FocusController.h"

#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Range.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/SlotScopedTraversal.h"
#include "core/editing/EditingUtilities.h"  // For firstPositionInOrBeforeNode
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/events/Event.h"
#include "core/frame/FrameClient.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLShadowElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/html/TextControlElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutObject.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusChangedObserver.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/SpatialNavigation.h"

#include <limits>

namespace blink {

using namespace HTMLNames;

namespace {

inline bool IsShadowInsertionPointFocusScopeOwner(Element& element) {
  return IsActiveShadowInsertionPoint(element) &&
         toHTMLShadowElement(element).OlderShadowRoot();
}

class ScopedFocusNavigation {
  STACK_ALLOCATED();

 public:
  // Searches through the given tree scope, starting from start element, for
  // the next/previous selectable element that comes after/before start element.
  // The order followed is as specified in the HTML spec[1], which is elements
  // with tab indexes first (from lowest to highest), and then elements without
  // tab indexes (in document order).  The search algorithm also conforms the
  // Shadow DOM spec[2], which inserts sequence in a shadow tree into its host.
  //
  // @param start The element from which to start searching. The element after
  //              this will be focused. May be null.
  // @return The focus element that comes after/before start element.
  //
  // [1]
  // https://html.spec.whatwg.org/multipage/interaction.html#sequential-focus-navigation
  // [2] https://w3c.github.io/webcomponents/spec/shadow/#focus-navigation
  Element* FindFocusableElement(WebFocusType type) {
    return (type == kWebFocusTypeForward) ? NextFocusableElement()
                                          : PreviousFocusableElement();
  }

  Element* CurrentElement() const { return current_; }
  Element* Owner() const;

  static ScopedFocusNavigation CreateFor(const Element&);
  static ScopedFocusNavigation CreateForDocument(Document&);
  static ScopedFocusNavigation OwnedByNonFocusableFocusScopeOwner(Element&);
  static ScopedFocusNavigation OwnedByShadowHost(const Element&);
  static ScopedFocusNavigation OwnedByShadowInsertionPoint(HTMLShadowElement&);
  static ScopedFocusNavigation OwnedByHTMLSlotElement(const HTMLSlotElement&);
  static ScopedFocusNavigation OwnedByIFrame(const HTMLFrameOwnerElement&);
  static HTMLSlotElement* FindFallbackScopeOwnerSlot(const Element&);
  static bool IsSlotFallbackScoped(const Element&);
  static bool IsSlotFallbackScopedForThisSlot(const HTMLSlotElement&,
                                              const Element&);

 private:
  ScopedFocusNavigation(TreeScope&, const Element*);
  ScopedFocusNavigation(HTMLSlotElement&, const Element*);

  Element* FindElementWithExactTabIndex(int tab_index, WebFocusType);
  Element* NextElementWithGreaterTabIndex(int tab_index);
  Element* PreviousElementWithLowerTabIndex(int tab_index);
  Element* NextFocusableElement();
  Element* PreviousFocusableElement();

  void SetCurrentElement(Element* element) { current_ = element; }
  void MoveToNext();
  void MoveToPrevious();
  void MoveToFirst();
  void MoveToLast();

  Member<ContainerNode> root_node_;
  Member<HTMLSlotElement> root_slot_;
  Member<Element> current_;
  bool slot_fallback_traversal_;
};

ScopedFocusNavigation::ScopedFocusNavigation(TreeScope& tree_scope,
                                             const Element* current)
    : root_node_(tree_scope.RootNode()),
      root_slot_(nullptr),
      current_(const_cast<Element*>(current)) {}

ScopedFocusNavigation::ScopedFocusNavigation(HTMLSlotElement& slot,
                                             const Element* current)
    : root_node_(nullptr),
      root_slot_(&slot),
      current_(const_cast<Element*>(current)),
      slot_fallback_traversal_(slot.AssignedNodes().IsEmpty()) {}

void ScopedFocusNavigation::MoveToNext() {
  DCHECK(current_);
  if (root_slot_) {
    if (slot_fallback_traversal_) {
      current_ = ElementTraversal::Next(*current_, root_slot_);
      while (current_ &&
             !ScopedFocusNavigation::IsSlotFallbackScopedForThisSlot(
                 *root_slot_, *current_))
        current_ = ElementTraversal::Next(*current_, root_slot_);
    } else {
      current_ = SlotScopedTraversal::Next(*current_);
    }
  } else {
    current_ = ElementTraversal::Next(*current_);
    while (current_ && (SlotScopedTraversal::IsSlotScoped(*current_) ||
                        ScopedFocusNavigation::IsSlotFallbackScoped(*current_)))
      current_ = ElementTraversal::Next(*current_);
  }
}

void ScopedFocusNavigation::MoveToPrevious() {
  DCHECK(current_);
  if (root_slot_) {
    if (slot_fallback_traversal_) {
      current_ = ElementTraversal::Previous(*current_, root_slot_);
      if (current_ == root_slot_)
        current_ = nullptr;
      while (current_ &&
             !ScopedFocusNavigation::IsSlotFallbackScopedForThisSlot(
                 *root_slot_, *current_))
        current_ = ElementTraversal::Previous(*current_);
    } else {
      current_ = SlotScopedTraversal::Previous(*current_);
    }
  } else {
    current_ = ElementTraversal::Previous(*current_);
    while (current_ && (SlotScopedTraversal::IsSlotScoped(*current_) ||
                        ScopedFocusNavigation::IsSlotFallbackScoped(*current_)))
      current_ = ElementTraversal::Previous(*current_);
  }
}

void ScopedFocusNavigation::MoveToFirst() {
  if (root_slot_) {
    if (!slot_fallback_traversal_) {
      current_ = SlotScopedTraversal::FirstAssignedToSlot(*root_slot_);
    } else {
      Element* first = ElementTraversal::FirstChild(*root_slot_);
      while (first && !ScopedFocusNavigation::IsSlotFallbackScopedForThisSlot(
                          *root_slot_, *first))
        first = ElementTraversal::Next(*first, root_slot_);
      current_ = first;
    }
  } else {
    Element* first = root_node_->IsElementNode()
                         ? &ToElement(*root_node_)
                         : ElementTraversal::Next(*root_node_);
    while (first && (SlotScopedTraversal::IsSlotScoped(*first) ||
                     ScopedFocusNavigation::IsSlotFallbackScoped(*first)))
      first = ElementTraversal::Next(*first, root_node_);
    current_ = first;
  }
}

void ScopedFocusNavigation::MoveToLast() {
  if (root_slot_) {
    if (!slot_fallback_traversal_) {
      current_ = SlotScopedTraversal::LastAssignedToSlot(*root_slot_);
    } else {
      Element* last = ElementTraversal::LastWithin(*root_slot_);
      while (last && !ScopedFocusNavigation::IsSlotFallbackScopedForThisSlot(
                         *root_slot_, *last))
        last = ElementTraversal::Previous(*last, root_slot_);
      current_ = last;
    }
  } else {
    Element* last = ElementTraversal::LastWithin(*root_node_);
    while (last && (SlotScopedTraversal::IsSlotScoped(*last) ||
                    ScopedFocusNavigation::IsSlotFallbackScoped(*last)))
      last = ElementTraversal::Previous(*last, root_node_);
    current_ = last;
  }
}

Element* ScopedFocusNavigation::Owner() const {
  if (root_slot_)
    return root_slot_;
  DCHECK(root_node_);
  if (root_node_->IsShadowRoot()) {
    ShadowRoot& shadow_root = ToShadowRoot(*root_node_);
    return shadow_root.IsYoungest()
               ? &shadow_root.host()
               : shadow_root.ShadowInsertionPointOfYoungerShadowRoot();
  }
  // FIXME: Figure out the right thing for OOPI here.
  if (Frame* frame = root_node_->GetDocument().GetFrame())
    return frame->DeprecatedLocalOwner();
  return nullptr;
}

ScopedFocusNavigation ScopedFocusNavigation::CreateFor(const Element& current) {
  if (HTMLSlotElement* slot = SlotScopedTraversal::FindScopeOwnerSlot(current))
    return ScopedFocusNavigation(*slot, &current);
  if (HTMLSlotElement* slot =
          ScopedFocusNavigation::FindFallbackScopeOwnerSlot(current))
    return ScopedFocusNavigation(*slot, &current);
  return ScopedFocusNavigation(current.ContainingTreeScope(), &current);
}

ScopedFocusNavigation ScopedFocusNavigation::CreateForDocument(
    Document& document) {
  return ScopedFocusNavigation(document, nullptr);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByNonFocusableFocusScopeOwner(
    Element& element) {
  if (IsShadowHost(element))
    return ScopedFocusNavigation::OwnedByShadowHost(element);
  if (IsShadowInsertionPointFocusScopeOwner(element))
    return ScopedFocusNavigation::OwnedByShadowInsertionPoint(
        toHTMLShadowElement(element));
  DCHECK(isHTMLSlotElement(element));
  return ScopedFocusNavigation::OwnedByHTMLSlotElement(
      toHTMLSlotElement(element));
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByShadowHost(
    const Element& element) {
  DCHECK(IsShadowHost(element));
  return ScopedFocusNavigation(*&element.Shadow()->YoungestShadowRoot(),
                               nullptr);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByIFrame(
    const HTMLFrameOwnerElement& frame) {
  DCHECK(frame.ContentFrame());
  DCHECK(frame.ContentFrame()->IsLocalFrame());
  ToLocalFrame(frame.ContentFrame())->GetDocument()->UpdateDistribution();
  return ScopedFocusNavigation(
      *ToLocalFrame(frame.ContentFrame())->GetDocument(), nullptr);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByShadowInsertionPoint(
    HTMLShadowElement& shadow_insertion_point) {
  DCHECK(IsShadowInsertionPointFocusScopeOwner(shadow_insertion_point));
  return ScopedFocusNavigation(*shadow_insertion_point.OlderShadowRoot(),
                               nullptr);
}

ScopedFocusNavigation ScopedFocusNavigation::OwnedByHTMLSlotElement(
    const HTMLSlotElement& element) {
  return ScopedFocusNavigation(const_cast<HTMLSlotElement&>(element), nullptr);
}

HTMLSlotElement* ScopedFocusNavigation::FindFallbackScopeOwnerSlot(
    const Element& element) {
  Element* parent = const_cast<Element*>(element.parentElement());
  while (parent) {
    if (isHTMLSlotElement(parent))
      return toHTMLSlotElement(parent)->AssignedNodes().IsEmpty()
                 ? toHTMLSlotElement(parent)
                 : nullptr;
    parent = parent->parentElement();
  }
  return nullptr;
}

bool ScopedFocusNavigation::IsSlotFallbackScoped(const Element& element) {
  return ScopedFocusNavigation::FindFallbackScopeOwnerSlot(element);
}

bool ScopedFocusNavigation::IsSlotFallbackScopedForThisSlot(
    const HTMLSlotElement& slot,
    const Element& current) {
  Element* parent = current.parentElement();
  while (parent) {
    if (isHTMLSlotElement(parent) &&
        toHTMLSlotElement(parent)->AssignedNodes().IsEmpty())
      return !SlotScopedTraversal::IsSlotScoped(current) &&
             toHTMLSlotElement(parent) == slot;
    parent = parent->parentElement();
  }
  return false;
}

inline void DispatchBlurEvent(const Document& document,
                              Element& focused_element) {
  focused_element.DispatchBlurEvent(nullptr, kWebFocusTypePage);
  if (focused_element == document.FocusedElement()) {
    focused_element.DispatchFocusOutEvent(EventTypeNames::focusout, nullptr);
    if (focused_element == document.FocusedElement())
      focused_element.DispatchFocusOutEvent(EventTypeNames::DOMFocusOut,
                                            nullptr);
  }
}

inline void DispatchFocusEvent(const Document& document,
                               Element& focused_element) {
  focused_element.DispatchFocusEvent(0, kWebFocusTypePage);
  if (focused_element == document.FocusedElement()) {
    focused_element.DispatchFocusInEvent(EventTypeNames::focusin, nullptr,
                                         kWebFocusTypePage);
    if (focused_element == document.FocusedElement())
      focused_element.DispatchFocusInEvent(EventTypeNames::DOMFocusIn, nullptr,
                                           kWebFocusTypePage);
  }
}

inline void DispatchEventsOnWindowAndFocusedElement(Document* document,
                                                    bool focused) {
  DCHECK(document);
  // If we have a focused element we should dispatch blur on it before we blur
  // the window.  If we have a focused element we should dispatch focus on it
  // after we focus the window.  https://bugs.webkit.org/show_bug.cgi?id=27105

  // Do not fire events while modal dialogs are up.  See
  // https://bugs.webkit.org/show_bug.cgi?id=33962
  if (Page* page = document->GetPage()) {
    if (page->Suspended())
      return;
  }

  if (!focused && document->FocusedElement()) {
    Element* focused_element = document->FocusedElement();
    // Use focus_type kWebFocusTypePage, same as used in DispatchBlurEvent.
    focused_element->SetFocused(false, kWebFocusTypePage);
    focused_element->SetHasFocusWithinUpToAncestor(false, nullptr);
    DispatchBlurEvent(*document, *focused_element);
  }

  if (LocalDOMWindow* window = document->domWindow())
    window->DispatchEvent(
        Event::Create(focused ? EventTypeNames::focus : EventTypeNames::blur));
  if (focused && document->FocusedElement()) {
    Element* focused_element(document->FocusedElement());
    // Use focus_type kWebFocusTypePage, same as used in DispatchFocusEvent.
    focused_element->SetFocused(true, kWebFocusTypePage);
    focused_element->SetHasFocusWithinUpToAncestor(true, nullptr);
    DispatchFocusEvent(*document, *focused_element);
  }
}

inline bool HasCustomFocusLogic(const Element& element) {
  return element.IsHTMLElement() &&
         ToHTMLElement(element).HasCustomFocusLogic();
}

inline bool IsShadowHostWithoutCustomFocusLogic(const Element& element) {
  return IsShadowHost(element) && !HasCustomFocusLogic(element);
}

inline bool IsNonKeyboardFocusableShadowHost(const Element& element) {
  return IsShadowHostWithoutCustomFocusLogic(element) &&
         !(element.ShadowRootIfV1() ? element.IsFocusable()
                                    : element.IsKeyboardFocusable());
}

inline bool IsKeyboardFocusableShadowHost(const Element& element) {
  return IsShadowHostWithoutCustomFocusLogic(element) &&
         element.IsKeyboardFocusable();
}

inline bool IsNonFocusableFocusScopeOwner(Element& element) {
  return IsNonKeyboardFocusableShadowHost(element) ||
         IsShadowInsertionPointFocusScopeOwner(element) ||
         isHTMLSlotElement(element);
}

inline bool IsShadowHostDelegatesFocus(const Element& element) {
  return element.AuthorShadowRoot() &&
         element.AuthorShadowRoot()->delegatesFocus();
}

inline int AdjustedTabIndex(Element& element) {
  return (IsNonKeyboardFocusableShadowHost(element) ||
          IsShadowInsertionPointFocusScopeOwner(element))
             ? 0
             : element.tabIndex();
}

inline bool ShouldVisit(Element& element) {
  return element.IsKeyboardFocusable() ||
         IsNonFocusableFocusScopeOwner(element);
}

Element* ScopedFocusNavigation::FindElementWithExactTabIndex(
    int tab_index,
    WebFocusType type) {
  // Search is inclusive of start
  for (; CurrentElement();
       type == kWebFocusTypeForward ? MoveToNext() : MoveToPrevious()) {
    Element* current = CurrentElement();
    if (ShouldVisit(*current) && AdjustedTabIndex(*current) == tab_index)
      return current;
  }
  return nullptr;
}

Element* ScopedFocusNavigation::NextElementWithGreaterTabIndex(int tab_index) {
  // Search is inclusive of start
  int winning_tab_index = std::numeric_limits<int>::max();
  Element* winner = nullptr;
  for (; CurrentElement(); MoveToNext()) {
    Element* current = CurrentElement();
    int current_tab_index = AdjustedTabIndex(*current);
    if (ShouldVisit(*current) && current_tab_index > tab_index) {
      if (!winner || current_tab_index < winning_tab_index) {
        winner = current;
        winning_tab_index = current_tab_index;
      }
    }
  }
  SetCurrentElement(winner);
  return winner;
}

Element* ScopedFocusNavigation::PreviousElementWithLowerTabIndex(
    int tab_index) {
  // Search is inclusive of start
  int winning_tab_index = 0;
  Element* winner = nullptr;
  for (; CurrentElement(); MoveToPrevious()) {
    Element* current = CurrentElement();
    int current_tab_index = AdjustedTabIndex(*current);
    if (ShouldVisit(*current) && current_tab_index < tab_index &&
        current_tab_index > winning_tab_index) {
      winner = current;
      winning_tab_index = current_tab_index;
    }
  }
  SetCurrentElement(winner);
  return winner;
}

Element* ScopedFocusNavigation::NextFocusableElement() {
  Element* current = CurrentElement();
  if (current) {
    int tab_index = AdjustedTabIndex(*current);
    // If an element is excluded from the normal tabbing cycle, the next
    // focusable element is determined by tree order.
    if (tab_index < 0) {
      for (MoveToNext(); CurrentElement(); MoveToNext()) {
        current = CurrentElement();
        if (ShouldVisit(*current) && AdjustedTabIndex(*current) >= 0)
          return current;
      }
    } else {
      // First try to find an element with the same tabindex as start that comes
      // after start in the scope.
      MoveToNext();
      if (Element* winner =
              FindElementWithExactTabIndex(tab_index, kWebFocusTypeForward))
        return winner;
    }
    if (!tab_index) {
      // We've reached the last element in the document with a tabindex of 0.
      // This is the end of the tabbing order.
      return nullptr;
    }
  }

  // Look for the first element in the scope that:
  // 1) has the lowest tabindex that is higher than start's tabindex (or 0, if
  //    start is null), and
  // 2) comes first in the scope, if there's a tie.
  MoveToFirst();
  if (Element* winner = NextElementWithGreaterTabIndex(
          current ? AdjustedTabIndex(*current) : 0)) {
    return winner;
  }

  // There are no elements with a tabindex greater than start's tabindex,
  // so find the first element with a tabindex of 0.
  MoveToFirst();
  return FindElementWithExactTabIndex(0, kWebFocusTypeForward);
}

Element* ScopedFocusNavigation::PreviousFocusableElement() {
  // First try to find the last element in the scope that comes before start and
  // has the same tabindex as start.  If start is null, find the last element in
  // the scope with a tabindex of 0.
  int tab_index;
  Element* current = CurrentElement();
  if (current) {
    MoveToPrevious();
    tab_index = AdjustedTabIndex(*current);
  } else {
    MoveToLast();
    tab_index = 0;
  }

  // However, if an element is excluded from the normal tabbing cycle, the
  // previous focusable element is determined by tree order
  if (tab_index < 0) {
    for (; CurrentElement(); MoveToPrevious()) {
      current = CurrentElement();
      if (ShouldVisit(*current) && AdjustedTabIndex(*current) >= 0)
        return current;
    }
  } else {
    if (Element* winner =
            FindElementWithExactTabIndex(tab_index, kWebFocusTypeBackward))
      return winner;
  }

  // There are no elements before start with the same tabindex as start, so look
  // for an element that:
  // 1) has the highest non-zero tabindex (that is less than start's tabindex),
  //    and
  // 2) comes last in the scope, if there's a tie.
  tab_index =
      (current && tab_index) ? tab_index : std::numeric_limits<int>::max();
  MoveToLast();
  return PreviousElementWithLowerTabIndex(tab_index);
}

Element* FindFocusableElementRecursivelyForward(ScopedFocusNavigation& scope) {
  // Starting element is exclusive.
  while (Element* found = scope.FindFocusableElement(kWebFocusTypeForward)) {
    if (IsShadowHostDelegatesFocus(*found)) {
      // If tabindex is positive, find focusable element inside its shadow tree.
      if (found->tabIndex() >= 0 &&
          IsShadowHostWithoutCustomFocusLogic(*found)) {
        ScopedFocusNavigation inner_scope =
            ScopedFocusNavigation::OwnedByShadowHost(*found);
        if (Element* found_in_inner_focus_scope =
                FindFocusableElementRecursivelyForward(inner_scope))
          return found_in_inner_focus_scope;
      }
      // Skip to the next element in the same scope.
      continue;
    }
    if (!IsNonFocusableFocusScopeOwner(*found))
      return found;

    // Now |found| is on a non focusable scope owner (either shadow host or
    // <shadow> or slot) Find inside the inward scope and return it if found.
    // Otherwise continue searching in the same scope.
    ScopedFocusNavigation inner_scope =
        ScopedFocusNavigation::OwnedByNonFocusableFocusScopeOwner(*found);
    if (Element* found_in_inner_focus_scope =
            FindFocusableElementRecursivelyForward(inner_scope))
      return found_in_inner_focus_scope;
  }
  return nullptr;
}

Element* FindFocusableElementRecursivelyBackward(ScopedFocusNavigation& scope) {
  // Starting element is exclusive.
  while (Element* found = scope.FindFocusableElement(kWebFocusTypeBackward)) {
    // Now |found| is on a focusable shadow host.
    // Find inside shadow backwards. If any focusable element is found, return
    // it, otherwise return the host itself.
    if (IsKeyboardFocusableShadowHost(*found)) {
      ScopedFocusNavigation inner_scope =
          ScopedFocusNavigation::OwnedByShadowHost(*found);
      Element* found_in_inner_focus_scope =
          FindFocusableElementRecursivelyBackward(inner_scope);
      if (found_in_inner_focus_scope)
        return found_in_inner_focus_scope;
      if (IsShadowHostDelegatesFocus(*found))
        continue;
      return found;
    }

    // If delegatesFocus is true and tabindex is negative, skip the whole shadow
    // tree under the shadow host.
    if (IsShadowHostDelegatesFocus(*found) && found->tabIndex() < 0)
      continue;

    // Now |found| is on a non focusable scope owner (a shadow host, a <shadow>
    // or a slot).  Find focusable element in descendant scope. If not found,
    // find the next focusable element within the current scope.
    if (IsNonFocusableFocusScopeOwner(*found)) {
      ScopedFocusNavigation inner_scope =
          ScopedFocusNavigation::OwnedByNonFocusableFocusScopeOwner(*found);
      if (Element* found_in_inner_focus_scope =
              FindFocusableElementRecursivelyBackward(inner_scope))
        return found_in_inner_focus_scope;
      continue;
    }
    if (!IsShadowHostDelegatesFocus(*found))
      return found;
  }
  return nullptr;
}

Element* FindFocusableElementRecursively(WebFocusType type,
                                         ScopedFocusNavigation& scope) {
  return (type == kWebFocusTypeForward)
             ? FindFocusableElementRecursivelyForward(scope)
             : FindFocusableElementRecursivelyBackward(scope);
}

Element* FindFocusableElementDescendingDownIntoFrameDocument(WebFocusType type,
                                                             Element* element) {
  // The element we found might be a HTMLFrameOwnerElement, so descend down the
  // tree until we find either:
  // 1) a focusable element, or
  // 2) the deepest-nested HTMLFrameOwnerElement.
  while (element && element->IsFrameOwnerElement()) {
    HTMLFrameOwnerElement& owner = ToHTMLFrameOwnerElement(*element);
    if (!owner.ContentFrame() || !owner.ContentFrame()->IsLocalFrame())
      break;
    ToLocalFrame(owner.ContentFrame())
        ->GetDocument()
        ->UpdateStyleAndLayoutIgnorePendingStylesheets();
    ScopedFocusNavigation scope = ScopedFocusNavigation::OwnedByIFrame(owner);
    Element* found_element = FindFocusableElementRecursively(type, scope);
    if (!found_element)
      break;
    DCHECK_NE(element, found_element);
    element = found_element;
  }
  return element;
}

Element* FindFocusableElementAcrossFocusScopesForward(
    ScopedFocusNavigation& scope) {
  Element* current = scope.CurrentElement();
  Element* found;
  if (current && IsShadowHostWithoutCustomFocusLogic(*current)) {
    ScopedFocusNavigation inner_scope =
        ScopedFocusNavigation::OwnedByShadowHost(*current);
    Element* found_in_inner_focus_scope =
        FindFocusableElementRecursivelyForward(inner_scope);
    found = found_in_inner_focus_scope
                ? found_in_inner_focus_scope
                : FindFocusableElementRecursivelyForward(scope);
  } else {
    found = FindFocusableElementRecursivelyForward(scope);
  }

  // If there's no focusable element to advance to, move up the focus scopes
  // until we find one.
  ScopedFocusNavigation current_scope = scope;
  while (!found) {
    Element* owner = current_scope.Owner();
    if (!owner)
      break;
    current_scope = ScopedFocusNavigation::CreateFor(*owner);
    found = FindFocusableElementRecursivelyForward(current_scope);
  }
  return FindFocusableElementDescendingDownIntoFrameDocument(
      kWebFocusTypeForward, found);
}

Element* FindFocusableElementAcrossFocusScopesBackward(
    ScopedFocusNavigation& scope) {
  Element* found = FindFocusableElementRecursivelyBackward(scope);

  // If there's no focusable element to advance to, move up the focus scopes
  // until we find one.
  ScopedFocusNavigation current_scope = scope;
  while (!found) {
    Element* owner = current_scope.Owner();
    if (!owner)
      break;
    current_scope = ScopedFocusNavigation::CreateFor(*owner);
    if (IsKeyboardFocusableShadowHost(*owner) &&
        !IsShadowHostDelegatesFocus(*owner)) {
      found = owner;
      break;
    }
    found = FindFocusableElementRecursivelyBackward(current_scope);
  }
  return FindFocusableElementDescendingDownIntoFrameDocument(
      kWebFocusTypeBackward, found);
}

Element* FindFocusableElementAcrossFocusScopes(WebFocusType type,
                                               ScopedFocusNavigation& scope) {
  return (type == kWebFocusTypeForward)
             ? FindFocusableElementAcrossFocusScopesForward(scope)
             : FindFocusableElementAcrossFocusScopesBackward(scope);
}

}  // anonymous namespace

FocusController::FocusController(Page* page)
    : page_(page),
      is_active_(false),
      is_focused_(false),
      is_changing_focused_frame_(false) {}

FocusController* FocusController::Create(Page* page) {
  return new FocusController(page);
}

void FocusController::SetFocusedFrame(Frame* frame, bool notify_embedder) {
  DCHECK(!frame || frame->GetPage() == page_);
  if (focused_frame_ == frame || (is_changing_focused_frame_ && frame))
    return;

  is_changing_focused_frame_ = true;

  LocalFrame* old_frame = (focused_frame_ && focused_frame_->IsLocalFrame())
                              ? ToLocalFrame(focused_frame_.Get())
                              : nullptr;

  LocalFrame* new_frame =
      (frame && frame->IsLocalFrame()) ? ToLocalFrame(frame) : nullptr;

  focused_frame_ = frame;

  // Now that the frame is updated, fire events and update the selection focused
  // states of both frames.
  if (old_frame && old_frame->View()) {
    old_frame->Selection().SetFrameIsFocused(false);
    old_frame->DomWindow()->DispatchEvent(Event::Create(EventTypeNames::blur));
  }

  if (new_frame && new_frame->View() && IsFocused()) {
    new_frame->Selection().SetFrameIsFocused(true);
    new_frame->DomWindow()->DispatchEvent(Event::Create(EventTypeNames::focus));
  }

  is_changing_focused_frame_ = false;

  // Checking client() is necessary, as the frame might have been detached as
  // part of dispatching the focus event above. See https://crbug.com/570874.
  if (focused_frame_ && focused_frame_->Client() && notify_embedder)
    focused_frame_->Client()->FrameFocused();

  NotifyFocusChangedObservers();
}

void FocusController::FocusDocumentView(Frame* frame, bool notify_embedder) {
  DCHECK(!frame || frame->GetPage() == page_);
  if (focused_frame_ == frame)
    return;

  LocalFrame* focused_frame = (focused_frame_ && focused_frame_->IsLocalFrame())
                                  ? ToLocalFrame(focused_frame_.Get())
                                  : nullptr;
  if (focused_frame && focused_frame->View()) {
    Document* document = focused_frame->GetDocument();
    Element* focused_element = document ? document->FocusedElement() : nullptr;
    if (focused_element)
      DispatchBlurEvent(*document, *focused_element);
  }

  LocalFrame* new_focused_frame =
      (frame && frame->IsLocalFrame()) ? ToLocalFrame(frame) : nullptr;
  if (new_focused_frame && new_focused_frame->View()) {
    Document* document = new_focused_frame->GetDocument();
    Element* focused_element = document ? document->FocusedElement() : nullptr;
    if (focused_element)
      DispatchFocusEvent(*document, *focused_element);
  }

  // dispatchBlurEvent/dispatchFocusEvent could have changed the focused frame,
  // or detached the frame.
  if (new_focused_frame && !new_focused_frame->View())
    return;

  SetFocusedFrame(frame, notify_embedder);
}

LocalFrame* FocusController::FocusedFrame() const {
  // TODO(alexmos): Strengthen this to DCHECK that whoever called this really
  // expected a LocalFrame. Refactor call sites so that the rare cases that
  // need to know about focused RemoteFrames use a separate accessor (to be
  // added).
  if (focused_frame_ && focused_frame_->IsRemoteFrame())
    return nullptr;
  return ToLocalFrame(focused_frame_.Get());
}

Frame* FocusController::FocusedOrMainFrame() const {
  if (LocalFrame* frame = FocusedFrame())
    return frame;

  // FIXME: This is a temporary hack to ensure that we return a LocalFrame, even
  // when the mainFrame is remote.  FocusController needs to be refactored to
  // deal with RemoteFrames cross-process focus transfers.
  for (Frame* frame = &page_->MainFrame()->Tree().Top(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (frame->IsLocalRoot())
      return frame;
  }

  return page_->MainFrame();
}

HTMLFrameOwnerElement* FocusController::FocusedFrameOwnerElement(
    LocalFrame& current_frame) const {
  Frame* focused_frame = focused_frame_.Get();
  for (; focused_frame; focused_frame = focused_frame->Tree().Parent()) {
    if (focused_frame->Tree().Parent() == &current_frame) {
      DCHECK(focused_frame->Owner()->IsLocal());
      return focused_frame->DeprecatedLocalOwner();
    }
  }
  return nullptr;
}

bool FocusController::IsDocumentFocused(const Document& document) const {
  if (!IsActive() || !IsFocused())
    return false;

  return focused_frame_ &&
         focused_frame_->Tree().IsDescendantOf(document.GetFrame());
}

void FocusController::SetFocused(bool focused) {
  if (IsFocused() == focused)
    return;

  is_focused_ = focused;

  if (!is_focused_ && FocusedOrMainFrame()->IsLocalFrame())
    ToLocalFrame(FocusedOrMainFrame())->GetEventHandler().StopAutoscroll();

  // Do not set a focused frame when being unfocused. This might reset
  // m_isFocused to true.
  if (!focused_frame_ && is_focused_)
    SetFocusedFrame(page_->MainFrame());

  // setFocusedFrame above might reject to update m_focusedFrame, or
  // m_focusedFrame might be changed by blur/focus event handlers.
  if (focused_frame_ && focused_frame_->IsLocalFrame() &&
      ToLocalFrame(focused_frame_.Get())->View()) {
    ToLocalFrame(focused_frame_.Get())->Selection().SetFrameIsFocused(focused);
    DispatchEventsOnWindowAndFocusedElement(
        ToLocalFrame(focused_frame_.Get())->GetDocument(), focused);
  }

  NotifyFocusChangedObservers();
}

bool FocusController::SetInitialFocus(WebFocusType type) {
  bool did_advance_focus = AdvanceFocus(type, true);

  // If focus is being set initially, accessibility needs to be informed that
  // system focus has moved into the web area again, even if focus did not
  // change within WebCore.  PostNotification is called instead of
  // handleFocusedUIElementChanged, because this will send the notification even
  // if the element is the same.
  if (FocusedOrMainFrame()->IsLocalFrame()) {
    Document* document = ToLocalFrame(FocusedOrMainFrame())->GetDocument();
    if (AXObjectCache* cache = document->ExistingAXObjectCache())
      cache->HandleInitialFocus();
  }

  return did_advance_focus;
}

bool FocusController::AdvanceFocus(
    WebFocusType type,
    bool initial_focus,
    InputDeviceCapabilities* source_capabilities) {
  switch (type) {
    case kWebFocusTypeForward:
    case kWebFocusTypeBackward: {
      // We should never hit this when a RemoteFrame is focused, since the key
      // event that initiated focus advancement should've been routed to that
      // frame's process from the beginning.
      LocalFrame* starting_frame = ToLocalFrame(FocusedOrMainFrame());
      return AdvanceFocusInDocumentOrder(starting_frame, nullptr, type,
                                         initial_focus, source_capabilities);
    }
    case kWebFocusTypeLeft:
    case kWebFocusTypeRight:
    case kWebFocusTypeUp:
    case kWebFocusTypeDown:
      return AdvanceFocusDirectionally(type);
    default:
      NOTREACHED();
  }

  return false;
}

bool FocusController::AdvanceFocusAcrossFrames(
    WebFocusType type,
    RemoteFrame* from,
    LocalFrame* to,
    InputDeviceCapabilities* source_capabilities) {
  // If we are shifting focus from a child frame to its parent, the
  // child frame has no more focusable elements, and we should continue
  // looking for focusable elements in the parent, starting from the <iframe>
  // element of the child frame.
  Element* start = nullptr;
  if (from->Tree().Parent() == to) {
    DCHECK(from->Owner()->IsLocal());
    start = ToHTMLFrameOwnerElement(from->Owner());
  }

  // If we're coming from a parent frame, we need to restart from the first or
  // last focusable element.
  bool initial_focus = to->Tree().Parent() == from;

  return AdvanceFocusInDocumentOrder(to, start, type, initial_focus,
                                     source_capabilities);
}

#if DCHECK_IS_ON()
inline bool IsNonFocusableShadowHost(const Element& element) {
  return IsShadowHostWithoutCustomFocusLogic(element) && !element.IsFocusable();
}
#endif

bool FocusController::AdvanceFocusInDocumentOrder(
    LocalFrame* frame,
    Element* start,
    WebFocusType type,
    bool initial_focus,
    InputDeviceCapabilities* source_capabilities) {
  DCHECK(frame);
  Document* document = frame->GetDocument();
  document->UpdateDistribution();

  Element* current = start;
#if DCHECK_IS_ON()
  DCHECK(!current || !IsNonFocusableShadowHost(*current));
#endif
  if (!current && !initial_focus)
    current = document->SequentialFocusNavigationStartingPoint(type);

  document->UpdateStyleAndLayoutIgnorePendingStylesheets();
  ScopedFocusNavigation scope =
      current ? ScopedFocusNavigation::CreateFor(*current)
              : ScopedFocusNavigation::CreateForDocument(*document);
  Element* element = FindFocusableElementAcrossFocusScopes(type, scope);
  if (!element) {
    // If there's a RemoteFrame on the ancestor chain, we need to continue
    // searching for focusable elements there.
    if (frame->LocalFrameRoot() != frame->Tree().Top()) {
      document->ClearFocusedElement();
      document->SetSequentialFocusNavigationStartingPoint(nullptr);
      SetFocusedFrame(nullptr);
      ToRemoteFrame(frame->LocalFrameRoot().Tree().Parent())
          ->AdvanceFocus(type, &frame->LocalFrameRoot());
      return true;
    }

    // We didn't find an element to focus, so we should try to pass focus to
    // Chrome.
    if (!initial_focus && page_->GetChromeClient().CanTakeFocus(type)) {
      document->ClearFocusedElement();
      document->SetSequentialFocusNavigationStartingPoint(nullptr);
      SetFocusedFrame(nullptr);
      page_->GetChromeClient().TakeFocus(type);
      return true;
    }

    // Chrome doesn't want focus, so we should wrap focus.
    ScopedFocusNavigation scope = ScopedFocusNavigation::CreateForDocument(
        *ToLocalFrame(page_->MainFrame())->GetDocument());
    element = FindFocusableElementRecursively(type, scope);
    element =
        FindFocusableElementDescendingDownIntoFrameDocument(type, element);

    if (!element)
      return false;
  }

  if (element == document->FocusedElement()) {
    // Focus is either coming from a remote frame or has wrapped around.
    if (FocusedFrame() != document->GetFrame()) {
      SetFocusedFrame(document->GetFrame());
      DispatchFocusEvent(*document, *element);
    }
    return true;
  }

  if (element->IsFrameOwnerElement() &&
      (!IsHTMLPlugInElement(*element) || !element->IsKeyboardFocusable())) {
    // We focus frames rather than frame owners.
    // FIXME: We should not focus frames that have no scrollbars, as focusing
    // them isn't useful to the user.
    HTMLFrameOwnerElement* owner = ToHTMLFrameOwnerElement(element);
    if (!owner->ContentFrame())
      return false;

    document->ClearFocusedElement();

    // If ContentFrame is remote, continue the search for focusable elements in
    // that frame's process. The target ContentFrame's process will grab focus
    // from inside AdvanceFocusInDocumentOrder().
    //
    // ClearFocusedElement() fires events that might detach the contentFrame,
    // hence the need to null-check it again.
    if (owner->ContentFrame() && owner->ContentFrame()->IsRemoteFrame())
      ToRemoteFrame(owner->ContentFrame())->AdvanceFocus(type, frame);
    else
      SetFocusedFrame(owner->ContentFrame());

    return true;
  }

  DCHECK(element->IsFocusable());

  // FIXME: It would be nice to just be able to call setFocusedElement(element)
  // here, but we can't do that because some elements (e.g. HTMLInputElement
  // and HTMLTextAreaElement) do extra work in their focus() methods.
  Document& new_document = element->GetDocument();

  if (&new_document != document) {
    // Focus is going away from this document, so clear the focused element.
    document->ClearFocusedElement();
    document->SetSequentialFocusNavigationStartingPoint(nullptr);
  }

  SetFocusedFrame(new_document.GetFrame());

  element->focus(
      FocusParams(SelectionBehaviorOnFocus::kReset, type, source_capabilities));
  return true;
}

Element* FocusController::FindFocusableElement(WebFocusType type,
                                               Element& element) {
  // FIXME: No spacial navigation code yet.
  DCHECK(type == kWebFocusTypeForward || type == kWebFocusTypeBackward);
  ScopedFocusNavigation scope = ScopedFocusNavigation::CreateFor(element);
  return FindFocusableElementAcrossFocusScopes(type, scope);
}

Element* FocusController::NextFocusableElementInForm(Element* element,
                                                     WebFocusType focus_type) {
  element->GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (!element->IsHTMLElement())
    return nullptr;

  if (!element->IsFormControlElement() &&
      !ToHTMLElement(element)->isContentEditableForBinding())
    return nullptr;

  HTMLFormElement* form_owner = nullptr;
  if (ToHTMLElement(element)->isContentEditableForBinding())
    form_owner = Traversal<HTMLFormElement>::FirstAncestor(*element);
  else
    form_owner = ToHTMLFormControlElement(element)->formOwner();

  if (!form_owner)
    return nullptr;

  Element* next_element = element;
  for (next_element = FindFocusableElement(focus_type, *next_element);
       next_element;
       next_element = FindFocusableElement(focus_type, *next_element)) {
    if (!next_element->IsHTMLElement())
      continue;
    if (ToHTMLElement(next_element)->isContentEditableForBinding() &&
        next_element->IsDescendantOf(form_owner))
      return next_element;
    if (!next_element->IsFormControlElement())
      continue;
    HTMLFormControlElement* form_element =
        ToHTMLFormControlElement(next_element);
    if (form_element->formOwner() != form_owner ||
        form_element->IsDisabledOrReadOnly())
      continue;
    LayoutObject* layout = next_element->GetLayoutObject();
    if (layout && layout->IsTextControl()) {
      // TODO(ajith.v) Extend it for select elements, radio buttons and check
      // boxes
      return next_element;
    }
  }
  return nullptr;
}

Element* FocusController::FindFocusableElementInShadowHost(
    const Element& shadow_host) {
  DCHECK(shadow_host.AuthorShadowRoot());
  ScopedFocusNavigation scope =
      ScopedFocusNavigation::OwnedByShadowHost(shadow_host);
  return FindFocusableElementAcrossFocusScopes(kWebFocusTypeForward, scope);
}

static bool RelinquishesEditingFocus(const Element& element) {
  DCHECK(HasEditableStyle(element));
  return element.GetDocument().GetFrame() && RootEditableElement(element);
}

bool FocusController::SetFocusedElement(Element* element,
                                        Frame* new_focused_frame) {
  return SetFocusedElement(
      element, new_focused_frame,
      FocusParams(SelectionBehaviorOnFocus::kNone, kWebFocusTypeNone, nullptr));
}

bool FocusController::SetFocusedElement(Element* element,
                                        Frame* new_focused_frame,
                                        const FocusParams& params) {
  LocalFrame* old_focused_frame = FocusedFrame();
  Document* old_document =
      old_focused_frame ? old_focused_frame->GetDocument() : nullptr;

  Element* old_focused_element =
      old_document ? old_document->FocusedElement() : nullptr;
  if (element && old_focused_element == element)
    return true;

  if (old_focused_element && IsRootEditableElement(*old_focused_element) &&
      !RelinquishesEditingFocus(*old_focused_element))
    return false;

  if (old_focused_frame)
    old_focused_frame->GetInputMethodController().WillChangeFocus();

  Document* new_document = nullptr;
  if (element)
    new_document = &element->GetDocument();
  else if (new_focused_frame && new_focused_frame->IsLocalFrame())
    new_document = ToLocalFrame(new_focused_frame)->GetDocument();

  if (new_document && old_document == new_document &&
      new_document->FocusedElement() == element)
    return true;


  if (old_document && old_document != new_document)
    old_document->ClearFocusedElement();

  if (new_focused_frame && !new_focused_frame->GetPage()) {
    SetFocusedFrame(nullptr);
    return false;
  }
  SetFocusedFrame(new_focused_frame);

  if (new_document) {
    bool successfully_focused =
        new_document->SetFocusedElement(element, params);
    if (!successfully_focused)
      return false;
  }

  return true;
}

void FocusController::SetActive(bool active) {
  if (is_active_ == active)
    return;

  is_active_ = active;

  Frame* frame = FocusedOrMainFrame();
  if (frame->IsLocalFrame()) {
    Document* const document =
        ToLocalFrame(frame)->LocalFrameRoot().GetDocument();
    DCHECK(document);
    if (!document->IsActive())
      return;
    // Invalidate all custom scrollbars because they support the CSS
    // window-active attribute. This should be applied to the entire page so
    // we invalidate from the root LocalFrameView instead of just the focused.
    if (LocalFrameView* view = document->View())
      view->InvalidateAllCustomScrollbarsOnActiveChanged();
    ToLocalFrame(frame)->Selection().PageActivationChanged();
  }
}

static void UpdateFocusCandidateIfNeeded(WebFocusType type,
                                         const FocusCandidate& current,
                                         FocusCandidate& candidate,
                                         FocusCandidate& closest) {
  DCHECK(candidate.visible_node->IsElementNode());
  DCHECK(candidate.visible_node->GetLayoutObject());

  // Ignore iframes that don't have a src attribute
  if (FrameOwnerElement(candidate) &&
      (!FrameOwnerElement(candidate)->ContentFrame() ||
       candidate.rect.IsEmpty()))
    return;

  // Ignore off screen child nodes of containers that do not scroll
  // (overflow:hidden)
  if (candidate.is_offscreen && !CanBeScrolledIntoView(type, candidate))
    return;

  DistanceDataForNode(type, current, candidate);
  if (candidate.distance == MaxDistance())
    return;

  if (candidate.is_offscreen_after_scrolling)
    return;

  if (closest.IsNull()) {
    closest = candidate;
    return;
  }

  LayoutRect intersection_rect = Intersection(candidate.rect, closest.rect);
  if (!intersection_rect.IsEmpty() &&
      !AreElementsOnSameLine(closest, candidate) &&
      intersection_rect == candidate.rect) {
    // If 2 nodes are intersecting, do hit test to find which node in on top.
    LayoutUnit x = intersection_rect.X() + intersection_rect.Width() / 2;
    LayoutUnit y = intersection_rect.Y() + intersection_rect.Height() / 2;
    if (!candidate.visible_node->GetDocument()
             .GetPage()
             ->MainFrame()
             ->IsLocalFrame())
      return;
    HitTestResult result =
        candidate.visible_node->GetDocument()
            .GetPage()
            ->DeprecatedLocalMainFrame()
            ->GetEventHandler()
            .HitTestResultAtPoint(IntPoint(x.ToInt(), y.ToInt()),
                                  HitTestRequest::kReadOnly |
                                      HitTestRequest::kActive |
                                      HitTestRequest::kIgnoreClipping);
    if (candidate.visible_node->contains(result.InnerNode())) {
      closest = candidate;
      return;
    }
    if (closest.visible_node->contains(result.InnerNode()))
      return;
  }

  if (candidate.distance < closest.distance)
    closest = candidate;
}

void FocusController::FindFocusCandidateInContainer(
    Node& container,
    const LayoutRect& starting_rect,
    WebFocusType type,
    FocusCandidate& closest) {
  Element* focused_element =
      (FocusedFrame() && FocusedFrame()->GetDocument())
          ? FocusedFrame()->GetDocument()->FocusedElement()
          : nullptr;

  Element* element = ElementTraversal::FirstWithin(container);
  FocusCandidate current;
  current.rect = starting_rect;
  current.focusable_node = focused_element;
  current.visible_node = focused_element;

  for (; element;
       element =
           (element->IsFrameOwnerElement() ||
            CanScrollInDirection(element, type))
               ? ElementTraversal::NextSkippingChildren(*element, &container)
               : ElementTraversal::Next(*element, &container)) {
    if (element == focused_element)
      continue;

    if (!element->IsKeyboardFocusable() && !element->IsFrameOwnerElement() &&
        !CanScrollInDirection(element, type))
      continue;

    FocusCandidate candidate = FocusCandidate(element, type);
    if (candidate.IsNull())
      continue;

    candidate.enclosing_scrollable_box = &container;
    UpdateFocusCandidateIfNeeded(type, current, candidate, closest);
  }
}

bool FocusController::AdvanceFocusDirectionallyInContainer(
    Node* container,
    const LayoutRect& starting_rect,
    WebFocusType type) {
  if (!container)
    return false;

  LayoutRect new_starting_rect = starting_rect;

  if (starting_rect.IsEmpty())
    new_starting_rect =
        VirtualRectForDirection(type, NodeRectInAbsoluteCoordinates(container));

  // Find the closest node within current container in the direction of the
  // navigation.
  FocusCandidate focus_candidate;
  FindFocusCandidateInContainer(*container, new_starting_rect, type,
                                focus_candidate);

  if (focus_candidate.IsNull()) {
    // Nothing to focus, scroll if possible.
    // NOTE: If no scrolling is performed (i.e. scrollInDirection returns
    // false), the spatial navigation algorithm will skip this container.
    return ScrollInDirection(container, type);
  }

  HTMLFrameOwnerElement* frame_element = FrameOwnerElement(focus_candidate);
  // If we have an iframe without the src attribute, it will not have a
  // contentFrame().  We DCHECK here to make sure that
  // updateFocusCandidateIfNeeded() will never consider such an iframe as a
  // candidate.
  DCHECK(!frame_element || frame_element->ContentFrame());
  if (frame_element && frame_element->ContentFrame()->IsLocalFrame()) {
    if (focus_candidate.is_offscreen_after_scrolling) {
      ScrollInDirection(&focus_candidate.visible_node->GetDocument(), type);
      return true;
    }
    // Navigate into a new frame.
    LayoutRect rect;
    Element* focused_element =
        ToLocalFrame(FocusedOrMainFrame())->GetDocument()->FocusedElement();
    if (focused_element && !HasOffscreenRect(focused_element))
      rect = NodeRectInAbsoluteCoordinates(focused_element,
                                           true /* ignore border */);
    ToLocalFrame(frame_element->ContentFrame())
        ->GetDocument()
        ->UpdateStyleAndLayoutIgnorePendingStylesheets();
    if (!AdvanceFocusDirectionallyInContainer(
            ToLocalFrame(frame_element->ContentFrame())->GetDocument(), rect,
            type)) {
      // The new frame had nothing interesting, need to find another candidate.
      return AdvanceFocusDirectionallyInContainer(
          container,
          NodeRectInAbsoluteCoordinates(focus_candidate.visible_node, true),
          type);
    }
    return true;
  }

  if (CanScrollInDirection(focus_candidate.visible_node, type)) {
    if (focus_candidate.is_offscreen_after_scrolling) {
      ScrollInDirection(focus_candidate.visible_node, type);
      return true;
    }
    // Navigate into a new scrollable container.
    LayoutRect starting_rect;
    Element* focused_element =
        ToLocalFrame(FocusedOrMainFrame())->GetDocument()->FocusedElement();
    if (focused_element && !HasOffscreenRect(focused_element))
      starting_rect = NodeRectInAbsoluteCoordinates(focused_element, true);
    return AdvanceFocusDirectionallyInContainer(focus_candidate.visible_node,
                                                starting_rect, type);
  }
  if (focus_candidate.is_offscreen_after_scrolling) {
    Node* container = focus_candidate.enclosing_scrollable_box;
    ScrollInDirection(container, type);
    return true;
  }

  // We found a new focus node, navigate to it.
  Element* element = ToElement(focus_candidate.focusable_node);
  DCHECK(element);

  if (!element->IsTextControl() && !HasEditableStyle(*element->ToNode())) {
    // To fulfill the expectation of spatial-navigation/snav-textarea.html
    // we clear selection when spatnav moves focus away from a text-field.
    // TODO(hugoh@opera.com): crbug.com/734552 remove Selection.Clear()
    if (FocusedFrame())
      FocusedFrame()->Selection().Clear();
  }
  element->focus(FocusParams(SelectionBehaviorOnFocus::kReset, type, nullptr));
  return true;
}

bool FocusController::AdvanceFocusDirectionally(WebFocusType type) {
  // FIXME: Directional focus changes don't yet work with RemoteFrames.
  if (!FocusedOrMainFrame()->IsLocalFrame())
    return false;
  LocalFrame* cur_frame = ToLocalFrame(FocusedOrMainFrame());
  DCHECK(cur_frame);

  Document* focused_document = cur_frame->GetDocument();
  if (!focused_document)
    return false;

  Element* focused_element = focused_document->FocusedElement();
  Node* container = focused_document;

  if (container->IsDocumentNode())
    ToDocument(container)->UpdateStyleAndLayoutIgnorePendingStylesheets();

  // Figure out the starting rect.
  LayoutRect starting_rect;
  if (focused_element) {
    if (!HasOffscreenRect(focused_element)) {
      container = ScrollableEnclosingBoxOrParentFrameForNodeInDirection(
          type, focused_element);
      starting_rect = NodeRectInAbsoluteCoordinates(focused_element,
                                                    true /* ignore border */);
    } else if (isHTMLAreaElement(*focused_element)) {
      HTMLAreaElement& area = toHTMLAreaElement(*focused_element);
      container = ScrollableEnclosingBoxOrParentFrameForNodeInDirection(
          type, area.ImageElement());
      starting_rect = VirtualRectForAreaElementAndDirection(area, type);
    }
  }

  bool consumed = false;
  do {
    consumed =
        AdvanceFocusDirectionallyInContainer(container, starting_rect, type);
    starting_rect =
        NodeRectInAbsoluteCoordinates(container, true /* ignore border */);
    container =
        ScrollableEnclosingBoxOrParentFrameForNodeInDirection(type, container);
    if (container && container->IsDocumentNode())
      ToDocument(container)->UpdateStyleAndLayoutIgnorePendingStylesheets();
  } while (!consumed && container);

  return consumed;
}

void FocusController::RegisterFocusChangedObserver(
    FocusChangedObserver* observer) {
  DCHECK(observer);
  DCHECK(!focus_changed_observers_.Contains(observer));
  focus_changed_observers_.insert(observer);
}

void FocusController::NotifyFocusChangedObservers() const {
  for (const auto& it : focus_changed_observers_)
    it->FocusedFrameChanged();
}

DEFINE_TRACE(FocusController) {
  visitor->Trace(page_);
  visitor->Trace(focused_frame_);
  visitor->Trace(focus_changed_observers_);
}

}  // namespace blink
