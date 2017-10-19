// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientation_h
#define ScreenOrientation_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace blink {

class ExecutionContext;
class LocalFrame;
class ScriptPromise;
class ScriptState;
class ScreenOrientationControllerImpl;

class ScreenOrientation final : public EventTargetWithInlineData,
                                public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientation);

 public:
  static ScreenOrientation* Create(LocalFrame*);

  ~ScreenOrientation() override;

  // EventTarget implementation.
  const WTF::AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  String type() const;
  unsigned short angle() const;

  void SetType(WebScreenOrientationType);
  void SetAngle(unsigned short);

  ScriptPromise lock(ScriptState*, const AtomicString& orientation);
  void unlock();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

  // Helper being used by this class and LockOrientationCallback.
  static const AtomicString& OrientationTypeToString(WebScreenOrientationType);

  virtual void Trace(blink::Visitor*);

 private:
  explicit ScreenOrientation(LocalFrame*);

  ScreenOrientationControllerImpl* Controller();

  WebScreenOrientationType type_;
  unsigned short angle_;
};

}  // namespace blink

#endif  // ScreenOrientation_h
