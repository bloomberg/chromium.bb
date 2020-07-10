// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_INSTANCE_KEY_H_
#define DISCOVERY_DNSSD_IMPL_INSTANCE_KEY_H_

#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

class MdnsRecord;
class ServiceKey;

// This class is intended to be used as the key of a std::unordered_map or
// std::map  when referencing data related to a specific service instance.
class InstanceKey {
 public:
  // NOTE: The record provided must have valid instance, service, and domain
  // labels.
  explicit InstanceKey(const MdnsRecord& record);

  // NOTE: The provided parameters must be valid instance,  service and domain
  // ids.
  InstanceKey(absl::string_view instance,
              absl::string_view service,
              absl::string_view domain);
  InstanceKey(const InstanceKey& other);
  InstanceKey(InstanceKey&& other);

  InstanceKey& operator=(const InstanceKey& rhs);
  InstanceKey& operator=(InstanceKey&& rhs);

  const std::string& instance_id() const { return instance_id_; }
  const std::string& service_id() const { return service_id_; }
  const std::string& domain_id() const { return domain_id_; }

  // Represents whether this InstanceKey is an instance of the service provided.
  bool IsInstanceOf(const ServiceKey& service_key) const;

 private:
  static ErrorOr<InstanceKey> CreateFromRecord(const MdnsRecord& record);

  std::string instance_id_;
  std::string service_id_;
  std::string domain_id_;

  template <typename H>
  friend H AbslHashValue(H h, const InstanceKey& key);

  friend bool operator<(const InstanceKey& lhs, const InstanceKey& rhs);

  // Validation method which needs the same code as CreateFromRecord(). Use a
  // friend declaration to avoid duplicating this code while still keeping the
  // factory private.
  friend bool HasValidDnsRecordAddress(const MdnsRecord& record);
};

template <typename H>
H AbslHashValue(H h, const InstanceKey& key) {
  return H::combine(std::move(h), key.service_id_, key.domain_id_,
                    key.instance_id_);
}

inline bool operator<(const InstanceKey& lhs, const InstanceKey& rhs) {
  int comp = lhs.domain_id_.compare(rhs.domain_id_);
  if (comp != 0) {
    return comp < 0;
  }

  comp = lhs.service_id_.compare(rhs.service_id_);
  if (comp != 0) {
    return comp < 0;
  }

  return lhs.instance_id_ < rhs.instance_id_;
}

inline bool operator>(const InstanceKey& lhs, const InstanceKey& rhs) {
  return rhs < lhs;
}

inline bool operator<=(const InstanceKey& lhs, const InstanceKey& rhs) {
  return !(rhs > lhs);
}

inline bool operator>=(const InstanceKey& lhs, const InstanceKey& rhs) {
  return !(rhs < lhs);
}

inline bool operator==(const InstanceKey& lhs, const InstanceKey& rhs) {
  return lhs <= rhs && lhs >= rhs;
}

inline bool operator!=(const InstanceKey& lhs, const InstanceKey& rhs) {
  return !(lhs == rhs);
}

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_INSTANCE_KEY_H_
