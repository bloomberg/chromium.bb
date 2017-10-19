// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushEvent_h
#define PushEvent_h

#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/push_messaging/PushMessageData.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PushEventInit;

class MODULES_EXPORT PushEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PushEvent* Create(const AtomicString& type,
                           PushMessageData* data,
                           WaitUntilObserver* observer) {
    return new PushEvent(type, data, observer);
  }
  static PushEvent* Create(const AtomicString& type,
                           const PushEventInit& initializer) {
    return new PushEvent(type, initializer);
  }

  ~PushEvent() override;

  // ExtendableEvent interface.
  const AtomicString& InterfaceName() const override;

  PushMessageData* data();

  virtual void Trace(blink::Visitor*);

 private:
  PushEvent(const AtomicString& type, PushMessageData*, WaitUntilObserver*);
  PushEvent(const AtomicString& type, const PushEventInit&);

  Member<PushMessageData> data_;
};

}  // namespace blink

#endif  // PushEvent_h
