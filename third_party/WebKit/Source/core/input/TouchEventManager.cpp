// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/TouchEventManager.h"

#include "core/dom/Document.h"
#include "core/events/TouchEvent.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/input/EventHandler.h"
#include "core/input/TouchActionUtil.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/Histogram.h"
#include "platform/PlatformTouchEvent.h"



namespace blink {

namespace {

bool hasTouchHandlers(const EventHandlerRegistry& registry)
{
    return registry.hasEventHandlers(EventHandlerRegistry::TouchStartOrMoveEventBlocking)
        || registry.hasEventHandlers(EventHandlerRegistry::TouchStartOrMoveEventPassive)
        || registry.hasEventHandlers(EventHandlerRegistry::TouchEndOrCancelEventBlocking)
        || registry.hasEventHandlers(EventHandlerRegistry::TouchEndOrCancelEventPassive);
}

const AtomicString& touchEventNameForTouchPointState(PlatformTouchPoint::TouchState state)
{
    switch (state) {
    case PlatformTouchPoint::TouchReleased:
        return EventTypeNames::touchend;
    case PlatformTouchPoint::TouchCancelled:
        return EventTypeNames::touchcancel;
    case PlatformTouchPoint::TouchPressed:
        return EventTypeNames::touchstart;
    case PlatformTouchPoint::TouchMoved:
        return EventTypeNames::touchmove;
    case PlatformTouchPoint::TouchStationary:
        // Fall through to default
    default:
        ASSERT_NOT_REACHED();
        return emptyAtom;
    }
}

// These offsets change indicies into the ListenerHistogram
// enumeration. The addition of a series of offsets then
// produces the resulting ListenerHistogram value.
const size_t kTouchTargetHistogramRootScrollerOffset = 4;
const size_t kTouchTargetHistogramScrollableDocumentOffset = 2;
const size_t kTouchTargetHistogramHandledOffset = 1;

enum TouchTargetAndDispatchResultType {
    NonRootScrollerNonScrollableNotHandled, // Non-root-scroller, non-scrollable document, not handled.
    NonRootScrollerNonScrollableHandled, // Non-root-scroller, non-scrollable document, handled application.
    NonRootScrollerScrollableDocumentNotHandled, // Non-root-scroller, scrollable document, not handled.
    NonRootScrollerScrollableDocumentHandled, // Non-root-scroller, scrollable document, handled application.
    RootScrollerNonScrollableNotHandled, // Root-scroller, non-scrollable document, not handled.
    RootScrollerNonScrollableHandled, // Root-scroller, non-scrollable document, handled.
    RootScrollerScrollableDocumentNotHandled, // Root-scroller, scrollable document, not handled.
    RootScrollerScrollableDocumentHandled, // Root-scroller, scrollable document, handled.
    TouchTargetAndDispatchResultTypeMax,
};

TouchTargetAndDispatchResultType toTouchTargetHistogramValue(EventTarget* eventTarget, DispatchEventResult dispatchResult)
{
    int result = 0;
    Document* document = nullptr;

    if (const LocalDOMWindow* domWindow = eventTarget->toLocalDOMWindow()) {
        // Treat the window as a root scroller as well.
        document = domWindow->document();
        result += kTouchTargetHistogramRootScrollerOffset;
    } else if (Node* node = eventTarget->toNode()) {
        // Report if the target node is the document or body.
        if (node->isDocumentNode() || static_cast<Node*>(node->document().documentElement()) == node || static_cast<Node*>(node->document().body()) == node) {
            result += kTouchTargetHistogramRootScrollerOffset;
        }
        document = &node->document();
    }

    if (document) {
        FrameView* view = document->view();
        if (view && view->isScrollable())
            result += kTouchTargetHistogramScrollableDocumentOffset;
    }

    if (dispatchResult != DispatchEventResult::NotCanceled)
        result += kTouchTargetHistogramHandledOffset;
    return static_cast<TouchTargetAndDispatchResultType>(result);
}

enum TouchEventDispatchResultType {
    UnhandledTouches, // Unhandled touch events.
    HandledTouches, // Handled touch events.
    TouchEventDispatchResultTypeMax,
};

// Defining this class type local to dispatchTouchEvents() and annotating
// it with STACK_ALLOCATED(), runs into MSVC(VS 2013)'s C4822 warning
// that the local class doesn't provide a local definition for 'operator new'.
// Which it intentionally doesn't and shouldn't.
//
// Work around such toolchain bugginess by lifting out the type, thereby
// taking it out of C4822's reach.
class ChangedTouches final {
    STACK_ALLOCATED();
public:
    // The touches corresponding to the particular change state this struct
    // instance represents.
    Member<TouchList> m_touches;

