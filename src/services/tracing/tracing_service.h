// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_TRACING_SERVICE_H_
#define SERVICES_TRACING_TRACING_SERVICE_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"

namespace tracing {

class TracingService : public mojom::TracingService {
 public:
  TracingService();
  explicit TracingService(
      mojo::PendingReceiver<mojom::TracingService> receiver);
  TracingService(const TracingService&) = delete;
  ~TracingService() override;
  TracingService& operator=(const TracingService&) = delete;

  // mojom::TracingService implementation:
  void Initialize(std::vector<mojom::ClientInfoPtr> clients) override;
  void AddClient(mojom::ClientInfoPtr client) override;
#if !defined(OS_NACL) && !defined(OS_IOS)
  void BindConsumerHost(
      mojo::PendingReceiver<mojom::ConsumerHost> receiver) override;
#endif

 private:
  mojo::Receiver<mojom::TracingService> receiver_{this};
};

}  // namespace tracing

#endif  // SERVICES_TRACING_TRACING_SERVICE_H_
