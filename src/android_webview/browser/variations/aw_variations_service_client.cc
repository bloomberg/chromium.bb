// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/variations/aw_variations_service_client.h"

#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using version_info::Channel;

namespace android_webview {

AwVariationsServiceClient::AwVariationsServiceClient() = default;

AwVariationsServiceClient::~AwVariationsServiceClient() = default;

base::Version AwVariationsServiceClient::GetVersionForSimulation() {
  return version_info::GetVersion();
}

scoped_refptr<network::SharedURLLoaderFactory>
AwVariationsServiceClient::GetURLLoaderFactory() {
  return nullptr;
}

network_time::NetworkTimeTracker*
AwVariationsServiceClient::GetNetworkTimeTracker() {
  return nullptr;
}

Channel AwVariationsServiceClient::GetChannel() {
  return version_info::android::GetChannel();
}

bool AwVariationsServiceClient::OverridesRestrictParameter(
    std::string* parameter) {
  return false;
}

bool AwVariationsServiceClient::IsEnterprise() {
  return false;
}

}  // namespace android_webview
