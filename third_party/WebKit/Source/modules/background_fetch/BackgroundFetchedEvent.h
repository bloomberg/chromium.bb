// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchedEvent_h
#define BackgroundFetchedEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/background_fetch/BackgroundFetchEvent.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class BackgroundFetchedEventInit;

class BackgroundFetchedEvent final : public BackgroundFetchEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchedEvent* create(
      const AtomicString& type,
      const BackgroundFetchedEventInit& initializer) {
    return new BackgroundFetchedEvent(type, initializer);
  }

  ~BackgroundFetchedEvent() override;

  // Web Exposed method defined in the IDL file.
  ScriptPromise updateUI(ScriptState*, String title);

  // ExtendableEvent interface.
  const AtomicString& interfaceName() const override;

 private:
  BackgroundFetchedEvent(const AtomicString& type,
                         const BackgroundFetchedEventInit&);
};

}  // namespace blink

#endif  // BackgroundFetchedEvent_h
