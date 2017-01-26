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

#include "bindings/core/v8/ConditionalFeatures.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleEngine.h"
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

namespace blink {

namespace {

// https://html.spec.whatwg.org/multipage/embedded-content.html#allowed-to-use
bool allowedToUseFullscreen(const Frame* frame) {
  // To determine whether a Document object |document| is allowed to use the
  // feature indicated by attribute name |allowattribute|, run these steps:

  // 1. If |document| has no browsing context, then return false.
  if (!frame)
    return false;

  if (!RuntimeEnabledFeatures::featurePolicyEnabled()) {
    // 2. If |document|'s browsing context is a top-level browsing context, then
    // return true.
    if (frame->isMainFrame())
      return true;

    // 3. If |document|'s browsing context has a browsing context container that
    // is an iframe element with an |allowattribute| attribute specified, and
    // whose node document is allowed to use the feature indicated by
    // |allowattribute|, then return true.
    if (frame->owner() && frame->owner()->allowFullscreen())
      return allowedToUseFullscreen(frame->tree().parent());

    // 4. Return false.
    return false;
  }

  // If Feature Policy is enabled, then we need this hack to support it, until
  // we have proper support for <iframe allowfullscreen> in FP:

  // 1. If FP, by itself, enables fullscreen in this document, then fullscreen
  // is allowed.
  if (frame->securityContext()->getFeaturePolicy()->isFeatureEnabled(
          kFullscreenFeature)) {
    return true;
  }

  // 2. Otherwise, if the embedding frame's document is allowed to use
  // fullscreen (either through FP or otherwise), and either:
  //   a) this is a same-origin embedded document, or
  //   b) this document's iframe has the allowfullscreen attribute set,
  // then fullscreen is allowed.
  if (!frame->isMainFrame()) {
    if (allowedToUseFullscreen(frame->tree().parent())) {
      return (frame->owner() && frame->owner()->allowFullscreen()) ||
             frame->tree()
                 .parent()
                 ->securityContext()
                 ->getSecurityOrigin()
                 ->isSameSchemeHostPortAndSuborigin(
                     frame->securityContext()->getSecurityOrigin());
    }
  }

  // Otherwise, fullscreen is not allowed. (If we reach here and this is the
  // main frame, then fullscreen must have been disabled by FP.)
  return false;
}

bool allowedToRequestFullscreen(Document& document) {
  // An algorithm is allowed to request fullscreen if one of the following is
  // true:

  //  The algorithm is triggered by a user activation.
  if (UserGestureIndicator::utilizeUserGesture())
    return true;

  //  The algorithm is triggered by a user generated orientation change.
  if (ScopedOrientationChangeIndicator::processingOrientationChange()) {
    UseCounter::count(document,
                      UseCounter::FullscreenAllowedByOrientationChange);
    return true;
  }

  String message = ExceptionMessages::failedToExecute(
      "requestFullscreen", "Element",
      "API can only be initiated by a user gesture.");
  document.addConsoleMessage(
      ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));

