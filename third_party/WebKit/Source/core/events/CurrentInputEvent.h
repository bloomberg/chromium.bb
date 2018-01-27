// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CurrentInputEvent_h
#define CurrentInputEvent_h

namespace blink {

class WebFrameWidgetImpl;
class WebInputEvent;
class WebViewImpl;

class CurrentInputEvent {
 public:
  // Gets the "current" input event - event that is currently being processed by
  // either blink::WebViewImpl::HandleInputEventInternal or by
  // blink::WebFrameWidgetImpl::HandleInputEventInternal
  static const WebInputEvent* Get() { return current_input_event_; }

 private:
  friend class WebViewImpl;
  friend class WebFrameWidgetImpl;
  static const WebInputEvent* current_input_event_;
};

}  // namespace blink

#endif
