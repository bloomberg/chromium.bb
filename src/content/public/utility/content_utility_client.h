// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_
#define CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "content/public/common/content_client.h"
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

  virtual void RegisterNetworkBinders(
      service_manager::BinderRegistry* registry) {}

  virtual void RegisterAudioBinders(service_manager::BinderRegistry* registry) {
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_
