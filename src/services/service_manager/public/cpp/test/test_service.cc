// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/test/test_service.h"

#include "services/service_manager/public/mojom/connector.mojom.h"

namespace service_manager {

TestService::TestService(mojom::ServiceRequest request)
    : binding_(this, std::move(request)) {
  // Run until we have a functioning Connector end-to-end, in case e.g. the test
  // wants to making blocking calls on interfaces right away.
  mojom::ConnectorPtr flushing_connector;
  binding_.GetConnector()->BindConnectorRequest(
      mojo::MakeRequest(&flushing_connector));
  flushing_connector.FlushForTesting();
}

TestService::~TestService() = default;

}  // namespace service_manager
