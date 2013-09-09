/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef EventHandler_h
#define EventHandler_h

#include "core/dom/TextEventInputType.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/editing/TextGranularity.h"
#include "core/page/DragActions.h"
#include "core/page/FocusDirection.h"
#include "core/platform/Cursor.h"
#include "core/platform/PlatformMouseEvent.h"
#include "core/platform/ScrollTypes.h"
#include "core/platform/Timer.h"
#include "core/platform/graphics/LayoutPoint.h"
#include "core/rendering/HitTestRequest.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Clipboard;
class Document;
class Element;
class Event;
class EventTarget;
class FloatPoint;
class FloatQuad;
class FullscreenElementStack;
class Frame;
class HTMLFrameSetElement;
class HitTestRequest;
class HitTestResult;
class KeyboardEvent;
class MouseEventWithHitTestResults;
class Node;
class OptionalCursor;
class PlatformGestureEvent;
class PlatformKeyboardEvent;
class PlatformTouchEvent;
class PlatformWheelEvent;
class RenderLayer;
class RenderObject;
class RenderWidget;
class SVGElementInstance;
class ScrollableArea;
class Scrollbar;
class TextEvent;
class TouchEvent;
class VisibleSelection;
class WheelEvent;
class Widget;

struct DragState;

extern const int LinkDragHysteresis;
extern const int ImageDragHysteresis;
extern const int TextDragHysteresis;
extern const int GeneralDragHysteresis;

enum AppendTrailingWhitespace { ShouldAppendTrailingWhitespace, DontAppendTrailingWhitespace };
enum CheckDragHysteresis { ShouldCheckDragHysteresis, DontCheckDragHysteresis };

class EventHandler {
    WTF_MAKE_NONCOPYABLE(EventHandler);
public:
    explicit EventHandler(Frame*);
    ~EventHandler();

    void clear();
    void nodeWillBeRemoved(Node*);

    void updateSelectionForMouseDrag();

    Node* mousePressNode() const;
    void setMousePressNode(PassRefPtr<Node>);

#if OS(WIN)
    void startPanScrolling(RenderObject*);
#endif

    void stopAutoscrollTimer();
    bool mouseDownWasInSubframe() const { return m_mouseDownWasInSubframe; }

    void dispatchFakeMouseMoveEventSoon();
    void dispatchFakeMouseMoveEventSoonInQuad(const FloatQuad&);

    HitTestResult hitTestResultAtPoint(const LayoutPoint&,
        HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active,
        const LayoutSize& padding = LayoutSize());

    bool mousePressed() const { return m_mousePressed; }
    void setMousePressed(bool pressed) { m_mousePressed = pressed; }

    void setCapturingMouseEventsNode(PassRefPtr<Node>); // A caller is responsible for resetting capturing node to 0.

    bool updateDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    void cancelDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    bool performDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    void updateDragStateAfterEditDragIfNeeded(Element* rootEditableElement);

    void scheduleHoverStateUpdate();

    void setResizingFrameSet(HTMLFrameSetElement*);

    void resizeLayerDestroyed();

    IntPoint lastKnownMousePosition() const;
    Cursor currentMouseCursor() const { return m_currentMouseCursor; }

    static Frame* subframeForTargetNode(Node*);
    static Frame* subframeForHitTestResult(const MouseEventWithHitTestResults&);

    bool scrollOverflow(ScrollDirection, ScrollGranularity, Node* startingNode = 0);
    bool scrollRecursively(ScrollDirection, ScrollGranularity, Node* startingNode = 0);
    bool logicalScrollRecursively(ScrollLogicalDirection, ScrollGranularity, Node* startingNode = 0);

    bool handleMouseMoveEvent(const PlatformMouseEvent&);
    void handleMouseLeaveEvent(const PlatformMouseEvent&);

    void lostMouseCapture();

    bool handleMousePressEvent(const PlatformMouseEvent&);
    bool handleMouseReleaseEvent(const PlatformMouseEvent&);
    bool handleWheelEvent(const PlatformWheelEvent&);
    void defaultWheelEventHandler(Node*, WheelEvent*);
    bool handlePasteGlobalSelection(const PlatformMouseEvent&);

    bool handleGestureEvent(const PlatformGestureEvent&);
    bool handleGestureTap(const PlatformGestureEvent&);
    bool handleGestureLongPress(const PlatformGestureEvent&);
    bool handleGestureLongTap(const PlatformGestureEvent&);
    bool handleGestureTwoFingerTap(const PlatformGestureEvent&);
    bool handleGestureScrollUpdate(const PlatformGestureEvent&);
    bool handleGestureScrollBegin(const PlatformGestureEvent&);
    bool handleGestureScrollEnd(const PlatformGestureEvent&);
    void clearGestureScrollNodes();
    bool isScrollbarHandlingGestures() const;

    bool shouldApplyTouchAdjustment(const PlatformGestureEvent&) const;

