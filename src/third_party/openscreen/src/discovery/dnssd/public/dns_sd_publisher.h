// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_DNS_SD_PUBLISHER_H_
#define DISCOVERY_DNSSD_PUBLIC_DNS_SD_PUBLISHER_H_

#include "absl/strings/string_view.h"
#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

class DnsSdPublisher {
 public:
  virtual ~DnsSdPublisher() = default;

  // Publishes the PTR, SRV, TXT, A, and AAAA records provided in the
  // DnsSdInstanceRecord. If the record already exists, an error with code
  // kItemAlreadyExists is returned. On success, Error::None is returned.
  // NOTE: Some embedders may return errors on other conditions (for instance,
  // android will return an error if the resulting TXT record has values not
  // encodable with UTF8).
  virtual Error Register(const DnsSdInstanceRecord& record) = 0;

  // Unpublishes any PTR, SRV, TXT, A, and AAAA records associated with this
  // service id. If no such records are published, this operation will be a
  // no-op. Returns the number of records which were removed.
  virtual size_t DeregisterAll(absl::string_view service) = 0;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_DNS_SD_PUBLISHER_H_
