/*
 * Copyright 2008, The Android Open Source Project
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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

#ifndef TouchEvent_h
#define TouchEvent_h

#include "core/CoreExport.h"
#include "core/dom/TouchList.h"
#include "core/dom/events/EventDispatchMediator.h"
#include "core/events/TouchEventInit.h"
#include "core/events/UIEventWithKeyState.h"
#include "platform/graphics/TouchAction.h"
#include "public/platform/WebCoalescedInputEvent.h"
#include "public/platform/WebTouchEvent.h"

namespace blink {

class CORE_EXPORT TouchEvent final : public UIEventWithKeyState {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~TouchEvent() override;

  // We only initialize sourceCapabilities when we create TouchEvent from
  // EventHandler, null if it is from JavaScript.
  static TouchEvent* Create() { return new TouchEvent; }
  static TouchEvent* Create(const WebCoalescedInputEvent& event,
                            TouchList* touches,
                            TouchList* target_touches,
                            TouchList* changed_touches,
                            const AtomicString& type,
                            AbstractView* view,
                            TouchAction current_touch_action) {
    return new TouchEvent(event, touches, target_touches, changed_touches, type,
                          view, current_touch_action);
  }

  static TouchEvent* Create(const AtomicString& type,
                            const TouchEventInit& initializer) {
    return new TouchEvent(type, initializer);
  }

  TouchList* touches() const { return touches_.Get(); }
  TouchList* targetTouches() const { return target_touches_.Get(); }
  TouchList* changedTouches() const { return changed_touches_.Get(); }

  void SetTouches(TouchList* touches) { touches_ = touches; }
  void SetTargetTouches(TouchList* target_touches) {
    target_touches_ = target_touches;
  }
  void SetChangedTouches(TouchList* changed_touches) {
    changed_touches_ = changed_touches;
  }

  bool IsTouchEvent() const override;

  const AtomicString& InterfaceName() const override;

  void preventDefault() override;

  void DoneDispatchingEventAtCurrentTarget() override;

  EventDispatchMediator* CreateMediator() override;

  const WebCoalescedInputEvent* NativeEvent() const {
    return native_event_.get();
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  TouchEvent();
  TouchEvent(const WebCoalescedInputEvent&,
             TouchList* touches,
             TouchList* target_touches,
             TouchList* changed_touches,
             const AtomicString& type,
             AbstractView*,
             TouchAction current_touch_action);
  TouchEvent(const AtomicString&, const TouchEventInit&);
  bool IsTouchStartOrFirstTouchMove() const;

  Member<TouchList> touches_;
  Member<TouchList> target_touches_;
  Member<TouchList> changed_touches_;

  bool default_prevented_before_current_target_;

  // The current effective touch action computed before each
  // touchstart event is generated. It is used for UMA histograms.
  TouchAction current_touch_action_;

  std::unique_ptr<WebCoalescedInputEvent> native_event_;
};

class TouchEventDispatchMediator final : public EventDispatchMediator {
 public:
  static TouchEventDispatchMediator* Create(TouchEvent*);

 private:
  explicit TouchEventDispatchMediator(TouchEvent*);
  TouchEvent& Event() const;
  DispatchEventResult DispatchEvent(EventDispatcher&) const override;
};

DEFINE_EVENT_TYPE_CASTS(TouchEvent);

}  // namespace blink

#endif  // TouchEvent_h
