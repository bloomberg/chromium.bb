// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchBridge_h
#define BackgroundFetchBridge_h

#include <memory>
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/background_fetch/background_fetch.mojom-blink.h"

namespace blink {

class BackgroundFetchOptions;
class BackgroundFetchRegistration;
class WebServiceWorkerRequest;

// The bridge is responsible for establishing and maintaining the Mojo
// connection to the BackgroundFetchService. It's keyed on an active Service
// Worker Registration.
class BackgroundFetchBridge final
    : public GarbageCollectedFinalized<BackgroundFetchBridge>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(BackgroundFetchBridge);
  WTF_MAKE_NONCOPYABLE(BackgroundFetchBridge);

 public:
  using AbortCallback = Function<void(mojom::blink::BackgroundFetchError)>;
  using GetDeveloperIdsCallback =
      Function<void(mojom::blink::BackgroundFetchError, const Vector<String>&)>;
  using RegistrationCallback = Function<void(mojom::blink::BackgroundFetchError,
                                             BackgroundFetchRegistration*)>;
  using UpdateUICallback = Function<void(mojom::blink::BackgroundFetchError)>;

  static BackgroundFetchBridge* From(ServiceWorkerRegistration*);
  static const char* SupplementName();

  virtual ~BackgroundFetchBridge();

  // Creates a new Background Fetch registration identified by |developer_id|
  // with the given |options| for the sequence of |requests|. The |callback|
  // will be invoked when the registration has been created.
  void Fetch(const String& developer_id,
             Vector<WebServiceWorkerRequest> requests,
             const BackgroundFetchOptions&,
             RegistrationCallback);

  // Updates the user interface for the Background Fetch identified by
  // |unique_id| with the updated |title|. Will invoke the |callback| when the
  // interface has been requested to update.
  void UpdateUI(const String& developer_id,
                const String& unique_id,
                const String& title,
                UpdateUICallback);

  // Aborts the active Background Fetch for |unique_id|. Will invoke the
  // |callback| when the Background Fetch identified by |unique_id| has been
  // aborted, or could not be aborted for operational reasons.
  void Abort(const String& developer_id,
             const String& unique_id,
             AbortCallback);

  // Gets the Background Fetch registration for the given |developer_id|. Will
  // invoke the |callback| with the Background Fetch registration, which may be
  // a nullptr if the |developer_id| does not exist, when the Mojo call has
  // completed.
  void GetRegistration(const String& developer_id, RegistrationCallback);

  // Gets the sequence of ids for active Background Fetch registrations. Will
  // invoke the |callback| with the |developers_id|s when the Mojo call has
  // completed.
  void GetDeveloperIds(GetDeveloperIdsCallback);

 private:
  explicit BackgroundFetchBridge(ServiceWorkerRegistration&);

  // Returns the security origin for the Service Worker registration this bridge
  // is servicing, which is to be included in the Mojo calls.
  SecurityOrigin* GetSecurityOrigin();

  // Returns an initialized BackgroundFetchServicePtr. A connection will be
  // established after the first call to this method.
  mojom::blink::BackgroundFetchServicePtr& GetService();

  void DidGetRegistration(RegistrationCallback,
                          mojom::blink::BackgroundFetchError,
                          mojom::blink::BackgroundFetchRegistrationPtr);

  mojom::blink::BackgroundFetchServicePtr background_fetch_service_;
};

}  // namespace blink

#endif  // BackgroundFetchBridge_h
