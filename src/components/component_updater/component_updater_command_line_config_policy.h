// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_COMMAND_LINE_CONFIG_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_COMMAND_LINE_CONFIG_POLICY_H_

#include "base/macros.h"
#include "components/update_client/command_line_config_policy.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
}

namespace component_updater {

// Component updater config policy implementation.
class ComponentUpdaterCommandLineConfigPolicy final
    : public update_client::CommandLineConfigPolicy {
 public:
  explicit ComponentUpdaterCommandLineConfigPolicy(
      const base::CommandLine* cmdline);

  // update_client::CommandLineConfigPolicy overrides.
  bool BackgroundDownloadsEnabled() const override;
  bool DeltaUpdatesEnabled() const override;
  bool FastUpdate() const override;
  bool PingsEnabled() const override;
  bool TestRequest() const override;
  GURL UrlSourceOverride() const override;
  int InitialDelay() const override;

 private:
  bool background_downloads_enabled_ = false;
  bool deltas_enabled_ = true;
  bool fast_update_ = false;
  bool pings_enabled_ = true;
  bool test_request_ = false;

  // If non-zero, time interval in seconds until the first component
  // update check.
  int initial_delay_ = 0;

  GURL url_source_override_;

  DISALLOW_COPY_AND_ASSIGN(ComponentUpdaterCommandLineConfigPolicy);
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_COMMAND_LINE_CONFIG_POLICY_H_
