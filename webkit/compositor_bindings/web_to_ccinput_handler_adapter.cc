// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_to_ccinput_handler_adapter.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebInputHandlerClient.h"

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, cc_name) \
  COMPILE_ASSERT(static_cast<int>(WebKit::webkit_name) ==  \
                 static_cast<int>(cc::cc_name),            \
                 mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusOnMainThread,
                             InputHandler::ScrollOnMainThread);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusStarted,
                             InputHandler::ScrollStarted);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusIgnored,
                             InputHandler::ScrollIgnored);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollInputTypeGesture,
                             InputHandler::Gesture);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollInputTypeWheel,
                             InputHandler::Wheel);

namespace WebKit {

scoped_ptr<WebToCCInputHandlerAdapter> WebToCCInputHandlerAdapter::create(
    scoped_ptr<WebInputHandler> handler) {
  return scoped_ptr<WebToCCInputHandlerAdapter>(
      new WebToCCInputHandlerAdapter(handler.Pass()));
}

WebToCCInputHandlerAdapter::WebToCCInputHandlerAdapter(
    scoped_ptr<WebInputHandler> handler)
    : handler_(handler.Pass()) {}

WebToCCInputHandlerAdapter::~WebToCCInputHandlerAdapter() {}

class WebToCCInputHandlerAdapter::HandlerAdapter
    : public WebInputHandlerClient {
 public:
  explicit HandlerAdapter(cc::InputHandler* handler) : handler_(handler) {}

  virtual ~HandlerAdapter() {}

  virtual ScrollStatus scrollBegin(WebPoint point, ScrollInputType type) {
    return static_cast<WebInputHandlerClient::ScrollStatus>(
        handler_->ScrollBegin(
            point, static_cast<cc::InputHandler::ScrollInputType>(type)));
  }

  virtual bool scrollByIfPossible(WebPoint point, WebFloatSize delta) {
    return handler_->ScrollBy(point, delta);
  }

  virtual bool scrollVerticallyByPageIfPossible(
      WebPoint point, WebScrollbar::ScrollDirection direction) {
    return handler_->ScrollVerticallyByPage(point, direction);
  }

  virtual ScrollStatus flingScrollBegin() {
    return static_cast<WebInputHandlerClient::ScrollStatus>(
        handler_->FlingScrollBegin());
  }

  virtual void notifyCurrentFlingVelocity(WebFloatSize velocity) {
    handler_->NotifyCurrentFlingVelocity(velocity);
  }

  virtual void scrollEnd() { handler_->ScrollEnd(); }

  virtual void pinchGestureBegin() { handler_->PinchGestureBegin(); }

  virtual void pinchGestureUpdate(float magnify_delta, WebPoint anchor) {
    handler_->PinchGestureUpdate(magnify_delta, anchor);
  }

  virtual void pinchGestureEnd() { handler_->PinchGestureEnd(); }

  virtual void startPageScaleAnimation(WebSize target_position,
                                       bool anchor_point,
                                       float page_scale,
                                       double start_time_sec,
                                       double duration_sec) {
    base::TimeTicks start_time = base::TimeTicks::FromInternalValue(
        start_time_sec * base::Time::kMicrosecondsPerSecond);
    base::TimeDelta duration = base::TimeDelta::FromMicroseconds(
        duration_sec * base::Time::kMicrosecondsPerSecond);
    handler_->StartPageScaleAnimation(
        target_position, anchor_point, page_scale, start_time, duration);
  }

  virtual void scheduleAnimation() { handler_->ScheduleAnimation(); }

  virtual bool haveTouchEventHandlersAt(WebPoint point) {
    return handler_->HaveTouchEventHandlersAt(point);
  }

  virtual void didReceiveLastInputEventForVSync(double frame_time_sec)
      OVERRIDE {
    base::TimeTicks frame_time = base::TimeTicks::FromInternalValue(
        frame_time_sec * base::Time::kMicrosecondsPerSecond);
    handler_->DidReceiveLastInputEventForVSync(frame_time);
  }

 private:
  cc::InputHandler* handler_;
};

void WebToCCInputHandlerAdapter::BindToHandler(cc::InputHandler* handler) {
  handler_adapter_.reset(new HandlerAdapter(handler));
  handler_->bindToClient(handler_adapter_.get());
}

void WebToCCInputHandlerAdapter::Animate(base::TimeTicks time) {
  double monotonic_time_seconds = (time - base::TimeTicks()).InSecondsF();
  handler_->animate(monotonic_time_seconds);
}

void WebToCCInputHandlerAdapter::MainThreadHasStoppedFlinging() {
  handler_->mainThreadHasStoppedFlinging();
}

}  // namespace WebKit
