/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/dom/Fullscreen.h"

#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutFullScreen.h"
#include "core/layout/api/LayoutFullScreenItem.h"
#include "core/page/ChromeClient.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/ScopedOrientationChangeIndicator.h"
#include "platform/UserGestureIndicator.h"
#include "platform/feature_policy/FeaturePolicy.h"

namespace blink {

namespace {

// https://html.spec.whatwg.org/multipage/embedded-content.html#allowed-to-use
bool AllowedToUseFullscreen(const Frame* frame) {
  // To determine whether a Document object |document| is allowed to use the
  // feature indicated by attribute name |allowattribute|, run these steps:

  // 1. If |document| has no browsing context, then return false.
  if (!frame)
    return false;

  if (!IsSupportedInFeaturePolicy(WebFeaturePolicyFeature::kFullscreen)) {
    // 2. If |document|'s browsing context is a top-level browsing context, then
    // return true.
    if (frame->IsMainFrame())
      return true;

    // 3. If |document|'s browsing context has a browsing context container that
    // is an iframe element with an |allowattribute| attribute specified, and
    // whose node document is allowed to use the feature indicated by
    // |allowattribute|, then return true.
    if (frame->Owner() && frame->Owner()->AllowFullscreen())
      return AllowedToUseFullscreen(frame->Tree().Parent());

    // 4. Return false.
    return false;
  }

  // 2. If Feature Policy is enabled, return the policy for "fullscreen"
  // feature.
  return frame->IsFeatureEnabled(WebFeaturePolicyFeature::kFullscreen);
}

bool AllowedToRequestFullscreen(Document& document) {
  // An algorithm is allowed to request fullscreen if one of the following is
  // true:

  //  The algorithm is triggered by a user activation.
  if (UserGestureIndicator::ProcessingUserGesture())
    return true;

  //  The algorithm is triggered by a user generated orientation change.
  if (ScopedOrientationChangeIndicator::ProcessingOrientationChange()) {
    UseCounter::Count(document,
                      UseCounter::kFullscreenAllowedByOrientationChange);
    return true;
  }

  String message = ExceptionMessages::FailedToExecute(
      "requestFullscreen", "Element",
      "API can only be initiated by a user gesture.");
  document.AddConsoleMessage(
      ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel, message));

  return false;
}

// https://fullscreen.spec.whatwg.org/#fullscreen-is-supported
bool FullscreenIsSupported(const Document& document) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return false;

  // Fullscreen is supported if there is no previously-established user
  // preference, security risk, or platform limitation.
  return !document.GetSettings() ||
         document.GetSettings()->GetFullscreenSupported();
}

// https://fullscreen.spec.whatwg.org/#fullscreen-element-ready-check
bool FullscreenElementReady(const Element& element) {
  // A fullscreen element ready check for an element |element| returns true if
  // all of the following are true, and false otherwise:

  // |element| is in a document.
  if (!element.isConnected())
    return false;

  // |element|'s node document is allowed to use the feature indicated by
  // attribute name allowfullscreen.
  if (!AllowedToUseFullscreen(element.GetDocument().GetFrame()))
    return false;

  // |element|'s node document's fullscreen element stack is either empty or its
  // top element is an inclusive ancestor of |element|.
  if (const Element* top_element =
          Fullscreen::FullscreenElementFrom(element.GetDocument())) {
    if (!top_element->contains(&element))
      return false;
  }

  // |element| has no ancestor element whose local name is iframe and namespace
  // is the HTML namespace.
  if (Traversal<HTMLIFrameElement>::FirstAncestor(element))
    return false;

  // |element|'s node document's browsing context either has a browsing context
  // container and the fullscreen element ready check returns true for
  // |element|'s node document's browsing context's browsing context container,
  // or it has no browsing context container.
  if (const Element* owner = element.GetDocument().LocalOwner()) {
    if (!FullscreenElementReady(*owner))
      return false;
  }

  return true;
}

bool IsPrefixed(const AtomicString& type) {
  return type == EventTypeNames::webkitfullscreenchange ||
         type == EventTypeNames::webkitfullscreenerror;
}

Event* CreateEvent(const AtomicString& type, EventTarget& target) {
  EventInit initializer;
  initializer.setBubbles(IsPrefixed(type));
  Event* event = Event::Create(type, initializer);
  event->SetTarget(&target);
  return event;
}

