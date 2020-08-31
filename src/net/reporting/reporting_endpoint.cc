// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_endpoint.h"

#include <string>
#include <tuple>

#include "base/time/time.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

ReportingEndpointGroupKey::ReportingEndpointGroupKey() = default;

ReportingEndpointGroupKey::ReportingEndpointGroupKey(
    const NetworkIsolationKey& network_isolation_key,
    const url::Origin& origin,
    const std::string& group_name)
    : network_isolation_key(network_isolation_key),
      origin(origin),
      group_name(group_name) {}

ReportingEndpointGroupKey::ReportingEndpointGroupKey(
    const ReportingEndpointGroupKey& other) = default;
ReportingEndpointGroupKey::ReportingEndpointGroupKey(
    ReportingEndpointGroupKey&& other) = default;

ReportingEndpointGroupKey& ReportingEndpointGroupKey::operator=(
    const ReportingEndpointGroupKey&) = default;
ReportingEndpointGroupKey& ReportingEndpointGroupKey::operator=(
    ReportingEndpointGroupKey&&) = default;

ReportingEndpointGroupKey::~ReportingEndpointGroupKey() = default;

bool operator==(const ReportingEndpointGroupKey& lhs,
                const ReportingEndpointGroupKey& rhs) {
  return std::tie(lhs.network_isolation_key, lhs.origin, lhs.group_name) ==
         std::tie(rhs.network_isolation_key, rhs.origin, rhs.group_name);
}

bool operator!=(const ReportingEndpointGroupKey& lhs,
                const ReportingEndpointGroupKey& rhs) {
  return !(lhs == rhs);
}

bool operator<(const ReportingEndpointGroupKey& lhs,
               const ReportingEndpointGroupKey& rhs) {
  return std::tie(lhs.network_isolation_key, lhs.origin, lhs.group_name) <
         std::tie(rhs.network_isolation_key, rhs.origin, rhs.group_name);
}

bool operator>(const ReportingEndpointGroupKey& lhs,
               const ReportingEndpointGroupKey& rhs) {
  return std::tie(lhs.network_isolation_key, lhs.origin, lhs.group_name) >
         std::tie(rhs.network_isolation_key, rhs.origin, rhs.group_name);
}

std::string ReportingEndpointGroupKey::ToString() const {
  return "NIK: " + network_isolation_key.ToDebugString() +
         "; Origin: " + origin.Serialize() + "; Group name: " + group_name;
}

const int ReportingEndpoint::EndpointInfo::kDefaultPriority = 1;
const int ReportingEndpoint::EndpointInfo::kDefaultWeight = 1;

ReportingEndpoint::ReportingEndpoint() = default;

ReportingEndpoint::ReportingEndpoint(const ReportingEndpointGroupKey& group,
                                     const EndpointInfo& info)
    : group_key(group), info(info) {
  DCHECK_LE(0, info.weight);
  DCHECK_LE(0, info.priority);
}

ReportingEndpoint::ReportingEndpoint(const ReportingEndpoint& other) = default;
ReportingEndpoint::ReportingEndpoint(ReportingEndpoint&& other) = default;

ReportingEndpoint& ReportingEndpoint::operator=(const ReportingEndpoint&) =
    default;
ReportingEndpoint& ReportingEndpoint::operator=(ReportingEndpoint&&) = default;

ReportingEndpoint::~ReportingEndpoint() = default;

bool ReportingEndpoint::is_valid() const {
  return info.url.is_valid();
}

ReportingEndpointGroup::ReportingEndpointGroup() = default;

ReportingEndpointGroup::ReportingEndpointGroup(
    const ReportingEndpointGroup& other) = default;

ReportingEndpointGroup::~ReportingEndpointGroup() = default;

CachedReportingEndpointGroup::CachedReportingEndpointGroup(
    const ReportingEndpointGroupKey& group_key,
    OriginSubdomains include_subdomains,
    base::Time expires,
    base::Time last_used)
    : group_key(group_key),
      include_subdomains(include_subdomains),
      expires(expires),
      last_used(last_used) {}

CachedReportingEndpointGroup::CachedReportingEndpointGroup(
    const ReportingEndpointGroup& endpoint_group,
    base::Time now)
    : CachedReportingEndpointGroup(endpoint_group.group_key,
                                   endpoint_group.include_subdomains,
                                   now + endpoint_group.ttl /* expires */,
                                   now /* last_used */) {}

}  // namespace net
