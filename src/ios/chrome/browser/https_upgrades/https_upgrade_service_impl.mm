// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/https_upgrade_service_impl.h"

#include "base/containers/contains.h"
#include "base/time/default_clock.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The default expiration for certificate error and HTTPS-First Mode bypasses is
// one week.
const uint64_t kDeltaDefaultExpirationInSeconds = UINT64_C(604800);
}  // namespace

HttpsUpgradeServiceImpl::HttpsUpgradeServiceImpl(ChromeBrowserState* context)
    : clock_(new base::DefaultClock()),
      context_(context),
      allowlist_(
          ios::HostContentSettingsMapFactory::GetForBrowserState(context),
          clock_.get(),
          base::Seconds(kDeltaDefaultExpirationInSeconds)) {
  DCHECK(context_);
}

HttpsUpgradeServiceImpl::~HttpsUpgradeServiceImpl() = default;

bool HttpsUpgradeServiceImpl::IsHttpAllowedForHost(
    const std::string& host) const {
  return allowlist_.IsHttpAllowedForHost(host, context_->IsOffTheRecord());
}

void HttpsUpgradeServiceImpl::AllowHttpForHost(const std::string& host) {
  allowlist_.AllowHttpForHost(host, context_->IsOffTheRecord());
}

void HttpsUpgradeServiceImpl::ClearAllowlist(base::Time delete_begin,
                                             base::Time delete_end) {
  allowlist_.ClearAllowlist(delete_begin, delete_end);
}