// Walks the frame tree and returns the first local ancestor frame, if any.
LocalFrame* NextLocalAncestor(Frame& frame) {
  Frame* parent = frame.Tree().Parent();
  if (!parent)
    return nullptr;
  if (parent->IsLocalFrame())
    return ToLocalFrame(parent);
  return NextLocalAncestor(*parent);
}

// Walks the document's frame tree and returns the document of the first local
// ancestor frame, if any.
Document* NextLocalAncestor(Document& document) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return nullptr;
  LocalFrame* next = NextLocalAncestor(*document.GetFrame());
  if (!next)
    return nullptr;
  DCHECK(next->GetDocument());
  return next->GetDocument();
}

// Helper to walk the ancestor chain and return the Document of the topmost
// local ancestor frame. Note that this is not the same as the topmost frame's
// Document, which might be unavailable in OOPIF scenarios. For example, with
// OOPIFs, when called on the bottom frame's Document in a A-B-C-B hierarchy in
// process B, this will skip remote frame C and return this frame: A-[B]-C-B.
Document& TopmostLocalAncestor(Document& document) {
  if (Document* next = NextLocalAncestor(document))
    return TopmostLocalAncestor(*next);
  return document;
}

// Helper to find the browsing context container in |doc| that embeds the
// |descendant| Document, possibly through multiple levels of nesting.  This
// works even in OOPIF scenarios like A-B-A, where there may be remote frames
// in between |doc| and |descendant|.
HTMLFrameOwnerElement* FindContainerForDescendant(const Document& doc,
                                                  const Document& descendant) {
  Frame* frame = descendant.GetFrame();
  while (frame->Tree().Parent() != doc.GetFrame())
    frame = frame->Tree().Parent();
  return ToHTMLFrameOwnerElement(frame->Owner());
}

// Fullscreen status affects scroll paint properties through
// FrameView::userInputScrollable().
void SetNeedsPaintPropertyUpdate(Document* document) {
  if (!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() ||
      RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    return;

  if (!document)
    return;

  LocalFrame* frame = document->GetFrame();
  if (!frame)
    return;

  if (FrameView* frame_view = frame->View())
    frame_view->SetNeedsPaintPropertyUpdate();
}

}  // anonymous namespace

const char* Fullscreen::SupplementName() {
  return "Fullscreen";
}

Fullscreen& Fullscreen::From(Document& document) {
  Fullscreen* fullscreen = FromIfExists(document);
  if (!fullscreen) {
    fullscreen = new Fullscreen(document);
    Supplement<Document>::ProvideTo(document, SupplementName(), fullscreen);
  }

  return *fullscreen;
}

Fullscreen* Fullscreen::FromIfExistsSlow(Document& document) {
  return static_cast<Fullscreen*>(
      Supplement<Document>::From(document, SupplementName()));
}

Element* Fullscreen::FullscreenElementFrom(Document& document) {
  if (Fullscreen* found = FromIfExists(document))
    return found->FullscreenElement();
  return nullptr;
}

Element* Fullscreen::FullscreenElementForBindingFrom(TreeScope& scope) {
  Element* element = FullscreenElementFrom(scope.GetDocument());
  if (!element || !RuntimeEnabledFeatures::fullscreenUnprefixedEnabled())
    return element;

  // TODO(kochi): Once V0 code is removed, we can use the same logic for
  // Document and ShadowRoot.
  if (!scope.RootNode().IsShadowRoot()) {
    // For Shadow DOM V0 compatibility: We allow returning an element in V0
    // shadow tree, even though it leaks the Shadow DOM.
    if (element->IsInV0ShadowTree()) {
      UseCounter::Count(scope.GetDocument(),
                        UseCounter::kDocumentFullscreenElementInV0Shadow);
      return element;
    }
  } else if (!ToShadowRoot(scope.RootNode()).IsV1()) {
    return nullptr;
  }
  return scope.AdjustedElement(*element);
}

Element* Fullscreen::CurrentFullScreenElementFrom(Document& document) {
  if (Fullscreen* found = FromIfExists(document))
    return found->CurrentFullScreenElement();
  return nullptr;
}

