// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACING_IMPL_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACING_IMPL_H_

#include "base/macros.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/tracing/public/cpp/trace_provider_impl.h"
#include "services/tracing/public/interfaces/tracing.mojom.h"

namespace shell {
class Connection;
class Connector;
}

namespace mojo {

// Connects to mojo:tracing during your Service's OnStart() call once per
// process.
class TracingImpl : public shell::InterfaceFactory<tracing::TraceProvider> {
 public:
  TracingImpl();
  ~TracingImpl() override;

  // Connect to the Tracing service & provide tracing data on demand.
  void Initialize(shell::Connector* connector, const std::string& url);

 private:
  // InterfaceFactory<tracing::TraceProvider> implementation.
  void Create(const shell::Identity& remote_identity,
              tracing::TraceProviderRequest request) override;

  std::unique_ptr<shell::Connection> connection_;
  TraceProviderImpl provider_impl_;

  DISALLOW_COPY_AND_ASSIGN(TracingImpl);
};

}  // namespace mojo

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACING_IMPL_H_