  return false;
}

// https://fullscreen.spec.whatwg.org/#fullscreen-is-supported
bool fullscreenIsSupported(const Document& document) {
  LocalFrame* frame = document.frame();
  if (!frame)
    return false;

  // Fullscreen is supported if there is no previously-established user
  // preference, security risk, or platform limitation.
  return !document.settings() || document.settings()->getFullscreenSupported();
}

// https://fullscreen.spec.whatwg.org/#fullscreen-element-ready-check
bool fullscreenElementReady(const Element& element) {
  // A fullscreen element ready check for an element |element| returns true if
  // all of the following are true, and false otherwise:

  // |element| is in a document.
  if (!element.isConnected())
    return false;

  // |element|'s node document is allowed to use the feature indicated by
  // attribute name allowfullscreen.
  if (!allowedToUseFullscreen(element.document().frame()))
    return false;

  // |element|'s node document's fullscreen element stack is either empty or its
  // top element is an inclusive ancestor of |element|.
  if (const Element* topElement =
          Fullscreen::fullscreenElementFrom(element.document())) {
    if (!topElement->contains(&element))
      return false;
  }

  // |element| has no ancestor element whose local name is iframe and namespace
  // is the HTML namespace.
  if (Traversal<HTMLIFrameElement>::firstAncestor(element))
    return false;

  // |element|'s node document's browsing context either has a browsing context
  // container and the fullscreen element ready check returns true for
  // |element|'s node document's browsing context's browsing context container,
  // or it has no browsing context container.
  if (const Element* owner = element.document().localOwner()) {
    if (!fullscreenElementReady(*owner))
      return false;
  }

  return true;
}

// https://fullscreen.spec.whatwg.org/#dom-element-requestfullscreen step 4:
bool requestFullscreenConditionsMet(Element& pending, Document& document) {
  // |pending|'s namespace is the HTML namespace or |pending| is an SVG svg or
  // MathML math element. Note: MathML is not supported.
  if (!pending.isHTMLElement() && !isSVGSVGElement(pending))
    return false;

  // The fullscreen element ready check for |pending| returns false.
  if (!fullscreenElementReady(pending))
    return false;

  // Fullscreen is supported.
  if (!fullscreenIsSupported(document))
    return false;

  // This algorithm is allowed to request fullscreen.
  if (!allowedToRequestFullscreen(document))
    return false;

  return true;
}

// Walks the frame tree and returns the first local ancestor frame, if any.
LocalFrame* nextLocalAncestor(Frame& frame) {
  Frame* parent = frame.tree().parent();
  if (!parent)
    return nullptr;
  if (parent->isLocalFrame())
    return toLocalFrame(parent);
  return nextLocalAncestor(*parent);
}

// Walks the document's frame tree and returns the document of the first local
// ancestor frame, if any.
Document* nextLocalAncestor(Document& document) {
  LocalFrame* frame = document.frame();
  if (!frame)
    return nullptr;
  LocalFrame* next = nextLocalAncestor(*frame);
  if (!next)
    return nullptr;
  DCHECK(next->document());
  return next->document();
}

// Helper to walk the ancestor chain and return the Document of the topmost
// local ancestor frame. Note that this is not the same as the topmost frame's
// Document, which might be unavailable in OOPIF scenarios. For example, with
// OOPIFs, when called on the bottom frame's Document in a A-B-C-B hierarchy in
// process B, this will skip remote frame C and return this frame: A-[B]-C-B.
Document& topmostLocalAncestor(Document& document) {
  if (Document* next = nextLocalAncestor(document))
    return topmostLocalAncestor(*next);
  return document;
}

// https://fullscreen.spec.whatwg.org/#collect-documents-to-unfullscreen
HeapVector<Member<Document>> collectDocumentsToUnfullscreen(
    Document& doc,
    Fullscreen::ExitType exitType) {
  DCHECK(Fullscreen::fullscreenElementFrom(doc));

  // 1. If |doc|'s top layer consists of more than a single element that has
  // its fullscreen flag set, return the empty set.
  // TODO(foolip): See TODO in |fullyExitFullscreen()|.
  if (exitType != Fullscreen::ExitType::Fully &&
      Fullscreen::fullscreenElementStackSizeFrom(doc) > 1)
    return HeapVector<Member<Document>>();

  // 2. Let |docs| be an ordered set consisting of |doc|.
  HeapVector<Member<Document>> docs;
  docs.push_back(&doc);

  // 3. While |docs|'s last document ...
  //
  // OOPIF: Skip over remote frames, assuming that they have exactly one element
  // in their fullscreen element stacks, thereby erring on the side of exiting
  // fullscreen. TODO(alexmos): Deal with nested fullscreen cases, see
  // https://crbug.com/617369.
  for (Document* document = nextLocalAncestor(doc); document;
       document = nextLocalAncestor(*document)) {
    // ... has a browsing context container whose node document's top layer
    // consists of a single element that has its fullscreen flag set and does
    // not have its iframe fullscreen flag set (if any), append that node
    // document to |docs|.
    // TODO(foolip): Support the iframe fullscreen flag.
    // https://crbug.com/644695
    if (Fullscreen::fullscreenElementStackSizeFrom(*document) == 1)
      docs.push_back(document);
    else
      break;
  }

  // 4. Return |docs|.
  return docs;
}

// Creates a non-bubbling event with |document| as its target.
Event* createEvent(const AtomicString& type, Document& document) {
  DCHECK(type == EventTypeNames::fullscreenchange ||
         type == EventTypeNames::fullscreenerror);

  Event* event = Event::create(type);
  event->setTarget(&document);
  return event;
}

// Creates a bubbling event with |element| as its target. If |element| is not
// connected to |document|, then |document| is used as the target instead.
Event* createPrefixedEvent(const AtomicString& type,
                           Element& element,
                           Document& document) {
  DCHECK(type == EventTypeNames::webkitfullscreenchange ||
         type == EventTypeNames::webkitfullscreenerror);

  Event* event = Event::createBubble(type);
  if (element.isConnected() && element.document() == document)
    event->setTarget(&element);
  else
    event->setTarget(&document);
  return event;
}

Event* createChangeEvent(Document& document,
                         Element& element,
                         Fullscreen::RequestType requestType) {
  if (requestType == Fullscreen::RequestType::Unprefixed)
    return createEvent(EventTypeNames::fullscreenchange, document);
  return createPrefixedEvent(EventTypeNames::webkitfullscreenchange, element,
                             document);
}

Event* createErrorEvent(Document& document,
                        Element& element,
                        Fullscreen::RequestType requestType) {
  if (requestType == Fullscreen::RequestType::Unprefixed)
    return createEvent(EventTypeNames::fullscreenerror, document);
  return createPrefixedEvent(EventTypeNames::webkitfullscreenerror, element,
                             document);
}

void dispatchEvents(const HeapVector<Member<Event>>& events) {
  for (Event* event : events)
    event->target()->dispatchEvent(event);
}

}  // anonymous namespace