Element* Fullscreen::CurrentFullScreenElementForBindingFrom(
    Document& document) {
  Element* element = CurrentFullScreenElementFrom(document);
  if (!element || !RuntimeEnabledFeatures::fullscreenUnprefixedEnabled())
    return element;

  // For Shadow DOM V0 compatibility: We allow returning an element in V0 shadow
  // tree, even though it leaks the Shadow DOM.
  if (element->IsInV0ShadowTree()) {
    UseCounter::Count(document,
                      UseCounter::kDocumentFullscreenElementInV0Shadow);
    return element;
  }
  return document.AdjustedElement(*element);
}

Fullscreen::Fullscreen(Document& document)
    : Supplement<Document>(document),
      ContextLifecycleObserver(&document),
      full_screen_layout_object_(nullptr),
      event_queue_timer_(
          TaskRunnerHelper::Get(TaskType::kUnthrottled, &document),
          this,
          &Fullscreen::EventQueueTimerFired),
      for_cross_process_descendant_(false) {
  document.SetHasFullscreenSupplement();
}

Fullscreen::~Fullscreen() {}

inline Document* Fullscreen::GetDocument() {
  return ToDocument(LifecycleContext());
}

void Fullscreen::ContextDestroyed(ExecutionContext*) {
  event_queue_.clear();

  if (full_screen_layout_object_)
    full_screen_layout_object_->Destroy();

  current_full_screen_element_ = nullptr;
  fullscreen_element_stack_.clear();
}

// https://fullscreen.spec.whatwg.org/#dom-element-requestfullscreen
void Fullscreen::RequestFullscreen(Element& element) {
  // TODO(foolip): Make RequestType::Unprefixed the default when the unprefixed
  // API is enabled. https://crbug.com/383813
  RequestFullscreen(element, RequestType::kPrefixed, false);
}

void Fullscreen::RequestFullscreen(Element& element,
                                   RequestType request_type,
                                   bool for_cross_process_descendant) {
  Document& document = element.GetDocument();

  // Use counters only need to be incremented in the process of the actual
  // fullscreen element.
  if (!for_cross_process_descendant) {
    if (document.IsSecureContext()) {
      UseCounter::Count(document, UseCounter::kFullscreenSecureOrigin);
    } else {
      UseCounter::Count(document, UseCounter::kFullscreenInsecureOrigin);
      HostsUsingFeatures::CountAnyWorld(
          document, HostsUsingFeatures::Feature::kFullscreenInsecureHost);
    }
  }

  // Ignore this call if the document is not in a live frame.
  if (!document.IsActive() || !document.GetFrame())
    return;

  // If |element| is on top of |doc|'s fullscreen element stack, terminate these
  // substeps.
  if (&element == FullscreenElementFrom(document))
    return;

  do {
    // 1. If any of the following conditions are false, then terminate these
    // steps and queue a task to fire an event named fullscreenerror with its
    // bubbles attribute set to true on the context object's node document:

    // |element|'s namespace is the HTML namespace or |element| is an SVG
    // svg or MathML math element.
    // Note: MathML is not supported.
    if (!element.IsHTMLElement() && !isSVGSVGElement(element))
      break;

    // The fullscreen element ready check for |element| returns true.
    if (!FullscreenElementReady(element))
      break;

    // Fullscreen is supported.
    if (!FullscreenIsSupported(document))
      break;

    // This algorithm is allowed to request fullscreen.
    // OOPIF: If |forCrossProcessDescendant| is true, requestFullscreen was
    // already called on a descendant element in another process, and
    // getting here means that it was already allowed to request fullscreen.
    if (!for_cross_process_descendant && !AllowedToRequestFullscreen(document))
      break;

    // 2. Let doc be element's node document. (i.e. "this")

    // 3. Let docs be all doc's ancestor browsing context's documents (if any)
    // and doc.
    //
    // For OOPIF scenarios, |docs| will only contain documents for local
    // ancestors, and remote ancestors will be processed in their
    // respective processes.  This preserves the spec's event firing order
    // for local ancestors, but not for remote ancestors.  However, that
    // difference shouldn't be observable in practice: a fullscreenchange
    // event handler would need to postMessage a frame in another renderer
    // process, where the message should be queued up and processed after
    // the IPC that dispatches fullscreenchange.
    HeapDeque<Member<Document>> docs;
    for (Document* doc = &document; doc; doc = NextLocalAncestor(*doc))
      docs.push_front(doc);

    // 4. For each document in docs, run these substeps:
    HeapDeque<Member<Document>>::iterator current = docs.begin(),
                                          following = docs.begin();

    do {
      ++following;

      // 1. Let following document be the document after document in docs, or
      // null if there is no such document.
      Document* current_doc = *current;
      Document* following_doc = following != docs.end() ? *following : nullptr;

      // 2. If following document is null, push context object on document's
      // fullscreen element stack, and queue a task to fire an event named
      // fullscreenchange with its bubbles attribute set to true on the
      // document.
      if (!following_doc) {
        From(*current_doc).PushFullscreenElementStack(element, request_type);
        From(document).EnqueueChangeEvent(*current_doc, request_type);
        continue;
      }

      // 3. Otherwise, if document's fullscreen element stack is either empty or
      // its top element is not following document's browsing context container,
      Element* top_element = FullscreenElementFrom(*current_doc);
      HTMLFrameOwnerElement* following_owner =
          FindContainerForDescendant(*current_doc, *following_doc);
      if (!top_element || top_element != following_owner) {
        // ...push following document's browsing context container on document's
        // fullscreen element stack, and queue a task to fire an event named
        // fullscreenchange with its bubbles attribute set to true on document.
        From(*current_doc)
            .PushFullscreenElementStack(*following_owner, request_type);
        From(document).EnqueueChangeEvent(*current_doc, request_type);
        continue;
      }

      // 4. Otherwise, do nothing for this document. It stays the same.
    } while (++current != docs.end());

    From(document).for_cross_process_descendant_ = for_cross_process_descendant;

    // 5. Return, and run the remaining steps asynchronously.
    // 6. Optionally, perform some animation.
    if (From(document).pending_fullscreen_element_) {
      UseCounter::Count(document,
                        UseCounter::kFullscreenRequestWithPendingElement);
    }
    From(document).pending_fullscreen_element_ = &element;
    document.GetFrame()->GetChromeClient().EnterFullscreen(
        *document.GetFrame());

    // 7. Optionally, display a message indicating how the user can exit
    // displaying the context object fullscreen.
    return;
  } while (false);

  From(document).EnqueueErrorEvent(element, request_type);
}

