// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/test/test_service_manager.h"

#include "services/service_manager/background/background_service_manager.h"
#include "services/service_manager/service_manager.h"

namespace service_manager {

TestServiceManager::TestServiceManager() : TestServiceManager(nullptr) {}

TestServiceManager::TestServiceManager(std::unique_ptr<base::Value> catalog)
    : background_service_manager_(
          std::make_unique<BackgroundServiceManager>(nullptr,
                                                     std::move(catalog))) {}

TestServiceManager::~TestServiceManager() = default;

mojom::ServiceRequest TestServiceManager::RegisterTestInstance(
    const std::string& service_name) {
  return RegisterInstance(Identity{service_name, base::Token::CreateRandom(),
                                   base::Token{}, base::Token::CreateRandom()});
}

mojom::ServiceRequest TestServiceManager::RegisterInstance(
    const Identity& identity) {
  mojom::ServicePtr service;
  mojom::ServiceRequest request = mojo::MakeRequest(&service);
  background_service_manager_->RegisterService(identity, std::move(service),
                                               nullptr);
  return request;
}

}  // namespace service_manager