const char* Fullscreen::supplementName() {
  return "Fullscreen";
}

Fullscreen& Fullscreen::from(Document& document) {
  Fullscreen* fullscreen = fromIfExists(document);
  if (!fullscreen) {
    fullscreen = new Fullscreen(document);
    Supplement<Document>::provideTo(document, supplementName(), fullscreen);
  }

  return *fullscreen;
}

Fullscreen* Fullscreen::fromIfExistsSlow(Document& document) {
  return static_cast<Fullscreen*>(
      Supplement<Document>::from(document, supplementName()));
}

Element* Fullscreen::fullscreenElementFrom(Document& document) {
  if (Fullscreen* found = fromIfExists(document))
    return found->fullscreenElement();
  return nullptr;
}

size_t Fullscreen::fullscreenElementStackSizeFrom(Document& document) {
  if (Fullscreen* found = fromIfExists(document))
    return found->m_fullscreenElementStack.size();
  return 0;
}

Element* Fullscreen::fullscreenElementForBindingFrom(TreeScope& scope) {
  Element* element = fullscreenElementFrom(scope.document());
  if (!element || !RuntimeEnabledFeatures::fullscreenUnprefixedEnabled())
    return element;

  // TODO(kochi): Once V0 code is removed, we can use the same logic for
  // Document and ShadowRoot.
  if (!scope.rootNode().isShadowRoot()) {
    // For Shadow DOM V0 compatibility: We allow returning an element in V0
    // shadow tree, even though it leaks the Shadow DOM.
    if (element->isInV0ShadowTree()) {
      UseCounter::count(scope.document(),
                        UseCounter::DocumentFullscreenElementInV0Shadow);
      return element;
    }
  } else if (!toShadowRoot(scope.rootNode()).isV1()) {
    return nullptr;
  }
  return scope.adjustedElement(*element);
}

Fullscreen::Fullscreen(Document& document)
    : Supplement<Document>(document),
      ContextLifecycleObserver(&document),
      m_fullScreenLayoutObject(nullptr) {
  document.setHasFullscreenSupplement();
}

Fullscreen::~Fullscreen() {}

Document* Fullscreen::document() {
  return toDocument(lifecycleContext());
}

