/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#include "core/page/PageWidgetDelegate.h"

#include "core/dom/AXObjectCache.h"
#include "core/events/WebInputEventConversion.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/input/EventHandler.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/paint/TransformRecorder.h"
#include "core/paint/compositing/PaintLayerCompositor.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/wtf/CurrentTime.h"
#include "public/platform/WebInputEvent.h"

namespace blink {

void PageWidgetDelegate::Animate(Page& page,
                                 double monotonic_frame_begin_time) {
  page.GetAutoscrollController().Animate(monotonic_frame_begin_time);
  page.Animator().ServiceScriptedAnimations(monotonic_frame_begin_time);
}

void PageWidgetDelegate::UpdateAllLifecyclePhases(Page& page,
                                                  LocalFrame& root) {
  page.Animator().UpdateAllLifecyclePhases(root);
}

static void PaintInternal(Page& page,
                          WebCanvas* canvas,
                          const WebRect& rect,
                          LocalFrame& root,
                          const GlobalPaintFlags global_paint_flags) {
  if (rect.IsEmpty())
    return;

  IntRect int_rect(rect);
  // TODO(enne): intRect is not correct: http://crbug.com/703231
  PaintRecordBuilder builder(int_rect);
  {
    GraphicsContext& paint_context = builder.Context();

    // FIXME: device scale factor settings are layering violations and should
    // not be used within Blink paint code.
    float scale_factor = page.DeviceScaleFactorDeprecated();
    paint_context.SetDeviceScaleFactor(scale_factor);

    AffineTransform scale;
    scale.Scale(scale_factor);
    TransformRecorder scale_recorder(paint_context, builder, scale);

    IntRect dirty_rect(rect);
    LocalFrameView* view = root.View();
    view->UpdateAllLifecyclePhasesExceptPaint();
    if (view) {
      ClipRecorder clip_recorder(paint_context, builder,
                                 DisplayItem::kPageWidgetDelegateClip,
                                 dirty_rect);
      view->PaintWithLifecycleUpdate(paint_context, global_paint_flags,
                                     CullRect(dirty_rect));
    } else {
      DrawingRecorder recorder(
          paint_context, builder,
          DisplayItem::kPageWidgetDelegateBackgroundFallback, dirty_rect);
      paint_context.FillRect(dirty_rect, Color::kWhite);
    }
  }

  builder.EndRecording(*canvas);
}

void PageWidgetDelegate::Paint(Page& page,
                               WebCanvas* canvas,
                               const WebRect& rect,
                               LocalFrame& root) {
  PaintInternal(page, canvas, rect, root, kGlobalPaintNormalPhase);
}

void PageWidgetDelegate::PaintIgnoringCompositing(Page& page,
                                                  WebCanvas* canvas,
                                                  const WebRect& rect,
                                                  LocalFrame& root) {
  PaintInternal(page, canvas, rect, root, kGlobalPaintFlattenCompositingLayers);
}

WebInputEventResult PageWidgetDelegate::HandleInputEvent(
    PageWidgetEventHandler& handler,
    const WebCoalescedInputEvent& coalesced_event,
    LocalFrame* root) {
  const WebInputEvent& event = coalesced_event.Event();
  if (event.GetModifiers() & WebInputEvent::kIsTouchAccessibility &&
      WebInputEvent::IsMouseEventType(event.GetType())) {
    WebMouseEvent mouse_event = TransformWebMouseEvent(
        root->View(), static_cast<const WebMouseEvent&>(event));

    IntPoint doc_point(root->View()->RootFrameToContents(
        FlooredIntPoint(mouse_event.PositionInRootFrame())));
    HitTestResult result = root->GetEventHandler().HitTestResultAtPoint(
        doc_point, HitTestRequest::kReadOnly | HitTestRequest::kActive);
    result.SetToShadowHostIfInRestrictedShadowRoot();
    if (result.InnerNodeFrame()) {
      Document* document = result.InnerNodeFrame()->GetDocument();
      if (document) {
        AXObjectCache* cache = document->ExistingAXObjectCache();
        if (cache) {
          cache->OnTouchAccessibilityHover(
              result.RoundedPointInInnerNodeFrame());
        }
      }
    }
  }

  switch (event.GetType()) {
    // FIXME: WebKit seems to always return false on mouse events processing
    // methods. For now we'll assume it has processed them (as we are only
    // interested in whether keyboard events are processed).
    // FIXME: Why do we return HandleSuppressed when there is no root or
    // the root is detached?
    case WebInputEvent::kMouseMove:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      handler.HandleMouseMove(*root, static_cast<const WebMouseEvent&>(event),
                              coalesced_event.GetCoalescedEventsPointers());
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::kMouseLeave:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      handler.HandleMouseLeave(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::kMouseDown:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      handler.HandleMouseDown(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::kMouseUp:
      if (!root || !root->View())
        return WebInputEventResult::kHandledSuppressed;
      handler.HandleMouseUp(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::kHandledSystem;
    case WebInputEvent::kMouseWheel:
      if (!root || !root->View())
        return WebInputEventResult::kNotHandled;
      return handler.HandleMouseWheel(
          *root, static_cast<const WebMouseWheelEvent&>(event));

    case WebInputEvent::kRawKeyDown:
    case WebInputEvent::kKeyDown:
    case WebInputEvent::kKeyUp:
      return handler.HandleKeyEvent(
          static_cast<const WebKeyboardEvent&>(event));

    case WebInputEvent::kChar:
      return handler.HandleCharEvent(
          static_cast<const WebKeyboardEvent&>(event));
    case WebInputEvent::kGestureScrollBegin:
    case WebInputEvent::kGestureScrollEnd:
    case WebInputEvent::kGestureScrollUpdate:
    case WebInputEvent::kGestureFlingStart:
    case WebInputEvent::kGestureFlingCancel:
    case WebInputEvent::kGestureTap:
    case WebInputEvent::kGestureTapUnconfirmed:
    case WebInputEvent::kGestureTapDown:
    case WebInputEvent::kGestureShowPress:
    case WebInputEvent::kGestureTapCancel:
    case WebInputEvent::kGestureDoubleTap:
    case WebInputEvent::kGestureTwoFingerTap:
    case WebInputEvent::kGestureLongPress:
    case WebInputEvent::kGestureLongTap:
      return handler.HandleGestureEvent(
          static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::kTouchStart:
    case WebInputEvent::kTouchMove:
    case WebInputEvent::kTouchEnd:
    case WebInputEvent::kTouchCancel:
    case WebInputEvent::kTouchScrollStarted:
      if (!root || !root->View())
        return WebInputEventResult::kNotHandled;
      return handler.HandleTouchEvent(
          *root, static_cast<const WebTouchEvent&>(event),
          coalesced_event.GetCoalescedEventsPointers());
    case WebInputEvent::kGesturePinchBegin:
    case WebInputEvent::kGesturePinchEnd:
    case WebInputEvent::kGesturePinchUpdate:
      // Touchscreen pinch events are currently not handled in main thread.
      // Once they are, these should be passed to |handleGestureEvent| similar
      // to gesture scroll events.
      return WebInputEventResult::kNotHandled;
    default:
      return WebInputEventResult::kNotHandled;
  }
}

// ----------------------------------------------------------------
// Default handlers for PageWidgetEventHandler

void PageWidgetEventHandler::HandleMouseMove(
    LocalFrame& main_frame,
    const WebMouseEvent& event,
    const std::vector<const WebInputEvent*>& coalesced_events) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(main_frame.View(), event);
  main_frame.GetEventHandler().HandleMouseMoveEvent(
      transformed_event,
      TransformWebMouseEventVector(main_frame.View(), coalesced_events));
}

void PageWidgetEventHandler::HandleMouseLeave(LocalFrame& main_frame,
                                              const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(main_frame.View(), event);
  main_frame.GetEventHandler().HandleMouseLeaveEvent(transformed_event);
}

void PageWidgetEventHandler::HandleMouseDown(LocalFrame& main_frame,
                                             const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(main_frame.View(), event);
  main_frame.GetEventHandler().HandleMousePressEvent(transformed_event);
}

void PageWidgetEventHandler::HandleMouseUp(LocalFrame& main_frame,
                                           const WebMouseEvent& event) {
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(main_frame.View(), event);
  main_frame.GetEventHandler().HandleMouseReleaseEvent(transformed_event);
}

WebInputEventResult PageWidgetEventHandler::HandleMouseWheel(
    LocalFrame& frame,
    const WebMouseWheelEvent& event) {
  WebMouseWheelEvent transformed_event =
      TransformWebMouseWheelEvent(frame.View(), event);
  return frame.GetEventHandler().HandleWheelEvent(transformed_event);
}

WebInputEventResult PageWidgetEventHandler::HandleTouchEvent(
    LocalFrame& main_frame,
    const WebTouchEvent& event,
    const std::vector<const WebInputEvent*>& coalesced_events) {
  WebTouchEvent transformed_event =
      TransformWebTouchEvent(main_frame.View(), event);
  return main_frame.GetEventHandler().HandleTouchEvent(
      transformed_event,
      TransformWebTouchEventVector(main_frame.View(), coalesced_events));
}

}  // namespace blink
