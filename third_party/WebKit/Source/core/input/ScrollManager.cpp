// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/ScrollManager.h"

#include "core/dom/DOMNodeIds.h"
#include "core/events/GestureEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/TopControls.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutPart.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/RootScroller.h"
#include "core/page/scrolling/ScrollState.h"
#include "core/paint/PaintLayer.h"
#include "platform/PlatformGestureEvent.h"


namespace blink {

namespace {

// TODO(bokan): This method can go away once all scrolls happen through the
// scroll customization path.
void computeScrollChainForSingleNode(Node& node, std::deque<int>& scrollChain)
{
    scrollChain.clear();

    DCHECK(node.layoutObject());
    Element* element = toElement(&node);

    scrollChain.push_front(DOMNodeIds::idForNode(element));
}

void recomputeScrollChain(const LocalFrame& frame, const Node& startNode,
    std::deque<int>& scrollChain)
{
    scrollChain.clear();

    DCHECK(startNode.layoutObject());
    LayoutBox* curBox = startNode.layoutObject()->enclosingBox();

    // Scrolling propagates along the containing block chain.
    while (curBox && !curBox->isLayoutView()) {
        Node* curNode = curBox->node();
        // FIXME: this should reject more elements, as part of crbug.com/410974.
        if (curNode && curNode->isElementNode()) {
            Element* curElement = toElement(curNode);
            if (curElement == frame.document()->scrollingElement())
                break;
            scrollChain.push_front(DOMNodeIds::idForNode(curElement));
        }
        curBox = curBox->containingBlock();
    }
    // TODO(tdresser): this should sometimes be excluded, as part of crbug.com/410974.
    // We need to ensure that the scrollingElement is always part of
    // the scroll chain. In quirks mode, when the scrollingElement is
    // the body, some elements may use the documentElement as their
    // containingBlock, so we ensure the scrollingElement is added
    // here.
    scrollChain.push_front(DOMNodeIds::idForNode(frame.document()->scrollingElement()));
}

} // namespace

ScrollManager::ScrollManager(LocalFrame* frame)
: m_frame(frame)
{
    clear();
}

ScrollManager::~ScrollManager()
{
}

void ScrollManager::clear()
{
    m_lastGestureScrollOverWidget = false;
    m_scrollbarHandlingScrollGesture = nullptr;
    m_resizeScrollableArea = nullptr;
    m_offsetFromResizeCorner = LayoutSize();
    clearGestureScrollState();
}

void ScrollManager::clearGestureScrollState()
{
    m_scrollGestureHandlingNode = nullptr;
    m_previousGestureScrolledNode = nullptr;
    m_deltaConsumedForScrollSequence = false;
    m_currentScrollChain.clear();

    if (FrameHost* host = frameHost()) {
        bool resetX = true;
        bool resetY = true;
        host->overscrollController().resetAccumulated(resetX, resetY);
    }
}

void ScrollManager::stopAutoscroll()
{
    if (AutoscrollController* controller = autoscrollController())
        controller->stopAutoscroll();
}

bool ScrollManager::panScrollInProgress() const
{
    return autoscrollController() && autoscrollController()->panScrollInProgress();
}

AutoscrollController* ScrollManager::autoscrollController() const
{
    if (Page* page = m_frame->page())
        return &page->autoscrollController();
    return nullptr;
}

ScrollResult ScrollManager::physicalScroll(ScrollGranularity granularity,
    const FloatSize& delta, const FloatPoint& position,
    const FloatSize& velocity, Node* startNode, Node** stopNode, bool* consumed)
{
    if (consumed)
        *consumed = false;
    if (delta.isZero())
        return ScrollResult();

    Node* node = startNode;
    DCHECK(node && node->layoutObject());

    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    ScrollResult result;

    LayoutBox* curBox = node->layoutObject()->enclosingBox();
    while (curBox) {
        // If we're at the stopNode, we should try to scroll it but we shouldn't
        // chain past it.
        bool shouldStopChaining =
            stopNode && *stopNode && curBox->node() == *stopNode;
        bool wasRootScroller = false;

        result = scrollBox(
            curBox,
            granularity,
            delta,
            position,
            velocity,
            &wasRootScroller);

        if (result.didScroll() && stopNode)
            *stopNode = curBox->node();

        if (result.didScroll() || shouldStopChaining) {
            setFrameWasScrolledByUser();
            if (consumed)
                *consumed = true;
            return result;
        }
        if (wasRootScroller) {
            // Don't try to chain past the root scroller, even if there's
            // eligible ancestors.
            break;
        }

        curBox = curBox->containingBlock();
    }

    return result;
}

bool ScrollManager::logicalScroll(ScrollDirection direction, ScrollGranularity granularity, Node* startNode, Node* mousePressNode)
{
    Node* node = startNode;

    if (!node)
        node = m_frame->document()->focusedElement();

    if (!node)
        node = mousePressNode;

    if ((!node || !node->layoutObject()) && m_frame->view() && !m_frame->view()->layoutViewItem().isNull())
        node = m_frame->view()->layoutViewItem().node();

    if (!node)
        return false;

    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    LayoutBox* curBox = node->layoutObject()->enclosingBox();
    while (curBox) {
        ScrollDirectionPhysical physicalDirection = toPhysicalDirection(
            direction, curBox->isHorizontalWritingMode(), curBox->style()->isFlippedBlocksWritingMode());

        ScrollResult result = curBox->scroll(granularity, toScrollDelta(physicalDirection, 1));

        if (result.didScroll()) {
            setFrameWasScrolledByUser();
            return true;
        }

        curBox = curBox->containingBlock();
    }

    return false;
}

// TODO(bokan): This should be merged with logicalScroll assuming
// defaultSpaceEventHandler's chaining scroll can be done crossing frames.
bool ScrollManager::bubblingScroll(ScrollDirection direction, ScrollGranularity granularity, Node* startingNode, Node* mousePressNode)
{
    // The layout needs to be up to date to determine if we can scroll. We may be
    // here because of an onLoad event, in which case the final layout hasn't been performed yet.
    m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();
    // FIXME: enable scroll customization in this case. See crbug.com/410974.
    if (logicalScroll(direction, granularity, startingNode, mousePressNode))
        return true;

    Frame* parentFrame = m_frame->tree().parent();
    if (!parentFrame || !parentFrame->isLocalFrame())
        return false;
    // FIXME: Broken for OOPI.
    return toLocalFrame(parentFrame)->eventHandler().bubblingScroll(direction, granularity, m_frame->deprecatedLocalOwner());
}

ScrollResult ScrollManager::scrollBox(LayoutBox* box,
    ScrollGranularity granularity, const FloatSize& delta,
    const FloatPoint& position, const FloatSize& velocity,
    bool* wasRootScroller)
{
    DCHECK(box);
    Node* node = box->node();

    // If there's no ApplyScroll callback on the element, scroll as usuall in
    // the non-scroll-customization case.
    if (!node || !node->isElementNode() || !toElement(node)->getApplyScroll()) {
        *wasRootScroller = false;
        return box->scroll(granularity, delta);
    }

    // Viewport actions should only happen when scrolling an element in the
    // main frame.
    DCHECK(m_frame->isMainFrame());

    // If there is an ApplyScroll callback, its because we placed one on the
    // root scroller to control top controls and overscroll. Invoke a scroll
    // using parts of the scroll customization framework on just this element.
    computeScrollChainForSingleNode(*node, m_currentScrollChain);

    OwnPtr<ScrollStateData> scrollStateData = adoptPtr(new ScrollStateData());
    scrollStateData->delta_x = delta.width();
    scrollStateData->delta_y = delta.height();
    scrollStateData->position_x = position.x();
    scrollStateData->position_y = position.y();
    // TODO(bokan): delta_granularity is meant to be the number of pixels per
    // unit of delta but we can't determine that until we get to the area we'll
    // scroll. This is a hack, we stuff the enum into the double value for
    // now.
    scrollStateData->delta_granularity = static_cast<double>(granularity);
    scrollStateData->velocity_x = velocity.width();
    scrollStateData->velocity_y = velocity.height();
    scrollStateData->should_propagate = false;
    scrollStateData->is_in_inertial_phase = false;
    scrollStateData->from_user_input = true;
    scrollStateData->delta_consumed_for_scroll_sequence = false;
    ScrollState* scrollState =
        ScrollState::create(std::move(scrollStateData));

    customizedScroll(*node, *scrollState);

    ScrollResult result(
        scrollState->deltaX() != delta.width(),
        scrollState->deltaY() != delta.height(),
        scrollState->deltaX(),
        scrollState->deltaY());

    *wasRootScroller = true;
    m_currentScrollChain.clear();

    return result;
}

void ScrollManager::setFrameWasScrolledByUser()
{
    if (DocumentLoader* documentLoader = m_frame->loader().documentLoader())
        documentLoader->initialScrollState().wasScrolledByUser = true;
}

void ScrollManager::customizedScroll(const Node& startNode, ScrollState& scrollState)
{
    if (scrollState.fullyConsumed())
        return;

    if (scrollState.deltaX() || scrollState.deltaY())
        m_frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    if (m_currentScrollChain.empty())
        recomputeScrollChain(*m_frame, startNode, m_currentScrollChain);
    scrollState.setScrollChain(m_currentScrollChain);

    scrollState.distributeToScrollChainDescendant();
}

WebInputEventResult ScrollManager::handleGestureScrollBegin(const PlatformGestureEvent& gestureEvent)
{
    Document* document = m_frame->document();
    if (document->layoutViewItem().isNull())
        return WebInputEventResult::NotHandled;

    FrameView* view = m_frame->view();
    if (!view)
        return WebInputEventResult::NotHandled;

    // If there's no layoutObject on the node, send the event to the nearest ancestor with a layoutObject.
    // Needed for <option> and <optgroup> elements so we can touch scroll <select>s
    while (m_scrollGestureHandlingNode && !m_scrollGestureHandlingNode->layoutObject())
        m_scrollGestureHandlingNode = m_scrollGestureHandlingNode->parentOrShadowHostNode();

    if (!m_scrollGestureHandlingNode) {
        if (RuntimeEnabledFeatures::scrollCustomizationEnabled())
            m_scrollGestureHandlingNode = m_frame->document()->documentElement();
        else
            return WebInputEventResult::NotHandled;
    }
    DCHECK(m_scrollGestureHandlingNode);

    passScrollGestureEventToWidget(gestureEvent, m_scrollGestureHandlingNode->layoutObject());
    if (RuntimeEnabledFeatures::scrollCustomizationEnabled()) {
        m_currentScrollChain.clear();
        OwnPtr<ScrollStateData> scrollStateData = adoptPtr(new ScrollStateData());
        scrollStateData->position_x = gestureEvent.position().x();
        scrollStateData->position_y = gestureEvent.position().y();
        scrollStateData->is_beginning = true;
        scrollStateData->from_user_input = true;
        scrollStateData->delta_consumed_for_scroll_sequence = m_deltaConsumedForScrollSequence;
        ScrollState* scrollState = ScrollState::create(std::move(scrollStateData));
        customizedScroll(*m_scrollGestureHandlingNode.get(), *scrollState);
    } else {
        if (m_frame->isMainFrame())
            m_frame->host()->topControls().scrollBegin();
    }
    return WebInputEventResult::HandledSystem;
}

WebInputEventResult ScrollManager::handleGestureScrollUpdate(const PlatformGestureEvent& gestureEvent)
{
    DCHECK_EQ(gestureEvent.type(), PlatformEvent::GestureScrollUpdate);

    // Negate the deltas since the gesture event stores finger movement and
    // scrolling occurs in the direction opposite the finger's movement
    // direction. e.g. Finger moving up has negative event delta but causes the
    // page to scroll down causing positive scroll delta.
    FloatSize delta(-gestureEvent.deltaX(), -gestureEvent.deltaY());
    FloatSize velocity(-gestureEvent.velocityX(), -gestureEvent.velocityY());
    if (delta.isZero())
        return WebInputEventResult::NotHandled;

    ScrollGranularity granularity = gestureEvent.deltaUnits();
    Node* node = m_scrollGestureHandlingNode.get();

    // Scroll customization is only available for touch.
    bool handleScrollCustomization = RuntimeEnabledFeatures::scrollCustomizationEnabled() && gestureEvent.source() == PlatformGestureSourceTouchscreen;
    if (node) {
        LayoutObject* layoutObject = node->layoutObject();
        if (!layoutObject)
            return WebInputEventResult::NotHandled;

        // Try to send the event to the correct view.
        WebInputEventResult result = passScrollGestureEventToWidget(gestureEvent, layoutObject);
        if (result != WebInputEventResult::NotHandled) {
            if (gestureEvent.preventPropagation()
                && !RuntimeEnabledFeatures::scrollCustomizationEnabled()) {
                // This is an optimization which doesn't apply with
                // scroll customization enabled.
                m_previousGestureScrolledNode = m_scrollGestureHandlingNode;
            }
            // FIXME: we should allow simultaneous scrolling of nested
            // iframes along perpendicular axes. See crbug.com/466991.
            m_deltaConsumedForScrollSequence = true;
            return result;
        }

        if (handleScrollCustomization) {
            OwnPtr<ScrollStateData> scrollStateData = adoptPtr(new ScrollStateData());
            scrollStateData->delta_x = delta.width();
            scrollStateData->delta_y = delta.height();
            scrollStateData->delta_granularity = ScrollByPrecisePixel;
            scrollStateData->velocity_x = velocity.width();
            scrollStateData->velocity_y = velocity.height();
            scrollStateData->should_propagate = !gestureEvent.preventPropagation();
            scrollStateData->is_in_inertial_phase = gestureEvent.inertialPhase() == ScrollInertialPhaseMomentum;
            scrollStateData->from_user_input = true;
            scrollStateData->delta_consumed_for_scroll_sequence = m_deltaConsumedForScrollSequence;
            ScrollState* scrollState = ScrollState::create(std::move(scrollStateData));
            if (m_previousGestureScrolledNode) {
                // The ScrollState needs to know what the current
                // native scrolling element is, so that for an
                // inertial scroll that shouldn't propagate, only the
                // currently scrolling element responds.
                DCHECK(m_previousGestureScrolledNode->isElementNode());
                scrollState->setCurrentNativeScrollingElement(toElement(m_previousGestureScrolledNode.get()));
            }
            customizedScroll(*node, *scrollState);
            m_previousGestureScrolledNode = scrollState->currentNativeScrollingElement();
            m_deltaConsumedForScrollSequence = scrollState->deltaConsumedForScrollSequence();
            if (scrollState->deltaX() != delta.width()
                || scrollState->deltaY() != delta.height()) {
                setFrameWasScrolledByUser();
                return WebInputEventResult::HandledSystem;
            }
        } else {
            Node* stopNode = nullptr;
            if (gestureEvent.preventPropagation())
                stopNode = m_previousGestureScrolledNode.get();

            bool consumed = false;
            ScrollResult result = physicalScroll(
                granularity,
                delta,
                FloatPoint(gestureEvent.position()),
                velocity,
                node,
                &stopNode,
                &consumed);

            if (gestureEvent.preventPropagation())
                m_previousGestureScrolledNode = stopNode;

            if ((!stopNode || !isRootScroller(*stopNode)) && frameHost()) {
                frameHost()->overscrollController().resetAccumulated(
                    result.didScrollX, result.didScrollY);
            }

            if (consumed)
                return WebInputEventResult::HandledSystem;
        }
    }

    return WebInputEventResult::NotHandled;
}

WebInputEventResult ScrollManager::handleGestureScrollEnd(const PlatformGestureEvent& gestureEvent)
{
    Node* node = m_scrollGestureHandlingNode;

    if (node) {
        passScrollGestureEventToWidget(gestureEvent, node->layoutObject());
        if (RuntimeEnabledFeatures::scrollCustomizationEnabled()) {
            OwnPtr<ScrollStateData> scrollStateData = adoptPtr(new ScrollStateData());
            scrollStateData->is_ending = true;
            scrollStateData->is_in_inertial_phase = gestureEvent.inertialPhase() == ScrollInertialPhaseMomentum;
            scrollStateData->from_user_input = true;
            scrollStateData->is_direct_manipulation = true;
            scrollStateData->delta_consumed_for_scroll_sequence = m_deltaConsumedForScrollSequence;
            ScrollState* scrollState = ScrollState::create(std::move(scrollStateData));
            customizedScroll(*node, *scrollState);
        }
    }

    clearGestureScrollState();
    return WebInputEventResult::NotHandled;
}

FrameHost* ScrollManager::frameHost() const
{
    if (!m_frame->page())
        return nullptr;

    return &m_frame->page()->frameHost();
}

WebInputEventResult ScrollManager::passScrollGestureEventToWidget(const PlatformGestureEvent& gestureEvent, LayoutObject* layoutObject)
{
    DCHECK(gestureEvent.isScrollEvent());

    if (!m_lastGestureScrollOverWidget || !layoutObject || !layoutObject->isLayoutPart())
        return WebInputEventResult::NotHandled;

    Widget* widget = toLayoutPart(layoutObject)->widget();

    if (!widget || !widget->isFrameView())
        return WebInputEventResult::NotHandled;

    return toFrameView(widget)->frame().eventHandler().handleGestureScrollEvent(gestureEvent);
}

bool ScrollManager::isRootScroller(const Node& node) const
{
    // The root scroller is the one Element on the page designated to perform
    // "viewport actions" like top controls movement and overscroll glow.
    if (!frameHost() || !frameHost()->rootScroller())
        return false;

    return frameHost()->rootScroller()->get() == &node;
}


WebInputEventResult ScrollManager::handleGestureScrollEvent(const PlatformGestureEvent& gestureEvent)
{
    Node* eventTarget = nullptr;
    Scrollbar* scrollbar = nullptr;
    if (gestureEvent.type() != PlatformEvent::GestureScrollBegin) {
        scrollbar = m_scrollbarHandlingScrollGesture.get();
        eventTarget = m_scrollGestureHandlingNode.get();
    }

    if (!eventTarget) {
        Document* document = m_frame->document();
        if (document->layoutViewItem().isNull())
            return WebInputEventResult::NotHandled;

        FrameView* view = m_frame->view();
        LayoutPoint viewPoint = view->rootFrameToContents(gestureEvent.position());
        HitTestRequest request(HitTestRequest::ReadOnly);
        HitTestResult result(request, viewPoint);
        document->layoutViewItem().hitTest(result);

        eventTarget = result.innerNode();

        m_lastGestureScrollOverWidget = result.isOverWidget();
        m_scrollGestureHandlingNode = eventTarget;
        m_previousGestureScrolledNode = nullptr;

        if (!scrollbar)
            scrollbar = result.scrollbar();
    }

    if (scrollbar) {
        bool shouldUpdateCapture = false;
        if (scrollbar->gestureEvent(gestureEvent, &shouldUpdateCapture)) {
            if (shouldUpdateCapture)
                m_scrollbarHandlingScrollGesture = scrollbar;
            return WebInputEventResult::HandledSuppressed;
        }
        m_scrollbarHandlingScrollGesture = nullptr;
    }

    if (eventTarget) {
        if (handleScrollGestureOnResizer(eventTarget, gestureEvent))
            return WebInputEventResult::HandledSuppressed;

        GestureEvent* gestureDomEvent = GestureEvent::create(eventTarget->document().domWindow(), gestureEvent);
        if (gestureDomEvent) {
            DispatchEventResult gestureDomEventResult = eventTarget->dispatchEvent(gestureDomEvent);
            if (gestureDomEventResult != DispatchEventResult::NotCanceled) {
                DCHECK(gestureDomEventResult != DispatchEventResult::CanceledByEventHandler);
                return EventHandler::toWebInputEventResult(gestureDomEventResult);
            }
        }
    }

    switch (gestureEvent.type()) {
    case PlatformEvent::GestureScrollBegin:
        return handleGestureScrollBegin(gestureEvent);
    case PlatformEvent::GestureScrollUpdate:
        return handleGestureScrollUpdate(gestureEvent);
    case PlatformEvent::GestureScrollEnd:
        return handleGestureScrollEnd(gestureEvent);
    case PlatformEvent::GestureFlingStart:
    case PlatformEvent::GesturePinchBegin:
    case PlatformEvent::GesturePinchEnd:
    case PlatformEvent::GesturePinchUpdate:
        return WebInputEventResult::NotHandled;
    default:
        NOTREACHED();
        return WebInputEventResult::NotHandled;
    }
}

bool ScrollManager::isScrollbarHandlingGestures() const
{
    return m_scrollbarHandlingScrollGesture.get();
}

bool ScrollManager::handleScrollGestureOnResizer(Node* eventTarget, const PlatformGestureEvent& gestureEvent)
{
    if (gestureEvent.type() == PlatformEvent::GestureScrollBegin) {
        PaintLayer* layer = eventTarget->layoutObject() ? eventTarget->layoutObject()->enclosingLayer() : nullptr;
        IntPoint p = m_frame->view()->rootFrameToContents(gestureEvent.position());
        if (layer && layer->getScrollableArea() && layer->getScrollableArea()->isPointInResizeControl(p, ResizerForTouch)) {
            m_resizeScrollableArea = layer->getScrollableArea();
            m_resizeScrollableArea->setInResizeMode(true);
            m_offsetFromResizeCorner = LayoutSize(m_resizeScrollableArea->offsetFromResizeCorner(p));
            return true;
        }
    } else if (gestureEvent.type() == PlatformEvent::GestureScrollUpdate) {
        if (m_resizeScrollableArea && m_resizeScrollableArea->inResizeMode()) {
            m_resizeScrollableArea->resize(gestureEvent, m_offsetFromResizeCorner);
            return true;
        }
    } else if (gestureEvent.type() == PlatformEvent::GestureScrollEnd) {
        if (m_resizeScrollableArea && m_resizeScrollableArea->inResizeMode()) {
            m_resizeScrollableArea->setInResizeMode(false);
            m_resizeScrollableArea = nullptr;
            return false;
        }
    }

    return false;
}

bool ScrollManager::inResizeMode() const
{
    return m_resizeScrollableArea && m_resizeScrollableArea->inResizeMode();
}

void ScrollManager::resize(const PlatformEvent& evt)
{
    m_resizeScrollableArea->resize(evt, m_offsetFromResizeCorner);
}

void ScrollManager::clearResizeScrollableArea(bool shouldNotBeNull)
{
    if (shouldNotBeNull)
        DCHECK(m_resizeScrollableArea);

    if (m_resizeScrollableArea)
        m_resizeScrollableArea->setInResizeMode(false);
    m_resizeScrollableArea = nullptr;
}

void ScrollManager::setResizeScrollableArea(PaintLayer* layer, IntPoint p)
{
    m_resizeScrollableArea = layer->getScrollableArea();
    m_resizeScrollableArea->setInResizeMode(true);
    m_offsetFromResizeCorner = LayoutSize(m_resizeScrollableArea->offsetFromResizeCorner(p));
}

bool ScrollManager::canHandleGestureEvent(const GestureEventWithHitTestResults& targetedEvent)
{
    Scrollbar* scrollbar = targetedEvent.hitTestResult().scrollbar();

    if (scrollbar) {
        bool shouldUpdateCapture = false;
        if (scrollbar->gestureEvent(targetedEvent.event(), &shouldUpdateCapture)) {
            if (shouldUpdateCapture)
                m_scrollbarHandlingScrollGesture = scrollbar;
            return true;
        }
    }
    return false;
}

DEFINE_TRACE(ScrollManager)
{
    visitor->trace(m_frame);
    visitor->trace(m_scrollGestureHandlingNode);
    visitor->trace(m_previousGestureScrolledNode);
    visitor->trace(m_scrollbarHandlingScrollGesture);
    visitor->trace(m_resizeScrollableArea);
}

} // namespace blink