    bool bestClickableNodeForTouchPoint(const IntPoint& touchCenter, const IntSize& touchRadius, IntPoint& targetPoint, Node*& targetNode);
    bool bestContextMenuNodeForTouchPoint(const IntPoint& touchCenter, const IntSize& touchRadius, IntPoint& targetPoint, Node*& targetNode);
    bool bestZoomableAreaForTouchPoint(const IntPoint& touchCenter, const IntSize& touchRadius, IntRect& targetArea, Node*& targetNode);

    bool adjustGesturePosition(const PlatformGestureEvent&, IntPoint& adjustedPoint);

    bool sendContextMenuEvent(const PlatformMouseEvent&);
    bool sendContextMenuEventForKey();
    bool sendContextMenuEventForGesture(const PlatformGestureEvent&);

    void setMouseDownMayStartAutoscroll() { m_mouseDownMayStartAutoscroll = true; }

    static unsigned accessKeyModifiers();
    bool handleAccessKey(const PlatformKeyboardEvent&);
    bool keyEvent(const PlatformKeyboardEvent&);
    void defaultKeyboardEventHandler(KeyboardEvent*);

    bool handleTextInputEvent(const String& text, Event* underlyingEvent = 0, TextEventInputType = TextEventInputKeyboard);
    void defaultTextInputEventHandler(TextEvent*);

    void dragSourceEndedAt(const PlatformMouseEvent&, DragOperation);

    void focusDocumentView();

    void capsLockStateMayHaveChanged(); // Only called by FrameSelection

    void sendResizeEvent(); // Only called in FrameView
    void sendScrollEvent(); // Ditto

    bool handleTouchEvent(const PlatformTouchEvent&);

    bool useHandCursor(Node*, bool isOverLink, bool shiftKey);

private:
    static DragState& dragState();
    static const double TextDragDelay;

    PassRefPtr<Clipboard> createDraggingClipboard() const;

    bool updateSelectionForMouseDownDispatchingSelectStart(Node*, const VisibleSelection&, TextGranularity);
    void selectClosestWordFromHitTestResult(const HitTestResult&, AppendTrailingWhitespace);
    void selectClosestMisspellingFromHitTestResult(const HitTestResult&, AppendTrailingWhitespace);
    void selectClosestWordFromMouseEvent(const MouseEventWithHitTestResults&);
    void selectClosestMisspellingFromMouseEvent(const MouseEventWithHitTestResults&);
    void selectClosestWordOrLinkFromMouseEvent(const MouseEventWithHitTestResults&);

    bool handleMouseMoveOrLeaveEvent(const PlatformMouseEvent&, HitTestResult* hoveredNode = 0, bool onlyUpdateScrollbars = false);
    bool handleMousePressEvent(const MouseEventWithHitTestResults&);
    bool handleMousePressEventSingleClick(const MouseEventWithHitTestResults&);
    bool handleMousePressEventDoubleClick(const MouseEventWithHitTestResults&);
    bool handleMousePressEventTripleClick(const MouseEventWithHitTestResults&);
    bool handleMouseDraggedEvent(const MouseEventWithHitTestResults&);
    bool handleMouseReleaseEvent(const MouseEventWithHitTestResults&);

    OptionalCursor selectCursor(const MouseEventWithHitTestResults&, Scrollbar*);
    void hoverTimerFired(Timer<EventHandler>*);

    bool logicalScrollOverflow(ScrollLogicalDirection, ScrollGranularity, Node* startingNode = 0);

    bool shouldTurnVerticalTicksIntoHorizontal(const HitTestResult&, const PlatformWheelEvent&) const;
    bool mouseDownMayStartSelect() const { return m_mouseDownMayStartSelect; }

    static bool isKeyboardOptionTab(KeyboardEvent*);

    void fakeMouseMoveEventTimerFired(Timer<EventHandler>*);
    void cancelFakeMouseMoveEvent();
    bool isCursorVisible() const;

    bool isInsideScrollbar(const IntPoint&) const;

    ScrollableArea* associatedScrollableArea(const RenderLayer*) const;

    bool dispatchSyntheticTouchEventIfEnabled(const PlatformMouseEvent&);
    HitTestResult hitTestResultInFrame(Frame*, const LayoutPoint&, HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent);

    void invalidateClick();

    Node* nodeUnderMouse() const;

