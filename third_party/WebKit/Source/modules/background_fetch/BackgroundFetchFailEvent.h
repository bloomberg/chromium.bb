// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchFailEvent_h
#define BackgroundFetchFailEvent_h

#include "modules/ModulesExport.h"
#include "modules/background_fetch/BackgroundFetchEvent.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/WebVector.h"

namespace blink {

class BackgroundFetchFailEventInit;
class BackgroundFetchSettledFetch;
class ScriptState;
struct WebBackgroundFetchSettledFetch;

class MODULES_EXPORT BackgroundFetchFailEvent final
    : public BackgroundFetchEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BackgroundFetchFailEvent* Create(
      const AtomicString& type,
      const BackgroundFetchFailEventInit& initializer) {
    return new BackgroundFetchFailEvent(type, initializer);
  }

  static BackgroundFetchFailEvent* Create(
      const AtomicString& type,
      const BackgroundFetchFailEventInit& initializer,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches,
      ScriptState* script_state,
      WaitUntilObserver* observer) {
    return new BackgroundFetchFailEvent(type, initializer, fetches,
                                        script_state, observer);
  }

  ~BackgroundFetchFailEvent() override;

  // Web Exposed attribute defined in the IDL file.
  HeapVector<Member<BackgroundFetchSettledFetch>> fetches() const;

  // ExtendableEvent interface.
  const AtomicString& InterfaceName() const override;

  virtual void Trace(blink::Visitor*);

 private:
  BackgroundFetchFailEvent(const AtomicString& type,
                           const BackgroundFetchFailEventInit&);
  BackgroundFetchFailEvent(
      const AtomicString& type,
      const BackgroundFetchFailEventInit&,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches,
      ScriptState*,
      WaitUntilObserver*);

  HeapVector<Member<BackgroundFetchSettledFetch>> fetches_;
};

}  // namespace blink

#endif  // BackgroundFetchFailEvent_h