    using EventTargetSet = HeapHashSet<Member<EventTarget>>;
    // Set of targets involved in m_touches.
    EventTargetSet m_targets;
};

} // namespace

TouchEventManager::TouchEventManager(LocalFrame* frame)
: m_frame(frame)
{
    clear();
}

TouchEventManager::~TouchEventManager()
{
}

WebInputEventResult TouchEventManager::dispatchTouchEvents(
    const PlatformTouchEvent& event,
    const HeapVector<TouchInfo>& touchInfos,
    bool allTouchesReleased)
{
    bool touchStartOrFirstTouchMove = false;
    if (event.type() == PlatformEvent::TouchStart) {
        m_waitingForFirstTouchMove = true;
        touchStartOrFirstTouchMove = true;
    } else if (event.type() == PlatformEvent::TouchMove) {
        touchStartOrFirstTouchMove = m_waitingForFirstTouchMove;
        m_waitingForFirstTouchMove = false;
    }

    // Build up the lists to use for the |touches|, |targetTouches| and
    // |changedTouches| attributes in the JS event. See
    // http://www.w3.org/TR/touch-events/#touchevent-interface for how these
    // lists fit together.

    // Holds the complete set of touches on the screen.
    TouchList* touches = TouchList::create();

    // A different view on the 'touches' list above, filtered and grouped by
    // event target. Used for the |targetTouches| list in the JS event.
    using TargetTouchesHeapMap = HeapHashMap<EventTarget*, Member<TouchList>>;
    TargetTouchesHeapMap touchesByTarget;

    // Array of touches per state, used to assemble the |changedTouches| list.
    ChangedTouches changedTouches[PlatformTouchPoint::TouchStateEnd];

    for (unsigned i = 0; i < touchInfos.size(); ++i) {
        const TouchInfo& touchInfo = touchInfos[i];
        const PlatformTouchPoint& point = touchInfo.point;
        PlatformTouchPoint::TouchState pointState = point.state();

        if (touchInfo.consumed)
            continue;

        Touch* touch = Touch::create(
            touchInfo.targetFrame.get(),
            touchInfo.touchNode.get(),
            point.id(),
            point.screenPos(),
            touchInfo.adjustedPagePoint,
            touchInfo.adjustedRadius,
            point.rotationAngle(),
            point.force(),
            touchInfo.region);

        // Ensure this target's touch list exists, even if it ends up empty, so
        // it can always be passed to TouchEvent::Create below.
        TargetTouchesHeapMap::iterator targetTouchesIterator = touchesByTarget.find(touchInfo.touchNode.get());
        if (targetTouchesIterator == touchesByTarget.end()) {
            touchesByTarget.set(touchInfo.touchNode.get(), TouchList::create());
            targetTouchesIterator = touchesByTarget.find(touchInfo.touchNode.get());
        }

        // |touches| and |targetTouches| should only contain information about
        // touches still on the screen, so if this point is released or
        // cancelled it will only appear in the |changedTouches| list.
        if (pointState != PlatformTouchPoint::TouchReleased && pointState != PlatformTouchPoint::TouchCancelled) {
            touches->append(touch);
            targetTouchesIterator->value->append(touch);
        }

        // Now build up the correct list for |changedTouches|.
        // Note that  any touches that are in the TouchStationary state (e.g. if
        // the user had several points touched but did not move them all) should
        // never be in the |changedTouches| list so we do not handle them
        // explicitly here. See https://bugs.webkit.org/show_bug.cgi?id=37609
        // for further discussion about the TouchStationary state.
        if (pointState != PlatformTouchPoint::TouchStationary && touchInfo.knownTarget) {
            ASSERT(pointState < PlatformTouchPoint::TouchStateEnd);
            if (!changedTouches[pointState].m_touches)
                changedTouches[pointState].m_touches = TouchList::create();
            changedTouches[pointState].m_touches->append(touch);
            changedTouches[pointState].m_targets.add(touchInfo.touchNode);
        }
    }

    if (allTouchesReleased) {
        m_touchSequenceDocument.clear();
        m_touchSequenceUserGestureToken.clear();
    }

    WebInputEventResult eventResult = WebInputEventResult::NotHandled;

    // Now iterate through the |changedTouches| list and |m_targets| within it,
    // sending TouchEvents to the targets as required.
    for (unsigned state = 0; state != PlatformTouchPoint::TouchStateEnd; ++state) {
        if (!changedTouches[state].m_touches)
            continue;

        const AtomicString& eventName(touchEventNameForTouchPointState(static_cast<PlatformTouchPoint::TouchState>(state)));
        for (const auto& eventTarget : changedTouches[state].m_targets) {
            EventTarget* touchEventTarget = eventTarget;
            TouchEvent* touchEvent = TouchEvent::create(
                touches, touchesByTarget.get(touchEventTarget), changedTouches[state].m_touches.get(),
                eventName, touchEventTarget->toNode()->document().domWindow(),
                event.getModifiers(), event.cancelable(), event.causesScrollingIfUncanceled(), event.timestamp());

            DispatchEventResult domDispatchResult = touchEventTarget->dispatchEvent(touchEvent);

            // Only report for top level documents with a single touch on
            // touch-start or the first touch-move.
            if (touchStartOrFirstTouchMove && touchInfos.size() == 1 && event.cancelable() && m_frame->isMainFrame()) {
                DEFINE_STATIC_LOCAL(EnumerationHistogram, rootDocumentListenerHistogram, ("Event.Touch.TargetAndDispatchResult", TouchTargetAndDispatchResultTypeMax));
                rootDocumentListenerHistogram.count(toTouchTargetHistogramValue(eventTarget, domDispatchResult));

                // Count the handled touch starts and first touch moves before and after the page is fully loaded respectively.
                if (m_frame->document()->isLoadCompleted()) {
                    DEFINE_STATIC_LOCAL(EnumerationHistogram, touchDispositionsAfterPageLoadHistogram, ("Event.Touch.TouchDispositionsAfterPageLoad", TouchEventDispatchResultTypeMax));
                    touchDispositionsAfterPageLoadHistogram.count((domDispatchResult != DispatchEventResult::NotCanceled) ? HandledTouches : UnhandledTouches);
                } else {
                    DEFINE_STATIC_LOCAL(EnumerationHistogram, touchDispositionsBeforePageLoadHistogram, ("Event.Touch.TouchDispositionsBeforePageLoad", TouchEventDispatchResultTypeMax));
                    touchDispositionsBeforePageLoadHistogram.count((domDispatchResult != DispatchEventResult::NotCanceled) ? HandledTouches : UnhandledTouches);
                }
            }
            eventResult = EventHandler::mergeEventResult(eventResult,
                EventHandler::toWebInputEventResult(domDispatchResult));
        }
    }
    return eventResult;
}

void TouchEventManager::updateTargetAndRegionMapsForTouchStarts(
    HeapVector<TouchInfo>& touchInfos)
{
    for (auto& touchInfo : touchInfos) {
        // Touch events implicitly capture to the touched node, and don't change
        // active/hover states themselves (Gesture events do). So we only need
        // to hit-test on touchstart and when the target could be different than
        // the corresponding pointer event target.
        if (touchInfo.point.state() == PlatformTouchPoint::TouchPressed) {
            HitTestRequest::HitTestRequestType hitType = HitTestRequest::TouchEvent | HitTestRequest::ReadOnly | HitTestRequest::Active;
            LayoutPoint pagePoint = roundedLayoutPoint(m_frame->view()->rootFrameToContents(touchInfo.point.pos()));
            HitTestResult result;
            if (!m_touchSequenceDocument) {
                result = m_frame->eventHandler().hitTestResultAtPoint(pagePoint, hitType);
            } else if (m_touchSequenceDocument->frame()) {
                LayoutPoint framePoint = roundedLayoutPoint(m_touchSequenceDocument->frame()->view()->rootFrameToContents(touchInfo.point.pos()));
                result = EventHandler::hitTestResultInFrame(m_touchSequenceDocument->frame(), framePoint, hitType);
            } else {
                continue;
            }

            Node* node = result.innerNode();
            if (!node)
                continue;
            if (isHTMLCanvasElement(node)) {
                std::pair<Element*, String> regionInfo = toHTMLCanvasElement(node)->getControlAndIdIfHitRegionExists(result.pointInInnerNodeFrame());
                if (regionInfo.first)
                    node = regionInfo.first;
                touchInfo.region = regionInfo.second;
            }
            // Touch events should not go to text nodes.
            if (node->isTextNode())
                node = FlatTreeTraversal::parent(*node);
            touchInfo.touchNode = node;

            if (!m_touchSequenceDocument) {
                // Keep track of which document should receive all touch events
                // in the active sequence. This must be a single document to
                // ensure we don't leak Nodes between documents.
                m_touchSequenceDocument = &(touchInfo.touchNode->document());
                ASSERT(m_touchSequenceDocument->frame()->view());
            }

            // Ideally we'd ASSERT(!m_targetForTouchID.contains(point.id())
            // since we shouldn't get a touchstart for a touch that's already
            // down. However EventSender allows this to be violated and there's
            // some tests that take advantage of it. There may also be edge
            // cases in the browser where this happens.
            // See http://crbug.com/345372.
            m_targetForTouchID.set(touchInfo.point.id(), touchInfo.touchNode);

            m_regionForTouchID.set(touchInfo.point.id(), touchInfo.region);

            TouchAction effectiveTouchAction =
                TouchActionUtil::computeEffectiveTouchAction(
                    *touchInfo.touchNode);
            if (effectiveTouchAction != TouchActionAuto)
                m_frame->page()->chromeClient().setTouchAction(effectiveTouchAction);
        }
    }
}

void TouchEventManager::setAllPropertiesOfTouchInfos(
    HeapVector<TouchInfo>& touchInfos)
{
    for (auto& touchInfo : touchInfos) {
        PlatformTouchPoint::TouchState pointState = touchInfo.point.state();
        Node* touchNode = nullptr;
        String regionID;

        if (pointState == PlatformTouchPoint::TouchReleased
            || pointState == PlatformTouchPoint::TouchCancelled) {
            // The target should be the original target for this touch, so get
            // it from the hashmap. As it's a release or cancel we also remove
            // it from the map.
            touchNode = m_targetForTouchID.take(touchInfo.point.id());
            regionID = m_regionForTouchID.take(touchInfo.point.id());
        } else {
            // No hittest is performed on move or stationary, since the target
            // is not allowed to change anyway.
            touchNode = m_targetForTouchID.get(touchInfo.point.id());
            regionID = m_regionForTouchID.get(touchInfo.point.id());
        }

        LocalFrame* targetFrame = nullptr;
        bool knownTarget = false;
        if (touchNode) {
            Document& doc = touchNode->document();
            // If the target node has moved to a new document while it was being touched,
            // we can't send events to the new document because that could leak nodes
            // from one document to another. See http://crbug.com/394339.
            if (&doc == m_touchSequenceDocument.get()) {
                targetFrame = doc.frame();
                knownTarget = true;
            }
        }
        if (!knownTarget) {
            // If we don't have a target registered for the point it means we've
            // missed our opportunity to do a hit test for it (due to some
            // optimization that prevented blink from ever seeing the
            // touchstart), or that the touch started outside the active touch
            // sequence document. We should still include the touch in the
            // Touches list reported to the application (eg. so it can
            // differentiate between a one and two finger gesture), but we won't
            // actually dispatch any events for it. Set the target to the
            // Document so that there's some valid node here. Perhaps this
            // should really be LocalDOMWindow, but in all other cases the target of
            // a Touch is a Node so using the window could be a breaking change.
            // Since we know there was no handler invoked, the specific target
            // should be completely irrelevant to the application.
            touchNode = m_touchSequenceDocument;
            targetFrame = m_touchSequenceDocument->frame();
        }
        ASSERT(targetFrame);

        // pagePoint should always be in the target element's document coordinates.
        FloatPoint pagePoint = targetFrame->view()->rootFrameToContents(
            touchInfo.point.pos());
        float scaleFactor = 1.0f / targetFrame->pageZoomFactor();

        touchInfo.touchNode = touchNode;
        touchInfo.targetFrame = targetFrame;
        touchInfo.adjustedPagePoint = pagePoint.scaledBy(scaleFactor);
        touchInfo.adjustedRadius = touchInfo.point.radius().scaledBy(scaleFactor);
        touchInfo.knownTarget = knownTarget;
        touchInfo.consumed = false;
        touchInfo.region = regionID;
    }
}

bool TouchEventManager::generateTouchInfosAfterHittest(
    const PlatformTouchEvent& event,
    HeapVector<TouchInfo>& touchInfos)
{
    bool newTouchSequence = true;
    bool allTouchesReleased = true;

    for (const auto& point : event.touchPoints()) {
        if (point.state() != PlatformTouchPoint::TouchPressed)
            newTouchSequence = false;
        if (point.state() != PlatformTouchPoint::TouchReleased && point.state() != PlatformTouchPoint::TouchCancelled)
            allTouchesReleased = false;
    }
    if (newTouchSequence) {
        // Ideally we'd ASSERT(!m_touchSequenceDocument) here since we should
        // have cleared the active document when we saw the last release. But we
        // have some tests that violate this, ClusterFuzz could trigger it, and
        // there may be cases where the browser doesn't reliably release all
        // touches. http://crbug.com/345372 tracks this.
        m_touchSequenceDocument.clear();
        m_touchSequenceUserGestureToken.clear();
    }

    ASSERT(m_frame->view());
    if (m_touchSequenceDocument && (!m_touchSequenceDocument->frame() || !m_touchSequenceDocument->frame()->view())) {
        // If the active touch document has no frame or view, it's probably being destroyed
        // so we can't dispatch events.
        return false;
    }

    for (const auto& point : event.touchPoints()) {
        TouchEventManager::TouchInfo touchInfo;
        touchInfo.point = point;
        touchInfos.append(touchInfo);
    }

    updateTargetAndRegionMapsForTouchStarts(touchInfos);

    m_touchPressed = !allTouchesReleased;

    // If there's no document receiving touch events, or no handlers on the
    // document set to receive the events, then we can skip all the rest of
    // this work.
    if (!m_touchSequenceDocument || !m_touchSequenceDocument->frameHost() || !hasTouchHandlers(m_touchSequenceDocument->frameHost()->eventHandlerRegistry()) || !m_touchSequenceDocument->frame()) {
        if (allTouchesReleased) {
            m_touchSequenceDocument.clear();
            m_touchSequenceUserGestureToken.clear();
        }
        return false;
    }

    setAllPropertiesOfTouchInfos(touchInfos);

    return true;
}

WebInputEventResult TouchEventManager::handleTouchEvent(
    const PlatformTouchEvent& event,
    const HeapVector<TouchInfo>& touchInfos)
{
    // Note that the disposition of any pointer events affects only the generation of touch
    // events. If all pointer events were handled (and hence no touch events were fired), that
    // is still equivalent to the touch events going unhandled because pointer event handler
    // don't block scroll gesture generation.

    // TODO(crbug.com/507408): If PE handlers always call preventDefault, we won't see TEs until after
    // scrolling starts because the scrolling would suppress upcoming PEs. This sudden "break" in TE
    // suppression can make the visible TEs inconsistent (e.g. touchmove without a touchstart).

    bool allTouchesReleased = true;
    for (const auto& point : event.touchPoints()) {
        if (point.state() != PlatformTouchPoint::TouchReleased
            && point.state() != PlatformTouchPoint::TouchCancelled)
            allTouchesReleased = false;
    }

    // Whether a touch should be considered a "user gesture" or not is a tricky question.
    // https://docs.google.com/document/d/1oF1T3O7_E4t1PYHV6gyCwHxOi3ystm0eSL5xZu7nvOg/edit#
    // TODO(rbyers): Disable user gesture in some cases but retain logging for now (crbug.com/582140).
    OwnPtr<UserGestureIndicator> gestureIndicator;
    if (event.touchPoints().size() == 1
        && event.touchPoints()[0].state() == PlatformTouchPoint::TouchReleased
        && !event.causesScrollingIfUncanceled()) {
        // This is a touchend corresponding to a tap, definitely a user gesture.  So don't supply
        // a UserGestureUtilizedCallback.
        gestureIndicator = adoptPtr(new UserGestureIndicator(DefinitelyProcessingUserGesture));
    } else {
        // This is some other touch event that perhaps shouldn't be considered a user gesture.  So
        // use a UserGestureUtilizedCallback to get metrics / deprecation warnings.
        if (m_touchSequenceUserGestureToken)
            gestureIndicator = adoptPtr(new UserGestureIndicator(m_touchSequenceUserGestureToken.release(), &m_touchSequenceDocument->frame()->eventHandler()));
        else
            gestureIndicator = adoptPtr(new UserGestureIndicator(DefinitelyProcessingUserGesture, &m_touchSequenceDocument->frame()->eventHandler()));
        m_touchSequenceUserGestureToken = UserGestureIndicator::currentToken();
    }

    return dispatchTouchEvents(event, touchInfos, allTouchesReleased);
}

void TouchEventManager::clear()
{
    m_touchSequenceDocument.clear();
    m_touchSequenceUserGestureToken.clear();
    m_targetForTouchID.clear();
    m_regionForTouchID.clear();
    m_touchPressed = false;
    m_waitingForFirstTouchMove = false;
}

bool TouchEventManager::isAnyTouchActive() const
{
    return m_touchPressed;
}

DEFINE_TRACE(TouchEventManager)
{
    visitor->trace(m_frame);
    visitor->trace(m_touchSequenceDocument);
    visitor->trace(m_targetForTouchID);
}

} // namespace blink