    void updateMouseEventTargetNode(Node*, const PlatformMouseEvent&, bool fireMouseOverOut);
    void fireMouseOverOut(bool fireMouseOver = true, bool fireMouseOut = true, bool updateLastNodeUnderMouse = true);

    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const PlatformMouseEvent&);

    bool dispatchMouseEvent(const AtomicString& eventType, Node* target, bool cancelable, int clickCount, const PlatformMouseEvent&, bool setUnder);
    bool dispatchDragEvent(const AtomicString& eventType, Node* target, const PlatformMouseEvent&, Clipboard*);

    void freeClipboard();

    bool handleDrag(const MouseEventWithHitTestResults&, CheckDragHysteresis);
    bool tryStartDrag(const MouseEventWithHitTestResults&);
    void clearDragState();

    bool dispatchDragSrcEvent(const AtomicString& eventType, const PlatformMouseEvent&);

    bool dragHysteresisExceeded(const FloatPoint&) const;
    bool dragHysteresisExceeded(const IntPoint&) const;

    bool passMousePressEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);
    bool passMouseMoveEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe, HitTestResult* hoveredNode = 0);
    bool passMouseReleaseEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);

    bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe, HitTestResult* hoveredNode = 0);

    bool passMousePressEventToScrollbar(MouseEventWithHitTestResults&, Scrollbar*);

    bool passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&);

    bool passWheelEventToWidget(const PlatformWheelEvent&, Widget*);

    void defaultSpaceEventHandler(KeyboardEvent*);
    void defaultBackspaceEventHandler(KeyboardEvent*);
    void defaultTabEventHandler(KeyboardEvent*);
    void defaultEscapeEventHandler(KeyboardEvent*);
    void defaultArrowEventHandler(FocusDirection, KeyboardEvent*);

    DragSourceAction updateDragSourceActionsAllowed() const;

    void updateSelectionForMouseDrag(const HitTestResult&);

    void updateLastScrollbarUnderMouse(Scrollbar*, bool);

    void setFrameWasScrolledByUser();

    bool capturesDragging() const { return m_capturesDragging; }

    bool isKeyEventAllowedInFullScreen(FullscreenElementStack*, const PlatformKeyboardEvent&) const;

    bool handleGestureTapDown();

    bool handleScrollGestureOnResizer(Node*, const PlatformGestureEvent&);

    bool passGestureEventToWidget(const PlatformGestureEvent&, Widget*);
    bool passGestureEventToWidgetIfPossible(const PlatformGestureEvent&, RenderObject*);
    bool sendScrollEventToView(const PlatformGestureEvent&, const FloatSize&);
    Frame* getSubFrameForGestureEvent(const IntPoint& touchAdjustedPoint, const PlatformGestureEvent&);

    bool panScrollInProgress() const;
    void setLastKnownMousePosition(const PlatformMouseEvent&);

    Frame* const m_frame;

    bool m_mousePressed;
    bool m_capturesDragging;
    RefPtr<Node> m_mousePressNode;

    bool m_mouseDownMayStartSelect;
    bool m_mouseDownMayStartDrag;
    bool m_dragMayStartSelectionInstead;
    bool m_mouseDownWasSingleClickInSelection;
    enum SelectionInitiationState { HaveNotStartedSelection, PlacedCaret, ExtendedSelection };
    SelectionInitiationState m_selectionInitiationState;

    LayoutPoint m_dragStartPos;

    bool m_panScrollButtonPressed;

    Timer<EventHandler> m_hoverTimer;

    bool m_mouseDownMayStartAutoscroll;
    bool m_mouseDownWasInSubframe;

    Timer<EventHandler> m_fakeMouseMoveEventTimer;

    bool m_svgPan;
    RefPtr<SVGElementInstance> m_instanceUnderMouse;
    RefPtr<SVGElementInstance> m_lastInstanceUnderMouse;

    RenderLayer* m_resizeLayer;

    RefPtr<Node> m_capturingMouseEventsNode;
    bool m_eventHandlerWillResetCapturingMouseEventsNode;

    RefPtr<Node> m_nodeUnderMouse;
    RefPtr<Node> m_lastNodeUnderMouse;
    RefPtr<Frame> m_lastMouseMoveEventSubframe;
    RefPtr<Scrollbar> m_lastScrollbarUnderMouse;
    Cursor m_currentMouseCursor;

    int m_clickCount;
    RefPtr<Node> m_clickNode;

    RefPtr<Node> m_dragTarget;
    bool m_shouldOnlyFireDragOverEvent;

    RefPtr<HTMLFrameSetElement> m_frameSetBeingResized;

    LayoutSize m_offsetFromResizeCorner; // In the coords of m_resizeLayer.

    bool m_mousePositionIsUnknown;
    IntPoint m_lastKnownMousePosition;
    IntPoint m_lastKnownMouseGlobalPosition;
    IntPoint m_mouseDownPos; // In our view's coords.
    double m_mouseDownTimestamp;
    PlatformMouseEvent m_mouseDown;
    RefPtr<UserGestureToken> m_lastMouseDownUserGestureToken;

    RefPtr<Node> m_latchedWheelEventNode;
    bool m_widgetIsLatched;

    RefPtr<Node> m_previousWheelScrolledNode;

    typedef HashMap<int, RefPtr<EventTarget> > TouchTargetMap;
    TouchTargetMap m_originatingTouchPointTargets;
    RefPtr<Document> m_originatingTouchPointDocument;
    unsigned m_originatingTouchPointTargetKey;
    bool m_touchPressed;

    RefPtr<Node> m_scrollGestureHandlingNode;
    bool m_lastHitTestResultOverWidget;
    RefPtr<Node> m_previousGestureScrolledNode;
    RefPtr<Scrollbar> m_scrollbarHandlingScrollGesture;

    double m_maxMouseMovedDuration;
    PlatformEvent::Type m_baseEventType;
    bool m_didStartDrag;

    bool m_longTapShouldInvokeContextMenu;
};

} // namespace WebCore

#endif // EventHandler_h
