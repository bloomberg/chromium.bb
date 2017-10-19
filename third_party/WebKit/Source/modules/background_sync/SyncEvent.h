// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SyncEvent_h
#define SyncEvent_h

#include "modules/EventModules.h"
#include "modules/background_sync/SyncEventInit.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class MODULES_EXPORT SyncEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SyncEvent* Create(const AtomicString& type,
                           const String& tag,
                           bool last_chance,
                           WaitUntilObserver* observer) {
    return new SyncEvent(type, tag, last_chance, observer);
  }
  static SyncEvent* Create(const AtomicString& type,
                           const SyncEventInit& init) {
    return new SyncEvent(type, init);
  }

  ~SyncEvent() override;

  const AtomicString& InterfaceName() const override;

  String tag();
  bool lastChance();

  virtual void Trace(blink::Visitor*);

 private:
  SyncEvent(const AtomicString& type, const String&, bool, WaitUntilObserver*);
  SyncEvent(const AtomicString& type, const SyncEventInit&);

  String tag_;
  bool last_chance_;
};

}  // namespace blink

#endif  // SyncEvent_h
