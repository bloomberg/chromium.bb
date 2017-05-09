// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/WebFrameWidgetBase.h"

#include "core/dom/DocumentUserGestureToken.h"
#include "core/exported/WebViewBase.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/input/EventHandler.h"
#include "core/page/DragActions.h"
#include "core/page/DragController.h"
#include "core/page/DragData.h"
#include "core/page/DragSession.h"
#include "core/page/Page.h"
#include "core/page/PointerLockController.h"
#include "platform/UserGestureIndicator.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebDocument.h"
#include "public/web/WebWidgetClient.h"
#include "web/WebInputEventConversion.h"

namespace blink {

namespace {

// Helper to get LocalFrame* from WebLocalFrame*.
// TODO(dcheng): This should be moved into WebLocalFrame.
LocalFrame* ToCoreFrame(WebLocalFrame* frame) {
  return ToWebLocalFrameBase(frame)->GetFrame();
}

}  // namespace

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enum : " #a)
STATIC_ASSERT_ENUM(kDragOperationNone, kWebDragOperationNone);
STATIC_ASSERT_ENUM(kDragOperationCopy, kWebDragOperationCopy);
STATIC_ASSERT_ENUM(kDragOperationLink, kWebDragOperationLink);
STATIC_ASSERT_ENUM(kDragOperationGeneric, kWebDragOperationGeneric);
STATIC_ASSERT_ENUM(kDragOperationPrivate, kWebDragOperationPrivate);
STATIC_ASSERT_ENUM(kDragOperationMove, kWebDragOperationMove);
STATIC_ASSERT_ENUM(kDragOperationDelete, kWebDragOperationDelete);
STATIC_ASSERT_ENUM(kDragOperationEvery, kWebDragOperationEvery);

bool WebFrameWidgetBase::ignore_input_events_ = false;

WebDragOperation WebFrameWidgetBase::DragTargetDragEnter(
    const WebDragData& web_drag_data,
    const WebPoint& point_in_viewport,
    const WebPoint& screen_point,
    WebDragOperationsMask operations_allowed,
    int modifiers) {
  DCHECK(!current_drag_data_);

  current_drag_data_ = DataObject::Create(web_drag_data);
  operations_allowed_ = operations_allowed;

  return DragTargetDragEnterOrOver(point_in_viewport, screen_point, kDragEnter,
                                   modifiers);
}

WebDragOperation WebFrameWidgetBase::DragTargetDragOver(
    const WebPoint& point_in_viewport,
    const WebPoint& screen_point,
    WebDragOperationsMask operations_allowed,
    int modifiers) {
  operations_allowed_ = operations_allowed;

  return DragTargetDragEnterOrOver(point_in_viewport, screen_point, kDragOver,
                                   modifiers);
}

void WebFrameWidgetBase::DragTargetDragLeave(const WebPoint& point_in_viewport,
                                             const WebPoint& screen_point) {
  DCHECK(current_drag_data_);

  // TODO(paulmeyer): It shouldn't be possible for |m_currentDragData| to be
  // null here, but this is somehow happening (rarely). This suggests that in
  // some cases drag-leave is happening before drag-enter, which should be
  // impossible. This needs to be investigated further. Once fixed, the extra
  // check for |!m_currentDragData| should be removed. (crbug.com/671152)
  if (IgnoreInputEvents() || !current_drag_data_) {
    CancelDrag();
    return;
  }

  WebPoint point_in_root_frame(ViewportToRootFrame(point_in_viewport));
  DragData drag_data(current_drag_data_.Get(), point_in_root_frame,
                     screen_point,
                     static_cast<DragOperation>(operations_allowed_));

  GetPage()->GetDragController().DragExited(&drag_data,
                                            *ToCoreFrame(LocalRoot()));

  // FIXME: why is the drag scroll timer not stopped here?

  drag_operation_ = kWebDragOperationNone;
  current_drag_data_ = nullptr;
}

void WebFrameWidgetBase::DragTargetDrop(const WebDragData& web_drag_data,
                                        const WebPoint& point_in_viewport,
                                        const WebPoint& screen_point,
                                        int modifiers) {
  WebPoint point_in_root_frame(ViewportToRootFrame(point_in_viewport));

  DCHECK(current_drag_data_);
  current_drag_data_ = DataObject::Create(web_drag_data);

  // If this webview transitions from the "drop accepting" state to the "not
  // accepting" state, then our IPC message reply indicating that may be in-
  // flight, or else delayed by javascript processing in this webview.  If a
  // drop happens before our IPC reply has reached the browser process, then
  // the browser forwards the drop to this webview.  So only allow a drop to
  // proceed if our webview m_dragOperation state is not DragOperationNone.

  if (drag_operation_ == kWebDragOperationNone) {
    // IPC RACE CONDITION: do not allow this drop.
    DragTargetDragLeave(point_in_viewport, screen_point);
    return;
  }

  if (!IgnoreInputEvents()) {
    current_drag_data_->SetModifiers(modifiers);
    DragData drag_data(current_drag_data_.Get(), point_in_root_frame,
                       screen_point,
                       static_cast<DragOperation>(operations_allowed_));

    GetPage()->GetDragController().PerformDrag(&drag_data,
                                               *ToCoreFrame(LocalRoot()));
  }
  drag_operation_ = kWebDragOperationNone;
  current_drag_data_ = nullptr;
}

void WebFrameWidgetBase::DragSourceEndedAt(const WebPoint& point_in_viewport,
                                           const WebPoint& screen_point,
                                           WebDragOperation operation) {
  if (IgnoreInputEvents()) {
    CancelDrag();
    return;
  }
  WebFloatPoint point_in_root_frame(
      GetPage()->GetVisualViewport().ViewportToRootFrame(point_in_viewport));

  WebMouseEvent fake_mouse_move(WebInputEvent::kMouseMove, point_in_root_frame,
                                WebFloatPoint(screen_point.x, screen_point.y),
                                WebPointerProperties::Button::kLeft, 0,
                                WebInputEvent::kNoModifiers,
                                TimeTicks::Now().InSeconds());
  fake_mouse_move.SetFrameScale(1);
  ToCoreFrame(LocalRoot())
      ->GetEventHandler()
      .DragSourceEndedAt(fake_mouse_move,
                         static_cast<DragOperation>(operation));
}

void WebFrameWidgetBase::DragSourceSystemDragEnded() {
  CancelDrag();
}

void WebFrameWidgetBase::CancelDrag() {
  // It's possible for us this to be callback while we're not doing a drag if
  // it's from a previous page that got unloaded.
  if (!doing_drag_and_drop_)
    return;
  GetPage()->GetDragController().DragEnded();
  doing_drag_and_drop_ = false;
}

void WebFrameWidgetBase::StartDragging(WebReferrerPolicy policy,
                                       const WebDragData& data,
                                       WebDragOperationsMask mask,
                                       const WebImage& drag_image,
                                       const WebPoint& drag_image_offset) {
  doing_drag_and_drop_ = true;
  Client()->StartDragging(policy, data, mask, drag_image, drag_image_offset);
}

WebDragOperation WebFrameWidgetBase::DragTargetDragEnterOrOver(
    const WebPoint& point_in_viewport,
    const WebPoint& screen_point,
    DragAction drag_action,
    int modifiers) {
  DCHECK(current_drag_data_);
  // TODO(paulmeyer): It shouldn't be possible for |m_currentDragData| to be
  // null here, but this is somehow happening (rarely). This suggests that in
  // some cases drag-over is happening before drag-enter, which should be
  // impossible. This needs to be investigated further. Once fixed, the extra
  // check for |!m_currentDragData| should be removed. (crbug.com/671504)
  if (IgnoreInputEvents() || !current_drag_data_) {
    CancelDrag();
    return kWebDragOperationNone;
  }

  WebPoint point_in_root_frame(ViewportToRootFrame(point_in_viewport));

  current_drag_data_->SetModifiers(modifiers);
  DragData drag_data(current_drag_data_.Get(), point_in_root_frame,
                     screen_point,
                     static_cast<DragOperation>(operations_allowed_));

  DragSession drag_session;
  drag_session = GetPage()->GetDragController().DragEnteredOrUpdated(
      &drag_data, *ToCoreFrame(LocalRoot()));

  DragOperation drop_effect = drag_session.operation;

  // Mask the drop effect operation against the drag source's allowed
  // operations.
  if (!(drop_effect & drag_data.DraggingSourceOperationMask()))
    drop_effect = kDragOperationNone;

  drag_operation_ = static_cast<WebDragOperation>(drop_effect);

  return drag_operation_;
}

WebPoint WebFrameWidgetBase::ViewportToRootFrame(
    const WebPoint& point_in_viewport) const {
  return GetPage()->GetVisualViewport().ViewportToRootFrame(point_in_viewport);
}

WebViewBase* WebFrameWidgetBase::View() const {
  return ToWebLocalFrameBase(LocalRoot())->ViewImpl();
}

Page* WebFrameWidgetBase::GetPage() const {
  return View()->GetPage();
}

void WebFrameWidgetBase::DidAcquirePointerLock() {
  GetPage()->GetPointerLockController().DidAcquirePointerLock();
}

void WebFrameWidgetBase::DidNotAcquirePointerLock() {
  GetPage()->GetPointerLockController().DidNotAcquirePointerLock();
}

void WebFrameWidgetBase::DidLosePointerLock() {
  pointer_lock_gesture_token_.Clear();
  GetPage()->GetPointerLockController().DidLosePointerLock();
}

void WebFrameWidgetBase::PointerLockMouseEvent(const WebInputEvent& event) {
  std::unique_ptr<UserGestureIndicator> gesture_indicator;
  AtomicString event_type;
  switch (event.GetType()) {
    case WebInputEvent::kMouseDown:
      event_type = EventTypeNames::mousedown;
      if (!GetPage() || !GetPage()->GetPointerLockController().GetElement())
        break;
      gesture_indicator = WTF::WrapUnique(new UserGestureIndicator(
          DocumentUserGestureToken::Create(&GetPage()
                                                ->GetPointerLockController()
                                                .GetElement()
                                                ->GetDocument(),
                                           UserGestureToken::kNewGesture)));
      pointer_lock_gesture_token_ = gesture_indicator->CurrentToken();
      break;
    case WebInputEvent::kMouseUp:
      event_type = EventTypeNames::mouseup;
      gesture_indicator = WTF::WrapUnique(
          new UserGestureIndicator(pointer_lock_gesture_token_.Release()));
      break;
    case WebInputEvent::kMouseMove:
      event_type = EventTypeNames::mousemove;
      break;
    default:
      NOTREACHED();
  }

  const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);

  if (GetPage()) {
    WebMouseEvent transformed_event = TransformWebMouseEvent(
        ToWebLocalFrameBase(LocalRoot())->GetFrameView(), mouse_event);
    GetPage()->GetPointerLockController().DispatchLockedMouseEvent(
        transformed_event, event_type);
  }
}

}  // namespace blink