void Fullscreen::contextDestroyed(ExecutionContext*) {
  if (m_fullScreenLayoutObject)
    m_fullScreenLayoutObject->destroy();

  m_pendingRequests.clear();
  m_fullscreenElementStack.clear();
}

// https://fullscreen.spec.whatwg.org/#dom-element-requestfullscreen
void Fullscreen::requestFullscreen(Element& pending) {
  // TODO(foolip): Make RequestType::Unprefixed the default when the unprefixed
  // API is enabled. https://crbug.com/383813
  requestFullscreen(pending, RequestType::Prefixed);
}

void Fullscreen::requestFullscreen(Element& pending, RequestType requestType) {
  Document& document = pending.document();

  // Ignore this call if the document is not in a live frame.
  if (!document.isActive() || !document.frame())
    return;

  bool forCrossProcessDescendant =
      requestType == RequestType::PrefixedForCrossProcessDescendant;

  // Use counters only need to be incremented in the process of the actual
  // fullscreen element.
  if (!forCrossProcessDescendant) {
    if (document.isSecureContext()) {
      UseCounter::count(document, UseCounter::FullscreenSecureOrigin);
    } else {
      UseCounter::count(document, UseCounter::FullscreenInsecureOrigin);
      HostsUsingFeatures::countAnyWorld(
          document, HostsUsingFeatures::Feature::FullscreenInsecureHost);
    }
  }

  // 1. Let |pending| be the context object.

  // 2. Let |error| be false.
  bool error = false;

  // 3. Let |promise| be a new promise.
  // TODO(foolip): Promises. https://crbug.com/644637

  // 4. If any of the following conditions are false, then set |error| to true:
  //
  // OOPIF: If |requestFullscreen()| was already called in a descendant frame
  // and passed the checks, do not check again here.
  if (!forCrossProcessDescendant &&
      !requestFullscreenConditionsMet(pending, document))
    error = true;

  // 5. Return |promise|, and run the remaining steps in parallel.
  // TODO(foolip): Promises. https://crbug.com/644637

  // 6. If |error| is false: Resize pending's top-level browsing context's
  // document's viewport's dimensions to match the dimensions of the screen of
  // the output device. Optionally display a message how the end user can
  // revert this.
  if (!error) {
    from(document).m_pendingRequests.push_back(
        std::make_pair(&pending, requestType));
    LocalFrame& frame = *document.frame();
    frame.chromeClient().enterFullscreen(frame);
  } else {
    enqueueTaskForRequest(document, pending, requestType, true);
  }
}

void Fullscreen::didEnterFullscreen() {
  if (!document())
    return;

  ElementStack requests;
  requests.swap(m_pendingRequests);
  for (const ElementStackEntry& request : requests)
    enqueueTaskForRequest(*document(), *request.first, request.second, false);
}

void Fullscreen::enqueueTaskForRequest(Document& document,
                                       Element& pending,
                                       RequestType requestType,
                                       bool error) {
  // 7. As part of the next animation frame task, run these substeps:
  document.enqueueAnimationFrameTask(
      WTF::bind(&runTaskForRequest, wrapPersistent(&document),
                wrapPersistent(&pending), requestType, error));
}

