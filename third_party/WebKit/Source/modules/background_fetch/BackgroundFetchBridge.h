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
  static const char kSupplementName[];

  using AbortCallback =
      base::OnceCallback<void(mojom::blink::BackgroundFetchError)>;
  using GetDeveloperIdsCallback =
      base::OnceCallback<void(mojom::blink::BackgroundFetchError,
                              const Vector<String>&)>;
  using RegistrationCallback =
      base::OnceCallback<void(mojom::blink::BackgroundFetchError,
                              BackgroundFetchRegistration*)>;
  using GetIconDisplaySizeCallback = base::OnceCallback<void(const WebSize&)>;
  using UpdateUICallback =
      base::OnceCallback<void(mojom::blink::BackgroundFetchError)>;

  static BackgroundFetchBridge* From(ServiceWorkerRegistration*);

  virtual ~BackgroundFetchBridge();

  // Creates a new Background Fetch registration identified by |developer_id|
  // for the sequence of |requests|. The |callback| will be invoked when the
  // registration has been created.
  void Fetch(const String& developer_id,
             Vector<WebServiceWorkerRequest> requests,
             mojom::blink::BackgroundFetchOptionsPtr,
             const SkBitmap& icon,
             RegistrationCallback);

  // Gets the size of the icon to be displayed in Background Fetch UI.
  void GetIconDisplaySize(GetIconDisplaySizeCallback);

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

  // Registers the |observer| to receive progress events for the background
  // fetch registration identified by the |unique_id|.
  void AddRegistrationObserver(
      const String& unique_id,
      mojom::blink::BackgroundFetchRegistrationObserverPtr);

 private:
  explicit BackgroundFetchBridge(ServiceWorkerRegistration&);

  // Returns an initialized BackgroundFetchService*. A connection will be
  // established after the first call to this method.
  mojom::blink::BackgroundFetchService* GetService();

  void DidGetRegistration(RegistrationCallback,
                          mojom::blink::BackgroundFetchError,
                          mojom::blink::BackgroundFetchRegistrationPtr);

  mojom::blink::BackgroundFetchServicePtr background_fetch_service_;
};

}  // namespace blink

#endif  // BackgroundFetchBridge_h
