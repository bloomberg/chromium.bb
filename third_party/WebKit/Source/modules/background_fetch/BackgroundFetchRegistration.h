// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchRegistration_h
#define BackgroundFetchRegistration_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/events/EventTarget.h"
#include "mojo/public/cpp/bindings/binding.h"
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
class BackgroundFetchRegistration final
    : public EventTargetWithInlineData,
      public blink::mojom::blink::BackgroundFetchRegistrationObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(BackgroundFetchRegistration, Dispose);

 public:
  BackgroundFetchRegistration(const String& developer_id,
                              const String& unique_id,
                              unsigned long long upload_total,
                              unsigned long long uploaded,
                              unsigned long long download_total,
                              unsigned long long downloaded,
                              HeapVector<IconDefinition> icons,
                              const String& title);
  ~BackgroundFetchRegistration();

  // Initializes the BackgroundFetchRegistration to be associated with the given
  // ServiceWorkerRegistration. It will register itself as an observer for
  // progress events, powering the `progress` JavaScript event.
  void Initialize(ServiceWorkerRegistration*);

  // BackgroundFetchRegistrationObserver implementation.
  void OnProgress(uint64_t upload_total,
                  uint64_t uploaded,
                  uint64_t download_total,
                  uint64_t downloaded) override;

  // Web Exposed attribute defined in the IDL file. Corresponds to the
  // |developer_id| used elsewhere in the codebase.
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

  void Dispose();

  DECLARE_VIRTUAL_TRACE();

 private:
  void DidAbort(ScriptPromiseResolver*, mojom::blink::BackgroundFetchError);

  Member<ServiceWorkerRegistration> registration_;

  // Corresponds to IDL 'id' attribute. Not unique - an active registration can
  // have the same |developer_id_| as one or more inactive registrations.
  String developer_id_;

  // Globally unique ID for the registration, generated in content/. Used to
  // distinguish registrations in case a developer re-uses |developer_id_|s. Not
  // exposed to JavaScript.
  String unique_id_;

  unsigned long long upload_total_;
  unsigned long long uploaded_;
  unsigned long long download_total_;
  unsigned long long downloaded_;
  HeapVector<IconDefinition> icons_;
  String title_;

  mojo::Binding<blink::mojom::blink::BackgroundFetchRegistrationObserver>
      observer_binding_;
};

}  // namespace blink

#endif  // BackgroundFetchRegistration_h
