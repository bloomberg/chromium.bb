// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchEvent_h
#define BackgroundFetchEvent_h

#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class BackgroundFetchEventInit;
class WaitUntilObserver;

class MODULES_EXPORT BackgroundFetchEvent : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchEvent* Create(
      const AtomicString& type,
      const BackgroundFetchEventInit& initializer) {
    return new BackgroundFetchEvent(type, initializer, nullptr /* observer */);
  }

  static BackgroundFetchEvent* Create(
      const AtomicString& type,
      const BackgroundFetchEventInit& initializer,
      WaitUntilObserver* observer) {
    return new BackgroundFetchEvent(type, initializer, observer);
  }

  ~BackgroundFetchEvent() override;

  // Web Exposed attribute defined in the IDL file. Corresponds to the
  // |developer_id| used elsewhere in the codebase.
  String id() const;

  // ExtendableEvent interface.
  const AtomicString& InterfaceName() const override;

 protected:
  BackgroundFetchEvent(const AtomicString& type,
                       const BackgroundFetchEventInit&,
                       WaitUntilObserver*);

  // Corresponds to IDL 'id' attribute. Not unique - an active registration can
  // have the same |developer_id_| as one or more inactive registrations.
  String developer_id_;
};

}  // namespace blink

#endif  // BackgroundFetchEvent_h
