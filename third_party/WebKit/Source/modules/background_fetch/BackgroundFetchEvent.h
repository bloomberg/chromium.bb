// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchEvent_h
#define BackgroundFetchEvent_h

#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class BackgroundFetchEventInit;
class WaitUntilObserver;

class MODULES_EXPORT BackgroundFetchEvent : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchEvent* create(
      const AtomicString& type,
      const BackgroundFetchEventInit& initializer) {
    return new BackgroundFetchEvent(type, initializer, nullptr /* observer */);
  }

  static BackgroundFetchEvent* create(
      const AtomicString& type,
      const BackgroundFetchEventInit& initializer,
      WaitUntilObserver* observer) {
    return new BackgroundFetchEvent(type, initializer, observer);
  }

  ~BackgroundFetchEvent() override;

  // Web Exposed attribute defined in the IDL file.
  String tag() const;

  // ExtendableEvent interface.
  const AtomicString& interfaceName() const override;

 protected:
  BackgroundFetchEvent(const AtomicString& type,
                       const BackgroundFetchEventInit&,
                       WaitUntilObserver*);

  String m_tag;
};

}  // namespace blink

#endif  // BackgroundFetchEvent_h