void Fullscreen::runTaskForRequest(Document* document,
                                   Element* element,
                                   RequestType requestType,
                                   bool error) {
  DCHECK(document);
  DCHECK(document->isActive());
  DCHECK(document->frame());
  DCHECK(element);

  Document& pendingDoc = *document;
  Element& pending = *element;

  // TODO(foolip): Spec something like: If |pending|'s node document is not
  // |pendingDoc|, then set |error| to true.
  // https://github.com/whatwg/fullscreen/issues/33
  if (pending.document() != pendingDoc)
    error = true;

  // 7.1. If either |error| is true or the fullscreen element ready check for
  // |pending| returns false, fire an event named fullscreenerror on
  // |pending|'s node document, reject |promise| with a TypeError exception,
  // and terminate these steps.
  // TODO(foolip): Promises. https://crbug.com/644637
  if (error || !fullscreenElementReady(pending)) {
    Event* event = createErrorEvent(pendingDoc, pending, requestType);
    event->target()->dispatchEvent(event);
    return;
  }

  // 7.2. Let |fullscreenElements| be an ordered set initially consisting of
  // |pending|.
  HeapDeque<Member<Element>> fullscreenElements;
  fullscreenElements.append(pending);

  // 7.3. While the first element in |fullscreenElements| is in a nested
  // browsing context, prepend its browsing context container to
  // |fullscreenElements|.
  //
  // OOPIF: |fullscreenElements| will only contain elements for local ancestors,
  // and remote ancestors will be processed in their respective processes. This
  // preserves the spec's event firing order for local ancestors, but not for
  // remote ancestors. However, that difference shouldn't be observable in
  // practice: a fullscreenchange event handler would need to postMessage a
  // frame in another renderer process, where the message should be queued up
  // and processed after the IPC that dispatches fullscreenchange.
  for (Frame* frame = pending.document().frame(); frame;
       frame = frame->tree().parent()) {
    if (!frame->owner() || !frame->owner()->isLocal())
      continue;
    Element* element = toHTMLFrameOwnerElement(frame->owner());
    fullscreenElements.prepend(element);
  }

  // 7.4. Let |eventDocs| be an empty list.
  // Note: For prefixed requests, the event target is an element, so instead
  // let |events| be a list of events to dispatch.
  HeapVector<Member<Event>> events;

  // 7.5. For each |element| in |fullscreenElements|, in order, run these
  // subsubsteps:
  for (Element* element : fullscreenElements) {
    // 7.5.1. Let |doc| be |element|'s node document.
    Document& doc = element->document();

    // 7.5.2. If |element| is |doc|'s fullscreen element, terminate these
    // subsubsteps.
    if (element == fullscreenElementFrom(doc))
      continue;

    // 7.5.3. Otherwise, append |doc| to |eventDocs|.
    events.push_back(createChangeEvent(doc, *element, requestType));

    // 7.5.4. If |element| is |pending| and |pending| is an iframe element,
    // set |element|'s iframe fullscreen flag.
    // TODO(foolip): Support the iframe fullscreen flag.
    // https://crbug.com/644695

    // 7.5.5. Fullscreen |element| within |doc|.
    // TODO(foolip): Merge fullscreen element stack into top layer.
    // https://crbug.com/627790
    from(doc).pushFullscreenElementStack(*element, requestType);
  }

  // 7.6. For each |doc| in |eventDocs|, in order, fire an event named
  // fullscreenchange on |doc|.
  dispatchEvents(events);

  // 7.7. Fulfill |promise| with undefined.
  // TODO(foolip): Promises. https://crbug.com/644637
}

// https://fullscreen.spec.whatwg.org/#fully-exit-fullscreen
void Fullscreen::fullyExitFullscreen(Document& document) {
  // 1. If |document|'s fullscreen element is null, terminate these steps.

  // 2. Unfullscreen elements whose fullscreen flag is set, within
  // |document|'s top layer, except for |document|'s fullscreen element.

  // 3. Exit fullscreen |document|.

  // TODO(foolip): Change the spec. To remove elements from |document|'s top
  // layer as in step 2 could leave descendant frames in fullscreen. It may work
  // to give the "exit fullscreen" algorithm a |fully| flag that's used in the
  // animation frame task after exit. Here, retain the old behavior of fully
  // exiting fullscreen for the topmost local ancestor:
  exitFullscreen(topmostLocalAncestor(document), ExitType::Fully);
}

