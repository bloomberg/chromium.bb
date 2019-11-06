// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_DATA_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_DATA_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace declarative_net_request {

// Manifest data required for the kDeclarativeNetRequestKey manifest
// key.
struct DNRManifestData : Extension::ManifestData {
  explicit DNRManifestData(base::FilePath ruleset_relative_path);
  ~DNRManifestData() override;

  // Returns true if the extension specified the kDeclarativeNetRequestKey
  // manifest key.
  static bool HasRuleset(const Extension& extension);

  // Returns the path to the JSON ruleset for the |extension|. This must be
  // called only if HasRuleset returns true for the |extension|.
  static base::FilePath GetRulesetPath(const Extension& extension);

  base::FilePath ruleset_relative_path;

  DISALLOW_COPY_AND_ASSIGN(DNRManifestData);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_DATA_H_
