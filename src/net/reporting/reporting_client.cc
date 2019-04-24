// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_client.h"

#include <string>

#include "base/time/time.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

const char ReportingClient::kDefaultGroup[] = "default";
const int ReportingClient::kDefaultPriority = 0;
const int ReportingClient::kDefaultWeight = 1;

ReportingClient::ReportingClient(const url::Origin& origin,
                                 const GURL& endpoint,
                                 Subdomains subdomains,
                                 const std::string& group,
                                 base::TimeTicks expires,
                                 int priority,
                                 int weight)
    : origin(origin),
      endpoint(endpoint),
      subdomains(subdomains),
      group(group),
      expires(expires),
      priority(priority),
      weight(weight) {
  DCHECK_LT(0, weight);
}

ReportingClient::~ReportingClient() = default;

}  // namespace net
