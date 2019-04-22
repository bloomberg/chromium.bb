// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_engine_factory_registry.h"

#include <utility>

namespace ash {

ImeEngineFactoryRegistry::ImeEngineFactoryRegistry() = default;

ImeEngineFactoryRegistry::~ImeEngineFactoryRegistry() = default;

void ImeEngineFactoryRegistry::BindRequest(
    ime::mojom::ImeEngineFactoryRegistryRequest request) {
  registry_bindings_.AddBinding(this, std::move(request));
}

void ImeEngineFactoryRegistry::ConnectToImeEngine(
    ime::mojom::ImeEngineRequest engine_request,
    ime::mojom::ImeEngineClientPtr client) {
  if (engine_factory_) {
    engine_factory_->CreateEngine(std::move(engine_request), std::move(client));
  } else {
    pending_engine_request_ = std::move(engine_request);
    pending_engine_client_ = std::move(client);
  }
}

void ImeEngineFactoryRegistry::ActivateFactory(
    ime::mojom::ImeEngineFactoryPtr ief) {
  engine_factory_ = std::move(ief);
  if (pending_engine_request_) {
    engine_factory_->CreateEngine(std::move(pending_engine_request_),
                                  std::move(pending_engine_client_));
  }
}

}  // namespace ash
