// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy.h"


namespace network {

CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy() = default;
CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy(
    const CrossOriginEmbedderPolicy& src) = default;
CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy(
    CrossOriginEmbedderPolicy&& src) = default;
CrossOriginEmbedderPolicy::~CrossOriginEmbedderPolicy() = default;

CrossOriginEmbedderPolicy& CrossOriginEmbedderPolicy::operator=(
    const CrossOriginEmbedderPolicy& src) = default;
CrossOriginEmbedderPolicy& CrossOriginEmbedderPolicy::operator=(
    CrossOriginEmbedderPolicy&& src) = default;
bool CrossOriginEmbedderPolicy::operator==(
    const CrossOriginEmbedderPolicy& other) const {
  return value == other.value &&
         reporting_endpoint == other.reporting_endpoint &&
         report_only_value == other.report_only_value &&
         report_only_reporting_endpoint == other.report_only_reporting_endpoint;
}

}  // namespace network
