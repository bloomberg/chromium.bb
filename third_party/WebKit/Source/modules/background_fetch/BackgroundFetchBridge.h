// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundFetchBridge_h
#define BackgroundFetchBridge_h

#include <memory>
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/background_fetch/background_fetch.mojom-blink.h"
#include "wtf/Functional.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BackgroundFetchRegistration;

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
  using GetRegistrationCallback =
      Function<void(mojom::blink::BackgroundFetchError,
                    BackgroundFetchRegistration*)>;
  using GetTagsCallback =
      Function<void(mojom::blink::BackgroundFetchError, const Vector<String>&)>;
  using UpdateUICallback = Function<void(mojom::blink::BackgroundFetchError)>;

  static BackgroundFetchBridge* from(ServiceWorkerRegistration*);
  static const char* supplementName();

  virtual ~BackgroundFetchBridge();

  // TODO(peter): Implement support for the `fetch()` function in the bridge.

  // Updates the user interface for the Background Fetch identified by |tag|
  // with the updated |title|. Will invoke the |callback| when the interface
  // has been requested to update.
  void updateUI(const String& tag,
                const String& title,
                std::unique_ptr<UpdateUICallback>);

  // Aborts the active Background Fetch for |tag|. Will invoke the |callback|
  // when the Background Fetch identified by |tag| has been aborted, or could
  // not be aborted for operational reasons.
  void abort(const String& tag, std::unique_ptr<AbortCallback>);

  // Gets the Background Fetch registration for the given |tag|. Will invoke the
  // |callback| with the Background Fetch registration, which may be a nullptr
  // if the |tag| does not exist, when the Mojo call has completed.
  void getRegistration(const String& tag,
                       std::unique_ptr<GetRegistrationCallback>);

  // Gets the sequence of tags for active Background Fetch registrations. Will
  // invoke the |callback| with the tags when the Mojo call has completed.
  void getTags(std::unique_ptr<GetTagsCallback>);

 private:
  explicit BackgroundFetchBridge(ServiceWorkerRegistration&);

  // Returns an initialized BackgroundFetchServicePtr. A connection will be
  // established after the first call to this method.
  mojom::blink::BackgroundFetchServicePtr& getService();

  void didGetRegistration(std::unique_ptr<GetRegistrationCallback>,
                          mojom::blink::BackgroundFetchError,
                          mojom::blink::BackgroundFetchRegistrationPtr);

  mojom::blink::BackgroundFetchServicePtr m_backgroundFetchService;
};

}  // namespace blink

#endif  // BackgroundFetchBridge_h