// https://fullscreen.spec.whatwg.org/#exit-fullscreen
void Fullscreen::exitFullscreen(Document& doc, ExitType exitType) {
  if (!doc.isActive() || !doc.frame())
    return;

  // 1. Let |promise| be a new promise.
  // 2. If |doc|'s fullscreen element is null, reject |promise| with a
  // TypeError exception, and return |promise|.
  // TODO(foolip): Promises. https://crbug.com/644637
  if (!fullscreenElementFrom(doc))
    return;

  // 3. Let |resize| be false.
  bool resize = false;

  // 4. Let |docs| be the result of collecting documents to unfullscreen given
  // |doc|.
  HeapVector<Member<Document>> docs =
      collectDocumentsToUnfullscreen(doc, exitType);

  // 5. Let |topLevelDoc| be |doc|'s top-level browsing context's document.
  //
  // OOPIF: Let |topLevelDoc| be the topmost local ancestor instead. If the main
  // frame is in another process, we will still fully exit fullscreen even
  // though that's wrong if the main frame was in nested fullscreen.
  // TODO(alexmos): Deal with nested fullscreen cases, see
  // https://crbug.com/617369.
  Document& topLevelDoc = topmostLocalAncestor(doc);

  // 6. If |topLevelDoc| is in |docs|, set |resize| to true.
  if (!docs.isEmpty() && docs.back() == &topLevelDoc)
    resize = true;

  // 7. Return |promise|, and run the remaining steps in parallel.
  // TODO(foolip): Promises. https://crbug.com/644637

  // Note: |ExitType::Fully| is only used together with the topmost local
  // ancestor in |fullyExitFullscreen()|, and so implies that |resize| is true.
  // This would change if matching the spec for "fully exit fullscreen".
  if (exitType == ExitType::Fully)
    DCHECK(resize);

  // 8. If |resize| is true, resize |topLevelDoc|'s viewport to its "normal"
  // dimensions.
  if (resize) {
    LocalFrame& frame = *doc.frame();
    frame.chromeClient().exitFullscreen(frame);
  } else {
    enqueueTaskForExit(doc, exitType);
  }
}

void Fullscreen::didExitFullscreen() {
  if (!document())
    return;

  DCHECK_EQ(document(), &topmostLocalAncestor(*document()));

  enqueueTaskForExit(*document(), ExitType::Fully);
}

void Fullscreen::enqueueTaskForExit(Document& document, ExitType exitType) {
  // 9. As part of the next animation frame task, run these substeps:
  document.enqueueAnimationFrameTask(
      WTF::bind(&runTaskForExit, wrapPersistent(&document), exitType));
}

void Fullscreen::runTaskForExit(Document* document, ExitType exitType) {
  DCHECK(document);
  DCHECK(document->isActive());
  DCHECK(document->frame());

  Document& doc = *document;

  if (!fullscreenElementFrom(doc))
    return;

  // 9.1. Let |exitDocs| be the result of collecting documents to unfullscreen
  // given |doc|.

  // 9.2. If |resize| is true and |topLevelDoc| is not in |exitDocs|, fully
  // exit fullscreen |topLevelDoc|, reject promise with a TypeError exception,
  // and terminate these steps.

  // TODO(foolip): See TODO in |fullyExitFullscreen()|. Instead of using "fully
  // exit fullscreen" in step 9.2 (which is async), give "exit fullscreen" a
  // |fully| flag which is always true if |resize| was true.

  HeapVector<Member<Document>> exitDocs =
      collectDocumentsToUnfullscreen(doc, exitType);

  // 9.3. If |exitDocs| is the empty set, append |doc| to |exitDocs|.
  if (exitDocs.isEmpty())
    exitDocs.push_back(&doc);

  // 9.4. If |exitDocs|'s last document has a browsing context container,
  // append that browsing context container's node document to |exitDocs|.
  //
  // OOPIF: Skip over remote frames, assuming that they have exactly one element
  // in their fullscreen element stacks, thereby erring on the side of exiting
  // fullscreen. TODO(alexmos): Deal with nested fullscreen cases, see
  // https://crbug.com/617369.
  if (Document* document = nextLocalAncestor(*exitDocs.back()))
    exitDocs.push_back(document);

  // 9.5. Let |descendantDocs| be an ordered set consisting of |doc|'s
  // descendant browsing contexts' documents whose fullscreen element is
  // non-null, if any, in *reverse* tree order.
  HeapDeque<Member<Document>> descendantDocs;
  for (Frame* descendant = doc.frame()->tree().firstChild(); descendant;
       descendant = descendant->tree().traverseNext(doc.frame())) {
    if (!descendant->isLocalFrame())
      continue;
    DCHECK(toLocalFrame(descendant)->document());
    if (fullscreenElementFrom(*toLocalFrame(descendant)->document()))
      descendantDocs.prepend(toLocalFrame(descendant)->document());
  }

  // Note: For prefixed requests, the event target is an element, so let
  // |events| be a list of events to dispatch.
  HeapVector<Member<Event>> events;

  // 9.6. For each |descendantDoc| in |descendantDocs|, in order, unfullscreen
  // |descendantDoc|.
  for (Document* descendantDoc : descendantDocs) {
    Fullscreen& fullscreen = from(*descendantDoc);
    ElementStack& stack = fullscreen.m_fullscreenElementStack;
    DCHECK(!stack.isEmpty());
    events.push_back(createChangeEvent(*descendantDoc, *stack.back().first,
                                       stack.back().second));
    while (!stack.isEmpty())
      fullscreen.popFullscreenElementStack();
  }

  // 9.7. For each |exitDoc| in |exitDocs|, in order, unfullscreen |exitDoc|'s
  // fullscreen element.
  for (Document* exitDoc : exitDocs) {
    Fullscreen& fullscreen = from(*exitDoc);
    ElementStack& stack = fullscreen.m_fullscreenElementStack;
    DCHECK(!stack.isEmpty());
    events.push_back(
        createChangeEvent(*exitDoc, *stack.back().first, stack.back().second));
    fullscreen.popFullscreenElementStack();

    // TODO(foolip): See TODO in |fullyExitFullscreen()|.
    if (exitDoc == &doc && exitType == ExitType::Fully) {
      while (!stack.isEmpty())
        fullscreen.popFullscreenElementStack();
    }
  }

  // 9.8. For each |descendantDoc| in |descendantDocs|, in order, fire an
  // event named fullscreenchange on |descendantDoc|.
  // 9.9. For each |exitDoc| in |exitDocs|, in order, fire an event named
  // fullscreenchange on |exitDoc|.
  dispatchEvents(events);

  // 9.10. Fulfill |promise| with undefined.
  // TODO(foolip): Promises. https://crbug.com/644637
}