// https://fullscreen.spec.whatwg.org/#fully-exit-fullscreen
void Fullscreen::FullyExitFullscreen(Document& document) {
  // To fully exit fullscreen, run these steps:

  // 1. Let |doc| be the top-level browsing context's document.
  //
  // Since the top-level browsing context's document might be unavailable in
  // OOPIF scenarios (i.e., when the top frame is remote), this actually uses
  // the Document of the topmost local ancestor frame.  Without OOPIF, this
  // will be the top frame's document.  With OOPIF, each renderer process for
  // the current page will separately call fullyExitFullscreen to cover all
  // local frames in each process.
  Document& doc = TopmostLocalAncestor(document);

  // 2. If |doc|'s fullscreen element stack is empty, terminate these steps.
  if (!FullscreenElementFrom(doc))
    return;

  // 3. Remove elements from |doc|'s fullscreen element stack until only the top
  // element is left.
  size_t stack_size = From(doc).fullscreen_element_stack_.size();
  From(doc).fullscreen_element_stack_.erase(0, stack_size - 1);
  DCHECK_EQ(From(doc).fullscreen_element_stack_.size(), 1u);

  // 4. Act as if the exitFullscreen() method was invoked on |doc|.
  ExitFullscreen(doc);
}

// https://fullscreen.spec.whatwg.org/#exit-fullscreen
void Fullscreen::ExitFullscreen(Document& document) {
  // The exitFullscreen() method must run these steps:

  // Ignore this call if the document is not in a live frame.
  if (!document.IsActive() || !document.GetFrame())
    return;

  // 1. Let doc be the context object. (i.e. "this")
  // 2. If doc's fullscreen element stack is empty, terminate these steps.
  if (!FullscreenElementFrom(document))
    return;

  // 3. Let descendants be all the doc's descendant browsing context's documents
  // with a non-empty fullscreen element stack (if any), ordered so that the
  // child of the doc is last and the document furthest away from the doc is
  // first.
  HeapDeque<Member<Document>> descendants;
  for (Frame* descendant = document.GetFrame()->Tree().TraverseNext();
       descendant; descendant = descendant->Tree().TraverseNext()) {
    if (!descendant->IsLocalFrame())
      continue;
    DCHECK(ToLocalFrame(descendant)->GetDocument());
    if (FullscreenElementFrom(*ToLocalFrame(descendant)->GetDocument()))
      descendants.push_front(ToLocalFrame(descendant)->GetDocument());
  }

  // 4. For each descendant in descendants, empty descendant's fullscreen
  // element stack, and queue a task to fire an event named fullscreenchange
  // with its bubbles attribute set to true on descendant.
  for (auto& descendant : descendants) {
    DCHECK(descendant);
    RequestType request_type =
        From(*descendant).fullscreen_element_stack_.back().second;
    From(*descendant).ClearFullscreenElementStack();
    From(document).EnqueueChangeEvent(*descendant, request_type);
  }

  // 5. While doc is not null, run these substeps:
  Element* new_top = nullptr;
  for (Document* current_doc = &document; current_doc;) {
    RequestType request_type =
        From(*current_doc).fullscreen_element_stack_.back().second;

    // 1. Pop the top element of doc's fullscreen element stack.
    From(*current_doc).PopFullscreenElementStack();

    //    If doc's fullscreen element stack is non-empty and the element now at
    //    the top is either not in a document or its node document is not doc,
    //    repeat this substep.
    new_top = FullscreenElementFrom(*current_doc);
    if (new_top &&
        (!new_top->isConnected() || new_top->GetDocument() != current_doc))
      continue;

    // 2. Queue a task to fire an event named fullscreenchange with its bubbles
    // attribute set to true on doc.
    From(document).EnqueueChangeEvent(*current_doc, request_type);

    // 3. If doc's fullscreen element stack is empty and doc's browsing context
    // has a browsing context container, set doc to that browsing context
    // container's node document.
    //
    // OOPIF: If browsing context container's document is in another
    // process, keep moving up the ancestor chain and looking for a
    // browsing context container with a local document.
    // TODO(alexmos): Deal with nested fullscreen cases, see
    // https://crbug.com/617369.
    if (!new_top) {
      current_doc = NextLocalAncestor(*current_doc);
      continue;
    }

    // 4. Otherwise, set doc to null.
    current_doc = nullptr;
  }

  // 6. Return, and run the remaining steps asynchronously.
  // 7. Optionally, perform some animation.

  // Only exit fullscreen mode if the fullscreen element stack is empty.
  if (!new_top) {
    document.GetFrame()->GetChromeClient().ExitFullscreen(*document.GetFrame());
    return;
  }

  // Otherwise, enter fullscreen for the fullscreen element stack's top element.
  From(document).pending_fullscreen_element_ = new_top;
  From(document).DidEnterFullscreen();
}

