// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_SERVICE_IMPL_H_
#define DISCOVERY_DNSSD_IMPL_SERVICE_IMPL_H_

#include <memory>

#include "discovery/dnssd/impl/publisher_impl.h"
#include "discovery/dnssd/impl/querier_impl.h"
#include "discovery/dnssd/public/dns_sd_service.h"

namespace openscreen {
namespace platform {

class TaskRunner;

}

namespace discovery {

class MdnsService;

class ServiceImpl final : public DnsSdService {
 public:
  explicit ServiceImpl(TaskRunner* task_runner);
  ~ServiceImpl() override;

  // DnsSdService overrides.
  DnsSdQuerier* Querier() override { return &querier_; }
  DnsSdPublisher* Publisher() override { return &publisher_; }

 private:
  std::unique_ptr<MdnsService> mdns_service_;

  QuerierImpl querier_;
  PublisherImpl publisher_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_SERVICE_IMPL_H_
