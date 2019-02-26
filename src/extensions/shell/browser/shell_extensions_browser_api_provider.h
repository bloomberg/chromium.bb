// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_API_PROVIDER_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_API_PROVIDER_H_

#include "base/macros.h"
#include "extensions/browser/extensions_browser_api_provider.h"

namespace extensions {

class ShellExtensionsBrowserAPIProvider : public ExtensionsBrowserAPIProvider {
 public:
  ShellExtensionsBrowserAPIProvider();
  ~ShellExtensionsBrowserAPIProvider() override;

  void RegisterExtensionFunctions(ExtensionFunctionRegistry* registry) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsBrowserAPIProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_API_PROVIDER_H_
