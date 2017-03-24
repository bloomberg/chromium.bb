// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchFailEvent_h
#define BackgroundFetchFailEvent_h

#include "modules/ModulesExport.h"
#include "modules/background_fetch/BackgroundFetchEvent.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebVector.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class BackgroundFetchFailEventInit;
class BackgroundFetchSettledFetch;
class ScriptState;
struct WebBackgroundFetchSettledFetch;

class MODULES_EXPORT BackgroundFetchFailEvent final
    : public BackgroundFetchEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchFailEvent* create(
      const AtomicString& type,
      const BackgroundFetchFailEventInit& initializer) {
    return new BackgroundFetchFailEvent(type, initializer);
  }

  static BackgroundFetchFailEvent* create(
      const AtomicString& type,
      const BackgroundFetchFailEventInit& initializer,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches,
      ScriptState* scriptState,
      WaitUntilObserver* observer) {
    return new BackgroundFetchFailEvent(type, initializer, fetches, scriptState,
                                        observer);
  }

  ~BackgroundFetchFailEvent() override;

  // Web Exposed attribute defined in the IDL file.
  HeapVector<Member<BackgroundFetchSettledFetch>> fetches() const;

  // ExtendableEvent interface.
  const AtomicString& interfaceName() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  BackgroundFetchFailEvent(const AtomicString& type,
                           const BackgroundFetchFailEventInit&);
  BackgroundFetchFailEvent(
      const AtomicString& type,
      const BackgroundFetchFailEventInit&,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches,
      ScriptState*,
      WaitUntilObserver*);

  HeapVector<Member<BackgroundFetchSettledFetch>> m_fetches;
};

}  // namespace blink

#endif  // BackgroundFetchFailEvent_h
