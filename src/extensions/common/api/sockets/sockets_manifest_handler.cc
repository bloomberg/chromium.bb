// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/sockets/sockets_manifest_handler.h"

#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "extensions/common/api/sockets/sockets_manifest_permission.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

SocketsManifestHandler::SocketsManifestHandler() {}

SocketsManifestHandler::~SocketsManifestHandler() {}

bool SocketsManifestHandler::Parse(Extension* extension,
                                   base::string16* error) {
  const base::Value* sockets = NULL;
  CHECK(extension->manifest()->Get(manifest_keys::kSockets, &sockets));
  std::unique_ptr<SocketsManifestData> data =
      SocketsManifestData::FromValue(*sockets, error);
  if (!data)
    return false;

  extension->SetManifestData(manifest_keys::kSockets, std::move(data));
  return true;
}

ManifestPermission* SocketsManifestHandler::CreatePermission() {
  return new SocketsManifestPermission();
}

ManifestPermission* SocketsManifestHandler::CreateInitialRequiredPermission(
    const Extension* extension) {
  SocketsManifestData* data = SocketsManifestData::Get(extension);
  if (data)
    return data->permission()->Clone().release();
  return NULL;
}

base::span<const char* const> SocketsManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kSockets};
  return kKeys;
}

}  // namespace extensions
