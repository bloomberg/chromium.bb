// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_ORIGIN_TRIALS_COMPONENT_LOADER_POLICY_H_
#define ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_ORIGIN_TRIALS_COMPONENT_LOADER_POLICY_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "components/component_updater/android/component_loader_policy.h"

namespace base {
class DictionaryValue;
class Version;
}  // namespace base

namespace android_webview {

// OriginTrialsComponentLoaderPolicy defines a loader responsible
// for receiving origin trials config.
class OriginTrialsComponentLoaderPolicy
    : public component_updater::ComponentLoaderPolicy {
 public:
  OriginTrialsComponentLoaderPolicy();
  ~OriginTrialsComponentLoaderPolicy() override;

  OriginTrialsComponentLoaderPolicy(const OriginTrialsComponentLoaderPolicy&) =
      delete;
  OriginTrialsComponentLoaderPolicy& operator=(
      const OriginTrialsComponentLoaderPolicy&) = delete;

 private:
  // The following methods override ComponentLoaderPolicy.
  void ComponentLoaded(
      const base::Version& version,
      base::flat_map<std::string, base::ScopedFD>& fd_map,
      std::unique_ptr<base::DictionaryValue> manifest) override;
  void ComponentLoadFailed(
      component_updater::ComponentLoadResult error) override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetMetricsSuffix() const override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_ORIGIN_TRIALS_COMPONENT_LOADER_POLICY_H_