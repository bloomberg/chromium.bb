// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_BLUETOOTH_BLUETOOTH_MANIFEST_HANDLER_H_
#define EXTENSIONS_COMMON_API_BLUETOOTH_BLUETOOTH_MANIFEST_HANDLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {
class Extension;
class ManifestPermission;
}

namespace extensions {

// Parses the "bluetooth" manifest key.
class BluetoothManifestHandler : public ManifestHandler {
 public:
  BluetoothManifestHandler();
  ~BluetoothManifestHandler() override;

  // ManifestHandler overrides.
  bool Parse(Extension* extension, base::string16* error) override;
  ManifestPermission* CreatePermission() override;
  ManifestPermission* CreateInitialRequiredPermission(
      const Extension* extension) override;

 private:
  // ManifestHandler overrides.
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(BluetoothManifestHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_BLUETOOTH_BLUETOOTH_MANIFEST_HANDLER_H_
