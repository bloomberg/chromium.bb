// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_to_ccinput_handler_adapter.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebInputHandlerClient.h"

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, cc_name)                     \
  COMPILE_ASSERT(int(WebKit::webkit_name) == int(cc::cc_name),                 \
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
  ClientAdapter(cc::InputHandlerClient* client) : client_(client) {}

  virtual ~ClientAdapter() {}

  virtual ScrollStatus scrollBegin(WebPoint point, ScrollInputType type)
      OVERRIDE {
    return static_cast<WebInputHandlerClient::ScrollStatus>(
        client_->scrollBegin(
            point, static_cast<cc::InputHandlerClient::ScrollInputType>(type)));
  }

  virtual bool scrollByIfPossible(WebPoint point, WebSize offset) OVERRIDE {
    return client_->scrollBy(point, offset);
  }

  virtual void scrollEnd() OVERRIDE { client_->scrollEnd(); }

  virtual void pinchGestureBegin() OVERRIDE { client_->pinchGestureBegin(); }

  virtual void pinchGestureUpdate(float magnify_delta, WebPoint anchor)
      OVERRIDE {
    client_->pinchGestureUpdate(magnify_delta, anchor);
  }

  virtual void pinchGestureEnd() OVERRIDE { client_->pinchGestureEnd(); }

  virtual void startPageScaleAnimation(WebSize target_position,
                                       bool anchor_point,
                                       float page_scale,
                                       double start_time_sec,
                                       double duration_sec) OVERRIDE {
    base::TimeTicks start_time = base::TimeTicks::FromInternalValue(
        start_time_sec * base::Time::kMicrosecondsPerSecond);
    base::TimeDelta duration = base::TimeDelta::FromMicroseconds(
        duration_sec * base::Time::kMicrosecondsPerSecond);
    client_->startPageScaleAnimation(
        target_position, anchor_point, page_scale, start_time, duration);
  }

  virtual void scheduleAnimation() OVERRIDE { client_->scheduleAnimation(); }

  virtual bool haveTouchEventHandlersAt(WebPoint point) {
    return client_->haveTouchEventHandlersAt(point);
  }

 private:
  cc::InputHandlerClient* client_;
};

void WebToCCInputHandlerAdapter::bindToClient(cc::InputHandlerClient* client) {
  client_adapter_.reset(new ClientAdapter(client));
  handler_->bindToClient(client_adapter_.get());
}

void WebToCCInputHandlerAdapter::animate(base::TimeTicks time) {
  double monotonic_time_seconds = (time - base::TimeTicks()).InSecondsF();
  handler_->animate(monotonic_time_seconds);
}

void WebToCCInputHandlerAdapter::mainThreadHasStoppedFlinging() {
  handler_->mainThreadHasStoppedFlinging();
}

}  // namespace WebKit
