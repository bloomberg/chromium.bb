// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_PUBLISHER_IMPL_H_
#define DISCOVERY_DNSSD_IMPL_PUBLISHER_IMPL_H_

#include "absl/strings/string_view.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "discovery/dnssd/public/dns_sd_publisher.h"
#include "discovery/mdns/public/mdns_service.h"

namespace openscreen {
namespace discovery {

class PublisherImpl : public DnsSdPublisher {
 public:
  PublisherImpl(MdnsService* publisher);
  ~PublisherImpl() override;

  // DnsSdPublisher overrides.
  Error Register(const DnsSdInstanceRecord& record) override;
  size_t DeregisterAll(absl::string_view service) override;

 private:
  std::vector<DnsSdInstanceRecord> published_records_;

  MdnsService* const mdns_publisher_;

  friend class PublisherTesting;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_PUBLISHER_IMPL_H_
