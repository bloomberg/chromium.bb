// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchFailEvent_h
#define BackgroundFetchFailEvent_h

#include "modules/background_fetch/BackgroundFetchEvent.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class BackgroundFetchFailEventInit;

class BackgroundFetchFailEvent final : public BackgroundFetchEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchFailEvent* create(
      const AtomicString& type,
      const BackgroundFetchFailEventInit& initializer) {
    return new BackgroundFetchFailEvent(type, initializer);
  }

  ~BackgroundFetchFailEvent() override;

  // ExtendableEvent interface.
  const AtomicString& interfaceName() const override;

 private:
  BackgroundFetchFailEvent(const AtomicString& type,
                           const BackgroundFetchFailEventInit&);
};

}  // namespace blink

#endif  // BackgroundFetchFailEvent_h