// https://fullscreen.spec.whatwg.org/#dom-document-fullscreenenabled
bool Fullscreen::FullscreenEnabled(Document& document) {
  // The fullscreenEnabled attribute's getter must return true if the context
  // object is allowed to use the feature indicated by attribute name
  // allowfullscreen and fullscreen is supported, and false otherwise.
  return AllowedToUseFullscreen(document.GetFrame()) &&
         FullscreenIsSupported(document);
}

void Fullscreen::DidEnterFullscreen() {
  if (!GetDocument()->IsActive() || !GetDocument()->GetFrame())
    return;

  // Start the timer for events enqueued by |requestFullscreen()|. The hover
  // state update is scheduled first so that it's done when the events fire.
  GetDocument()->GetFrame()->GetEventHandler().ScheduleHoverStateUpdate();
  event_queue_timer_.StartOneShot(0, BLINK_FROM_HERE);

  Element* element = pending_fullscreen_element_.Release();
  if (!element)
    return;

  if (current_full_screen_element_ == element)
    return;

  if (!element->isConnected() || &element->GetDocument() != GetDocument()) {
    // The element was removed or has moved to another document since the
    // |requestFullscreen()| call. Exit fullscreen again to recover.
    // TODO(foolip): Fire a fullscreenerror event. This is currently difficult
    // because the fullscreenchange event has already been enqueued and possibly
    // even fired. https://crbug.com/402376
    LocalFrame& frame = *GetDocument()->GetFrame();
    frame.GetChromeClient().ExitFullscreen(frame);
    return;
  }

  if (full_screen_layout_object_)
    full_screen_layout_object_->UnwrapLayoutObject();

  Element* previous_element = current_full_screen_element_;
  current_full_screen_element_ = element;

  // Create a placeholder block for a the full-screen element, to keep the page
  // from reflowing when the element is removed from the normal flow. Only do
  // this for a LayoutBox, as only a box will have a frameRect. The placeholder
  // will be created in setFullScreenLayoutObject() during layout.
  LayoutObject* layout_object = current_full_screen_element_->GetLayoutObject();
  bool should_create_placeholder = layout_object && layout_object->IsBox();
  if (should_create_placeholder) {
    saved_placeholder_frame_rect_ = ToLayoutBox(layout_object)->FrameRect();
    saved_placeholder_computed_style_ =
        ComputedStyle::Clone(layout_object->StyleRef());
  }

  // TODO(alexmos): When |m_forCrossProcessDescendant| is true, some of
  // this layout work has already been done in another process, so it should
  // not be necessary to repeat it here.
  if (current_full_screen_element_ != GetDocument()->documentElement()) {
    LayoutFullScreen::WrapLayoutObject(
        layout_object, layout_object ? layout_object->Parent() : 0,
        GetDocument());
  }

  // When |m_forCrossProcessDescendant| is true, m_currentFullScreenElement
  // corresponds to the HTMLFrameOwnerElement for the out-of-process iframe
  // that contains the actual fullscreen element.   Hence, it must also set
  // the ContainsFullScreenElement flag (so that it gains the
  // -webkit-full-screen-ancestor style).
  if (for_cross_process_descendant_) {
    DCHECK(current_full_screen_element_->IsFrameOwnerElement());
    DCHECK(ToHTMLFrameOwnerElement(current_full_screen_element_)
               ->ContentFrame()
               ->IsRemoteFrame());
    current_full_screen_element_->SetContainsFullScreenElement(true);
  }

  current_full_screen_element_
      ->SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);

  GetDocument()->GetStyleEngine().EnsureUAStyleForFullscreen();
  current_full_screen_element_->PseudoStateChanged(
      CSSSelector::kPseudoFullScreen);

  // FIXME: This should not call updateStyleAndLayoutTree.
  GetDocument()->UpdateStyleAndLayoutTree();

  GetDocument()->GetFrame()->GetChromeClient().FullscreenElementChanged(
      previous_element, element);
}

