// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchRegistration_h
#define BackgroundFetchRegistration_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/events/EventTarget.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/background_fetch/background_fetch.mojom-blink.h"

namespace blink {

class IconDefinition;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;

// Represents an individual Background Fetch registration. Gives developers
// access to its properties, options, and enables them to abort the fetch.
class BackgroundFetchRegistration final : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BackgroundFetchRegistration(String id,
                              unsigned long long upload_total,
                              unsigned long long uploaded,
                              unsigned long long download_total,
                              unsigned long long downloaded,
                              HeapVector<IconDefinition> icons,
                              String title);
  ~BackgroundFetchRegistration();

  // Sets the ServiceWorkerRegistration that this BackgroundFetchRegistration
  // has been associated with.
  void SetServiceWorkerRegistration(ServiceWorkerRegistration*);

  String id() const;
  unsigned long long uploadTotal() const;
  unsigned long long uploaded() const;
  unsigned long long downloadTotal() const;
  unsigned long long downloaded() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(progress);

  ScriptPromise abort(ScriptState*);

  // TODO(crbug.com/769770): Remove the following deprecated attributes.
  HeapVector<IconDefinition> icons() const;
  String title() const;

  // EventTargetWithInlineData implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  void DidAbort(ScriptPromiseResolver*, mojom::blink::BackgroundFetchError);

  Member<ServiceWorkerRegistration> registration_;

  String id_;
  unsigned long long upload_total_;
  unsigned long long uploaded_;
  unsigned long long download_total_;
  unsigned long long downloaded_;
  HeapVector<IconDefinition> icons_;
  String title_;
};

}  // namespace blink

#endif  // BackgroundFetchRegistration_h
