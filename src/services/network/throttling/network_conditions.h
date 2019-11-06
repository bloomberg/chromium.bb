// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_NETWORK_CONDITIONS_H_
#define SERVICES_NETWORK_THROTTLING_NETWORK_CONDITIONS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"

namespace network {

// NetworkConditions holds information about desired network conditions.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkConditions {
 public:
  NetworkConditions();
  ~NetworkConditions();

  explicit NetworkConditions(bool offline);
  NetworkConditions(bool offline,
                    double latency,
                    double download_throughput,
                    double upload_throughput);

  bool IsThrottling() const;

  bool offline() const { return offline_; }
  double latency() const { return latency_; }
  double download_throughput() const { return download_throughput_; }
  double upload_throughput() const { return upload_throughput_; }

 private:
  const bool offline_;
  const double latency_;
  const double download_throughput_;
  const double upload_throughput_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConditions);
};

}  // namespace network

#endif  // SERVICES_NETWORK_THROTTLING_NETWORK_CONDITIONS_H_