void Fullscreen::DidExitFullscreen() {
  if (!GetDocument()->IsActive() || !GetDocument()->GetFrame())
    return;

  // Start the timer for events enqueued by |exitFullscreen()|. The hover state
  // update is scheduled first so that it's done when the events fire.
  GetDocument()->GetFrame()->GetEventHandler().ScheduleHoverStateUpdate();
  event_queue_timer_.StartOneShot(0, BLINK_FROM_HERE);

  // If fullscreen was canceled by the browser, e.g. if the user pressed Esc,
  // then |exitFullscreen()| was never called. Let |fullyExitFullscreen()| clear
  // the fullscreen element stack and fire any events as necessary.
  // TODO(foolip): Remove this when state changes and events are synchronized
  // with animation frames. https://crbug.com/402376
  FullyExitFullscreen(*GetDocument());

  if (!current_full_screen_element_)
    return;

  if (for_cross_process_descendant_)
    current_full_screen_element_->SetContainsFullScreenElement(false);

  current_full_screen_element_
      ->SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);

  if (full_screen_layout_object_)
    LayoutFullScreenItem(full_screen_layout_object_).UnwrapLayoutObject();

  GetDocument()->GetStyleEngine().EnsureUAStyleForFullscreen();
  current_full_screen_element_->PseudoStateChanged(
      CSSSelector::kPseudoFullScreen);
  Element* previous_element = current_full_screen_element_;
  current_full_screen_element_ = nullptr;

  for_cross_process_descendant_ = false;

  GetDocument()->GetFrame()->GetChromeClient().FullscreenElementChanged(
      previous_element, nullptr);
}

