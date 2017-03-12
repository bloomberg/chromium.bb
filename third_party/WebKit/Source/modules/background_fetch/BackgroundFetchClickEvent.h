// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchClickEvent_h
#define BackgroundFetchClickEvent_h

#include "modules/background_fetch/BackgroundFetchEvent.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class BackgroundFetchClickEventInit;

class BackgroundFetchClickEvent final : public BackgroundFetchEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchClickEvent* create(
      const AtomicString& type,
      const BackgroundFetchClickEventInit& initializer) {
    return new BackgroundFetchClickEvent(type, initializer);
  }

  ~BackgroundFetchClickEvent() override;

  // Web Exposed attribute defined in the IDL file.
  AtomicString state() const;

  // ExtendableEvent interface.
  const AtomicString& interfaceName() const override;

 private:
  BackgroundFetchClickEvent(const AtomicString& type,
                            const BackgroundFetchClickEventInit&);

  AtomicString m_state;
};

}  // namespace blink

#endif  // BackgroundFetchClickEvent_h
