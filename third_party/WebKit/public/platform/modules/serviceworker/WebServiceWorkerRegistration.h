// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerRegistration_h
#define WebServiceWorkerRegistration_h

#include <memory>

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebURL.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/common/service_worker/service_worker_registration.mojom-shared.h"

namespace blink {

class WebServiceWorkerRegistrationProxy;
struct WebNavigationPreloadState;

// The interface of the registration representation in the embedder. The
// embedder implements this interface and passes its handle
// (WebServiceWorkerRegistration::Handle) to Blink. Blink accesses the
// implementation via the handle to update or unregister the registration.
class WebServiceWorkerRegistration {
 public:
  virtual ~WebServiceWorkerRegistration() = default;

  using WebServiceWorkerUpdateCallbacks =
      WebCallbacks<void, const WebServiceWorkerError&>;
  using WebServiceWorkerUnregistrationCallbacks =
      WebCallbacks<bool, const WebServiceWorkerError&>;
  using WebEnableNavigationPreloadCallbacks =
      WebCallbacks<void, const WebServiceWorkerError&>;
  using WebGetNavigationPreloadStateCallbacks =
      WebCallbacks<const WebNavigationPreloadState&,
                   const WebServiceWorkerError&>;
  using WebSetNavigationPreloadHeaderCallbacks =
      WebCallbacks<void, const WebServiceWorkerError&>;

  // The handle interface that retains a reference to the implementation of
  // WebServiceWorkerRegistration in the embedder and is owned by
  // ServiceWorkerRegistration object in Blink. The embedder must keep the
  // registration representation while Blink is owning this handle.
  class Handle {
   public:
    virtual ~Handle() = default;
    virtual WebServiceWorkerRegistration* Registration() { return nullptr; }
  };

  virtual void SetProxy(WebServiceWorkerRegistrationProxy*) {}
  virtual WebServiceWorkerRegistrationProxy* Proxy() { return nullptr; }
  virtual void ProxyStopped() {}

  virtual WebURL Scope() const { return WebURL(); }
  // TODO(crbug.com/675540): Make this pure virtual once
  // implemented in derived classes.
  virtual mojom::ServiceWorkerUpdateViaCache UpdateViaCache() const {
    return mojom::ServiceWorkerUpdateViaCache::kImports;
  }
  virtual int64_t RegistrationId() const = 0;
  virtual void Update(std::unique_ptr<WebServiceWorkerUpdateCallbacks>) {}
  virtual void Unregister(
      std::unique_ptr<WebServiceWorkerUnregistrationCallbacks>) {}

  virtual void EnableNavigationPreload(
      bool enable,
      std::unique_ptr<WebEnableNavigationPreloadCallbacks>) {}
  virtual void GetNavigationPreloadState(
      std::unique_ptr<WebGetNavigationPreloadStateCallbacks>) {}
  virtual void SetNavigationPreloadHeader(
      const WebString& value,
      std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks>) {}
};

}  // namespace blink

#endif  // WebServiceWorkerRegistration_h