// https://fullscreen.spec.whatwg.org/#dom-document-fullscreenenabled
bool Fullscreen::fullscreenEnabled(Document& document) {
  // The fullscreenEnabled attribute's getter must return true if the context
  // object is allowed to use the feature indicated by attribute name
  // allowfullscreen and fullscreen is supported, and false otherwise.
  return allowedToUseFullscreen(document.frame()) &&
         fullscreenIsSupported(document);
}

void Fullscreen::setFullScreenLayoutObject(LayoutFullScreen* layoutObject) {
  if (layoutObject == m_fullScreenLayoutObject)
    return;

  if (layoutObject && m_savedPlaceholderComputedStyle) {
    layoutObject->createPlaceholder(std::move(m_savedPlaceholderComputedStyle),
                                    m_savedPlaceholderFrameRect);
  } else if (layoutObject && m_fullScreenLayoutObject &&
             m_fullScreenLayoutObject->placeholder()) {
    LayoutBlockFlow* placeholder = m_fullScreenLayoutObject->placeholder();
    layoutObject->createPlaceholder(
        ComputedStyle::clone(placeholder->styleRef()),
        placeholder->frameRect());
  }

  if (m_fullScreenLayoutObject)
    m_fullScreenLayoutObject->unwrapLayoutObject();
  DCHECK(!m_fullScreenLayoutObject);

  m_fullScreenLayoutObject = layoutObject;
}

void Fullscreen::fullScreenLayoutObjectDestroyed() {
  m_fullScreenLayoutObject = nullptr;
}

void Fullscreen::elementRemoved(Element& oldNode) {
  DCHECK_EQ(document(), &oldNode.document());

  // Whenever the removing steps run with an |oldNode| and |oldNode| is in its
  // node document's fullscreen element stack, run these steps:

  // 1. If |oldNode| is at the top of its node document's fullscreen element
  // stack, act as if the exitFullscreen() method was invoked on that document.
  if (fullscreenElement() == &oldNode) {
    exitFullscreen(oldNode.document());
    return;
  }

  // 2. Otherwise, remove |oldNode| from its node document's fullscreen element
  // stack.
  for (size_t i = 0; i < m_fullscreenElementStack.size(); ++i) {
    if (m_fullscreenElementStack[i].first.get() == &oldNode) {
      m_fullscreenElementStack.remove(i);
      return;
    }
  }

  // NOTE: |oldNode| was not in the fullscreen element stack.
}

