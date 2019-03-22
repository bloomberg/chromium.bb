// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/shell_extensions_api_provider.h"

#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/shell/common/api/generated_schemas.h"
#include "extensions/shell/common/api/shell_api_features.h"
#include "extensions/shell/grit/app_shell_resources.h"

namespace extensions {

ShellExtensionsAPIProvider::ShellExtensionsAPIProvider() = default;
ShellExtensionsAPIProvider::~ShellExtensionsAPIProvider() = default;

void ShellExtensionsAPIProvider::AddAPIFeatures(FeatureProvider* provider) {
  AddShellAPIFeatures(provider);
}

void ShellExtensionsAPIProvider::AddManifestFeatures(
    FeatureProvider* provider) {
  // No shell-specific manifest features.
}

void ShellExtensionsAPIProvider::AddPermissionFeatures(
    FeatureProvider* provider) {
  // No shell-specific permission features.
}

void ShellExtensionsAPIProvider::AddBehaviorFeatures(
    FeatureProvider* provider) {
  // No shell-specific behavior features.
}

void ShellExtensionsAPIProvider::AddAPIJSONSources(
    JSONFeatureProviderSource* json_source) {
  json_source->LoadJSON(IDR_SHELL_EXTENSION_API_FEATURES);
}

bool ShellExtensionsAPIProvider::IsAPISchemaGenerated(const std::string& name) {
  return shell::api::ShellGeneratedSchemas::IsGenerated(name);
}

base::StringPiece ShellExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  return shell::api::ShellGeneratedSchemas::Get(name);
}

void ShellExtensionsAPIProvider::RegisterPermissions(
    PermissionsInfo* permissions_info) {}

void ShellExtensionsAPIProvider::RegisterManifestHandlers() {}

}  // namespace extensions
