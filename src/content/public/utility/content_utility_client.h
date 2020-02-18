// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_
#define CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace content {

// Embedder API for participating in utility process logic.
class CONTENT_EXPORT ContentUtilityClient {
 public:
  virtual ~ContentUtilityClient() {}

  // Notifies us that the UtilityThread has been created.
  virtual void UtilityThreadStarted() {}

  // Allows the embedder to filter messages.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Allows the embedder to handle an incoming service request. If this is
  // called, this utility process was started for the sole purpose of running
  // the service identified by |service_name|.
  //
  // The embedder should return |true| to indicate that |request| has been
  // handled by running the expected service. It is the embedder's
  // responsibility to ensure that this utility process exits (see
  // |UtilityThread::ReleaseProcess()|) once the running service terminates.
  //
  // If the embedder returns |false| this process is terminated immediately.
  virtual bool HandleServiceRequest(
      const std::string& service_name,
      service_manager::mojom::ServiceRequest request);

  // Allows the embedder to handle an incoming service interface request to run
  // a service on the IO thread. |*receiver| is always valid when this called,
  // and the embedder is free to take ownership if handling the request. If the
  // embedder does not wish to handle this request on the I/O thread, it must
  // not modify |*receiver|.
  virtual void RunIOThreadService(mojo::GenericPendingReceiver* receiver);

  // Allows the embedder to handle an incoming service interface request to run
  // a service on the main thread. |receiver| is always valid when this called.
  virtual void RunMainThreadService(mojo::GenericPendingReceiver receiver);

  virtual void RegisterNetworkBinders(
      service_manager::BinderRegistry* registry) {}

  virtual void RegisterAudioBinders(service_manager::BinderMap* binders) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_
