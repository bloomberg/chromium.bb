// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_impl.h"

#include <utility>

#include "discovery/mdns/public/mdns_service.h"

namespace openscreen {
namespace discovery {

// static
std::unique_ptr<DnsSdService> DnsSdService::Create(TaskRunner* task_runner) {
  return std::make_unique<ServiceImpl>(task_runner);
}

ServiceImpl::ServiceImpl(TaskRunner* task_runner)
    : mdns_service_(MdnsService::Create(task_runner)),
      querier_(mdns_service_.get()),
      publisher_(mdns_service_.get()) {}

ServiceImpl::~ServiceImpl() = default;

}  // namespace discovery
}  // namespace openscreen
