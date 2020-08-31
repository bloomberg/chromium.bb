// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_DNS_SD_INSTANCE_ENDPOINT_H_
#define DISCOVERY_DNSSD_PUBLIC_DNS_SD_INSTANCE_ENDPOINT_H_

#include <string>

#include "discovery/dnssd/public/dns_sd_instance.h"
#include "discovery/dnssd/public/dns_sd_txt_record.h"
#include "platform/base/interface_info.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace discovery {

// Represents the data stored in DNS records of types SRV, TXT, A, and AAAA
class DnsSdInstanceEndpoint : public DnsSdInstance {
 public:
  // These ctors expect valid input, and will cause a crash if they are not.
  DnsSdInstanceEndpoint(std::string instance_id,
                        std::string service_id,
                        std::string domain_id,
                        DnsSdTxtRecord txt,
                        IPEndpoint endpoint,
                        NetworkInterfaceIndex network_interface);
  DnsSdInstanceEndpoint(DnsSdInstance record,
                        IPAddress address,
                        NetworkInterfaceIndex network_interface);

  // NOTE: These constructors expects one endpoint to be an IPv4 address and the
  // other to be an IPv6 address.
  DnsSdInstanceEndpoint(std::string instance_id,
                        std::string service_id,
                        std::string domain_id,
                        DnsSdTxtRecord txt,
                        IPEndpoint ipv4_endpoint,
                        IPEndpoint ipv6_endpoint,
                        NetworkInterfaceIndex network_interface);
  DnsSdInstanceEndpoint(DnsSdInstance instance,
                        IPAddress address_v4,
                        IPAddress address_v6,
                        NetworkInterfaceIndex network_interface);

  ~DnsSdInstanceEndpoint() override;

  // Returns the address associated with this DNS-SD record. In any valid
  // record, at least one will be set.
  const IPAddress& address_v4() const { return address_v4_; }
  const IPAddress& address_v6() const { return address_v6_; }
  IPEndpoint endpoint_v4() const;
  IPEndpoint endpoint_v6() const;

  // Network Interface associated with this endpoint.
  NetworkInterfaceIndex network_interface() const { return network_interface_; }

 private:
  IPAddress address_v4_;
  IPAddress address_v6_;

  NetworkInterfaceIndex network_interface_;

  friend bool operator<(const DnsSdInstanceEndpoint& lhs,
                        const DnsSdInstanceEndpoint& rhs);
};

bool operator<(const DnsSdInstanceEndpoint& lhs,
               const DnsSdInstanceEndpoint& rhs);

inline bool operator>(const DnsSdInstanceEndpoint& lhs,
                      const DnsSdInstanceEndpoint& rhs) {
  return rhs < lhs;
}

inline bool operator<=(const DnsSdInstanceEndpoint& lhs,
                       const DnsSdInstanceEndpoint& rhs) {
  return !(rhs > lhs);
}

inline bool operator>=(const DnsSdInstanceEndpoint& lhs,
                       const DnsSdInstanceEndpoint& rhs) {
  return !(rhs < lhs);
}

inline bool operator==(const DnsSdInstanceEndpoint& lhs,
                       const DnsSdInstanceEndpoint& rhs) {
  return lhs <= rhs && lhs >= rhs;
}

inline bool operator!=(const DnsSdInstanceEndpoint& lhs,
                       const DnsSdInstanceEndpoint& rhs) {
  return !(lhs == rhs);
}

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_DNS_SD_INSTANCE_ENDPOINT_H_
