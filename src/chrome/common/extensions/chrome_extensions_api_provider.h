// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_API_PROVIDER_H_
#define CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_API_PROVIDER_H_

#include "base/macros.h"
#include "extensions/common/extensions_api_provider.h"

namespace extensions {

class ChromeExtensionsAPIProvider : public ExtensionsAPIProvider {
 public:
  ChromeExtensionsAPIProvider();
  ~ChromeExtensionsAPIProvider() override;

  // ExtensionsAPIProvider:
  void AddAPIFeatures(FeatureProvider* provider) override;
  void AddManifestFeatures(FeatureProvider* provider) override;
  void AddPermissionFeatures(FeatureProvider* provider) override;
  void AddBehaviorFeatures(FeatureProvider* provider) override;
  void AddAPIJSONSources(JSONFeatureProviderSource* json_source) override;
  bool IsAPISchemaGenerated(const std::string& name) override;
  base::StringPiece GetAPISchema(const std::string& name) override;
  void RegisterPermissions(PermissionsInfo* permissions_info) override;
  void RegisterManifestHandlers() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsAPIProvider);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_API_PROVIDER_H_