void Fullscreen::popFullscreenElementStack() {
  DCHECK(!m_fullscreenElementStack.isEmpty());

  Element* previousElement = fullscreenElement();
  m_fullscreenElementStack.pop_back();

  // Note: |requestType| is only used if |fullscreenElement()| is non-null.
  RequestType requestType = m_fullscreenElementStack.isEmpty()
                                ? RequestType::Unprefixed
                                : m_fullscreenElementStack.back().second;
  fullscreenElementChanged(previousElement, fullscreenElement(), requestType);
}

void Fullscreen::pushFullscreenElementStack(Element& element,
                                            RequestType requestType) {
  Element* previousElement = fullscreenElement();
  m_fullscreenElementStack.push_back(std::make_pair(&element, requestType));

  fullscreenElementChanged(previousElement, &element, requestType);
}

void Fullscreen::fullscreenElementChanged(Element* fromElement,
                                          Element* toElement,
                                          RequestType toRequestType) {
  DCHECK_NE(fromElement, toElement);

  if (!document())
    return;

  document()->styleEngine().ensureUAStyleForFullscreen();

  if (m_fullScreenLayoutObject)
    m_fullScreenLayoutObject->unwrapLayoutObject();
  DCHECK(!m_fullScreenLayoutObject);

  if (fromElement) {
    DCHECK_NE(fromElement, fullscreenElement());

    fromElement->pseudoStateChanged(CSSSelector::PseudoFullScreen);

    fromElement->setContainsFullScreenElement(false);
    fromElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(
        false);
  }

  if (toElement) {
    DCHECK_EQ(toElement, fullscreenElement());

    toElement->pseudoStateChanged(CSSSelector::PseudoFullScreen);

    // OOPIF: For RequestType::PrefixedForCrossProcessDescendant, |toElement| is
    // the iframe element for the out-of-process frame that contains the
    // fullscreen element. Hence, it must match :-webkit-full-screen-ancestor.
    if (toRequestType == RequestType::PrefixedForCrossProcessDescendant) {
      DCHECK(isHTMLIFrameElement(toElement));
      toElement->setContainsFullScreenElement(true);
    }
    toElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(
        true);

    // Create a placeholder block for the fullscreen element, to keep the page
    // from reflowing when the element is removed from the normal flow. Only do
    // this for a LayoutBox, as only a box will have a frameRect. The
    // placeholder will be created in setFullScreenLayoutObject() during layout.
    LayoutObject* layoutObject = toElement->layoutObject();
    bool shouldCreatePlaceholder = layoutObject && layoutObject->isBox();
    if (shouldCreatePlaceholder) {
      m_savedPlaceholderFrameRect = toLayoutBox(layoutObject)->frameRect();
      m_savedPlaceholderComputedStyle =
          ComputedStyle::clone(layoutObject->styleRef());
    }

    if (toElement != document()->documentElement()) {
      LayoutFullScreen::wrapLayoutObject(
          layoutObject, layoutObject ? layoutObject->parent() : 0, document());
    }
  }

  if (LocalFrame* frame = document()->frame()) {
    // TODO(foolip): Synchronize hover state changes with animation frames.
    // https://crbug.com/668758
    frame->eventHandler().scheduleHoverStateUpdate();
    frame->chromeClient().fullscreenElementChanged(fromElement, toElement);

    if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() &&
        !RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      // Fullscreen status affects scroll paint properties through
      // FrameView::userInputScrollable().
      if (FrameView* frameView = frame->view())
        frameView->setNeedsPaintPropertyUpdate();
    }
  }

  // TODO(foolip): This should not call updateStyleAndLayoutTree.
  document()->updateStyleAndLayoutTree();
}

DEFINE_TRACE(Fullscreen) {
  visitor->trace(m_pendingRequests);
  visitor->trace(m_fullscreenElementStack);
  Supplement<Document>::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
