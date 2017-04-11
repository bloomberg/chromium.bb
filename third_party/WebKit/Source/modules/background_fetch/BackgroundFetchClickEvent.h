// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchClickEvent_h
#define BackgroundFetchClickEvent_h

#include "modules/ModulesExport.h"
#include "modules/background_fetch/BackgroundFetchEvent.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class BackgroundFetchClickEventInit;
class WaitUntilObserver;

class MODULES_EXPORT BackgroundFetchClickEvent final
    : public BackgroundFetchEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchClickEvent* Create(
      const AtomicString& type,
      const BackgroundFetchClickEventInit& initializer) {
    return new BackgroundFetchClickEvent(type, initializer,
                                         nullptr /* observer */);
  }

  static BackgroundFetchClickEvent* Create(
      const AtomicString& type,
      const BackgroundFetchClickEventInit& initializer,
      WaitUntilObserver* observer) {
    return new BackgroundFetchClickEvent(type, initializer, observer);
  }

  ~BackgroundFetchClickEvent() override;

  // Web Exposed attribute defined in the IDL file.
  AtomicString state() const;

  // ExtendableEvent interface.
  const AtomicString& InterfaceName() const override;

 private:
  BackgroundFetchClickEvent(const AtomicString& type,
                            const BackgroundFetchClickEventInit&,
                            WaitUntilObserver*);

  AtomicString state_;
};

}  // namespace blink

#endif  // BackgroundFetchClickEvent_h
