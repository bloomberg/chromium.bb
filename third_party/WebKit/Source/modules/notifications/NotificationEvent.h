// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationEvent_h
#define NotificationEvent_h

#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/notifications/Notification.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"

namespace blink {

class NotificationEventInit;

class MODULES_EXPORT NotificationEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NotificationEvent* Create(const AtomicString& type,
                                   const NotificationEventInit& initializer) {
    return new NotificationEvent(type, initializer);
  }
  static NotificationEvent* Create(const AtomicString& type,
                                   const NotificationEventInit& initializer,
                                   WaitUntilObserver* observer) {
    return new NotificationEvent(type, initializer, observer);
  }

  ~NotificationEvent() override;

  Notification* getNotification() const { return notification_.Get(); }
  String action() const { return action_; }
  String reply() const { return reply_; }

  // ExtendableEvent interface.
  const AtomicString& InterfaceName() const override;

  virtual void Trace(blink::Visitor*);

 private:
  NotificationEvent(const AtomicString& type, const NotificationEventInit&);
  NotificationEvent(const AtomicString& type,
                    const NotificationEventInit&,
                    WaitUntilObserver*);

  Member<Notification> notification_;
  String action_;
  String reply_;
};

}  // namespace blink

#endif  // NotificationEvent_h
