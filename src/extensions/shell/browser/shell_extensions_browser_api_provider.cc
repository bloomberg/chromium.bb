// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extensions_browser_api_provider.h"

#include "extensions/shell/browser/api/generated_api_registration.h"

namespace extensions {

ShellExtensionsBrowserAPIProvider::ShellExtensionsBrowserAPIProvider() =
    default;
ShellExtensionsBrowserAPIProvider::~ShellExtensionsBrowserAPIProvider() =
    default;

void ShellExtensionsBrowserAPIProvider::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) {
  shell::api::ShellGeneratedFunctionRegistry::RegisterAll(registry);
}

}  // namespace extensions
