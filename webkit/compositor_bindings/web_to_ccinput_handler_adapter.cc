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
                             InputHandlerClient::ScrollOnMainThread);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusStarted,
                             InputHandlerClient::ScrollStarted);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollStatusIgnored,
                             InputHandlerClient::ScrollIgnored);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollInputTypeGesture,
                             InputHandlerClient::Gesture);
COMPILE_ASSERT_MATCHING_ENUM(WebInputHandlerClient::ScrollInputTypeWheel,
                             InputHandlerClient::Wheel);

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

class WebToCCInputHandlerAdapter::ClientAdapter : public WebInputHandlerClient {
 public:
  explicit ClientAdapter(cc::InputHandlerClient* client) : client_(client) {}

  virtual ~ClientAdapter() {}

  virtual ScrollStatus scrollBegin(WebPoint point, ScrollInputType type) {
    return static_cast<WebInputHandlerClient::ScrollStatus>(
        client_->ScrollBegin(
            point, static_cast<cc::InputHandlerClient::ScrollInputType>(type)));
  }

  virtual bool scrollByIfPossible(WebPoint point, WebFloatSize delta) {
    return client_->ScrollBy(point, delta);
  }

  virtual bool scrollVerticallyByPageIfPossible(
      WebPoint point, WebScrollbar::ScrollDirection direction) {
    return client_->ScrollVerticallyByPage(point, direction);
  }

  virtual void scrollEnd() { client_->ScrollEnd(); }

  virtual void pinchGestureBegin() { client_->PinchGestureBegin(); }

  virtual void pinchGestureUpdate(float magnify_delta, WebPoint anchor) {
    client_->PinchGestureUpdate(magnify_delta, anchor);
  }

  virtual void pinchGestureEnd() { client_->PinchGestureEnd(); }

  virtual void startPageScaleAnimation(WebSize target_position,
                                       bool anchor_point,
                                       float page_scale,
                                       double start_time_sec,
                                       double duration_sec) {
    base::TimeTicks start_time = base::TimeTicks::FromInternalValue(
        start_time_sec * base::Time::kMicrosecondsPerSecond);
    base::TimeDelta duration = base::TimeDelta::FromMicroseconds(
        duration_sec * base::Time::kMicrosecondsPerSecond);
    client_->StartPageScaleAnimation(
        target_position, anchor_point, page_scale, start_time, duration);
  }

  virtual void scheduleAnimation() { client_->ScheduleAnimation(); }

  virtual bool haveTouchEventHandlersAt(WebPoint point) {
    return client_->HaveTouchEventHandlersAt(point);
  }

 private:
  cc::InputHandlerClient* client_;
};

void WebToCCInputHandlerAdapter::BindToClient(cc::InputHandlerClient* client) {
  client_adapter_.reset(new ClientAdapter(client));
  handler_->bindToClient(client_adapter_.get());
}

void WebToCCInputHandlerAdapter::Animate(base::TimeTicks time) {
  double monotonic_time_seconds = (time - base::TimeTicks()).InSecondsF();
  handler_->animate(monotonic_time_seconds);
}

void WebToCCInputHandlerAdapter::MainThreadHasStoppedFlinging() {
  handler_->mainThreadHasStoppedFlinging();
}

}  // namespace WebKit
