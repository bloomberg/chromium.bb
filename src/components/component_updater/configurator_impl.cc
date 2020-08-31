// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/configurator_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/component_updater/component_updater_url_constants.h"
#include "components/update_client/command_line_config_policy.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/utils.h"
#include "components/version_info/version_info.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif  // OS_WIN

namespace component_updater {

namespace {

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

}  // namespace

ConfiguratorImpl::ConfiguratorImpl(
    const update_client::CommandLineConfigPolicy& config_policy,
    bool require_encryption)
    : background_downloads_enabled_(config_policy.BackgroundDownloadsEnabled()),
      deltas_enabled_(config_policy.DeltaUpdatesEnabled()),
      fast_update_(config_policy.FastUpdate()),
      pings_enabled_(config_policy.PingsEnabled()),
      require_encryption_(require_encryption),
      url_source_override_(config_policy.UrlSourceOverride()),
      initial_delay_(config_policy.InitialDelay()) {
  if (config_policy.TestRequest()) {
    extra_info_["testrequest"] = "1";
    extra_info_["testsource"] = "dev";
  }
}

ConfiguratorImpl::~ConfiguratorImpl() = default;

int ConfiguratorImpl::InitialDelay() const {
  if (initial_delay_)
    return initial_delay_;
  return fast_update_ ? 10 : kDelayOneMinute;
}

int ConfiguratorImpl::NextCheckDelay() const {
  return 5 * kDelayOneHour;
}

int ConfiguratorImpl::OnDemandDelay() const {
  return fast_update_ ? 2 : (30 * kDelayOneMinute);
}

int ConfiguratorImpl::UpdateDelay() const {
  return fast_update_ ? 10 : (15 * kDelayOneMinute);
}

std::vector<GURL> ConfiguratorImpl::UpdateUrl() const {
  if (url_source_override_.is_valid())
    return {GURL(url_source_override_)};

  std::vector<GURL> urls{GURL(kUpdaterJSONDefaultUrl),
                         GURL(kUpdaterJSONFallbackUrl)};
  if (require_encryption_)
    update_client::RemoveUnsecureUrls(&urls);

  return urls;
}

std::vector<GURL> ConfiguratorImpl::PingUrl() const {
  return pings_enabled_ ? UpdateUrl() : std::vector<GURL>();
}

const base::Version& ConfiguratorImpl::GetBrowserVersion() const {
  return version_info::GetVersion();
}

std::string ConfiguratorImpl::GetOSLongName() const {
  return version_info::GetOSType();
}

base::flat_map<std::string, std::string> ConfiguratorImpl::ExtraRequestParams()
    const {
  return extra_info_;
}

std::string ConfiguratorImpl::GetDownloadPreference() const {
  return std::string();
}

bool ConfiguratorImpl::EnabledDeltas() const {
  return deltas_enabled_;
}

bool ConfiguratorImpl::EnabledComponentUpdates() const {
  return true;
}

bool ConfiguratorImpl::EnabledBackgroundDownloader() const {
  return background_downloads_enabled_;
}

bool ConfiguratorImpl::EnabledCupSigning() const {
  return true;
}

// The default implementation for most embedders returns an empty string.
// Desktop embedders, such as the Windows component updater can provide a
// meaningful implementation for this function.
std::string ConfiguratorImpl::GetAppGuid() const {
  return {};
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
ConfiguratorImpl::GetProtocolHandlerFactory() const {
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

}  // namespace component_updater