void Fullscreen::SetFullScreenLayoutObject(LayoutFullScreen* layout_object) {
  if (layout_object == full_screen_layout_object_)
    return;

  if (layout_object && saved_placeholder_computed_style_) {
    layout_object->CreatePlaceholder(
        std::move(saved_placeholder_computed_style_),
        saved_placeholder_frame_rect_);
  } else if (layout_object && full_screen_layout_object_ &&
             full_screen_layout_object_->Placeholder()) {
    LayoutBlockFlow* placeholder = full_screen_layout_object_->Placeholder();
    layout_object->CreatePlaceholder(
        ComputedStyle::Clone(placeholder->StyleRef()),
        placeholder->FrameRect());
  }

  if (full_screen_layout_object_)
    full_screen_layout_object_->UnwrapLayoutObject();
  DCHECK(!full_screen_layout_object_);

  full_screen_layout_object_ = layout_object;
}

void Fullscreen::FullScreenLayoutObjectDestroyed() {
  full_screen_layout_object_ = nullptr;
}

void Fullscreen::EnqueueChangeEvent(Document& document,
                                    RequestType request_type) {
  Event* event;
  if (request_type == RequestType::kUnprefixed) {
    event = CreateEvent(EventTypeNames::fullscreenchange, document);
  } else {
    DCHECK(document.HasFullscreenSupplement());
    Fullscreen& fullscreen = From(document);
    EventTarget* target = fullscreen.FullscreenElement();
    if (!target)
      target = fullscreen.CurrentFullScreenElement();
    if (!target)
      target = &document;
    event = CreateEvent(EventTypeNames::webkitfullscreenchange, *target);
  }
  event_queue_.push_back(event);
  // NOTE: The timer is started in didEnterFullscreen/didExitFullscreen.
}

void Fullscreen::EnqueueErrorEvent(Element& element, RequestType request_type) {
  Event* event;
  if (request_type == RequestType::kUnprefixed)
    event = CreateEvent(EventTypeNames::fullscreenerror, element.GetDocument());
  else
    event = CreateEvent(EventTypeNames::webkitfullscreenerror, element);
  event_queue_.push_back(event);
  event_queue_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void Fullscreen::EventQueueTimerFired(TimerBase*) {
  HeapDeque<Member<Event>> event_queue;
  event_queue_.Swap(event_queue);

  while (!event_queue.IsEmpty()) {
    Event* event = event_queue.TakeFirst();
    Node* target = event->target()->ToNode();

    // If the element was removed from our tree, also message the
    // documentElement.
    if (!target->isConnected() && GetDocument()->documentElement()) {
      DCHECK(IsPrefixed(event->type()));
      event_queue.push_back(
          CreateEvent(event->type(), *GetDocument()->documentElement()));
    }

    target->DispatchEvent(event);
  }
}

void Fullscreen::ElementRemoved(Element& old_node) {
  // Whenever the removing steps run with an |oldNode| and |oldNode| is in its
  // node document's fullscreen element stack, run these steps:

  // 1. If |oldNode| is at the top of its node document's fullscreen element
  // stack, act as if the exitFullscreen() method was invoked on that document.
  if (FullscreenElement() == &old_node) {
    ExitFullscreen(old_node.GetDocument());
    return;
  }

  // 2. Otherwise, remove |oldNode| from its node document's fullscreen element
  // stack.
  for (size_t i = 0; i < fullscreen_element_stack_.size(); ++i) {
    if (fullscreen_element_stack_[i].first.Get() == &old_node) {
      fullscreen_element_stack_.erase(i);
      return;
    }
  }

  // NOTE: |oldNode| was not in the fullscreen element stack.
}

void Fullscreen::ClearFullscreenElementStack() {
  if (fullscreen_element_stack_.IsEmpty())
    return;

  fullscreen_element_stack_.clear();

  SetNeedsPaintPropertyUpdate(GetDocument());
}

void Fullscreen::PopFullscreenElementStack() {
  if (fullscreen_element_stack_.IsEmpty())
    return;

  fullscreen_element_stack_.pop_back();

  SetNeedsPaintPropertyUpdate(GetDocument());
}

void Fullscreen::PushFullscreenElementStack(Element& element,
                                            RequestType request_type) {
  fullscreen_element_stack_.push_back(std::make_pair(&element, request_type));

  SetNeedsPaintPropertyUpdate(GetDocument());
}

DEFINE_TRACE(Fullscreen) {
  visitor->Trace(pending_fullscreen_element_);
  visitor->Trace(fullscreen_element_stack_);
  visitor->Trace(current_full_screen_element_);
  visitor->Trace(event_queue_);
  Supplement<Document>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
