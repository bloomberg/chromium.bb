// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_UTIL_EXPERIMENTS_
#define SYNC_UTIL_EXPERIMENTS_

#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

const char kKeystoreEncryptionTag[] = "keystore_encryption";
const char kKeystoreEncryptionFlag[] = "sync-keystore-encryption";
const char kAutofillCullingTag[] = "autofill_culling";
const char kFaviconSyncTag[] = "favicon_sync";
const char kFaviconSyncFlag[] = "enable-sync-favicons";

// A structure to hold the enable status of experimental sync features.
struct Experiments {
  Experiments() : keystore_encryption(false),
                  autofill_culling(false),
                  favicon_sync(false),
                  favicon_sync_limit(200) {}

  bool Matches(const Experiments& rhs) {
    return (keystore_encryption == rhs.keystore_encryption &&
            autofill_culling == rhs.autofill_culling &&
            favicon_sync == rhs.favicon_sync &&
            favicon_sync_limit == rhs.favicon_sync_limit);
  }

  // Enable keystore encryption logic and the new encryption UI.
  bool keystore_encryption;

  // Enable deletion of expired autofill entries (if autofill sync is enabled).
  bool autofill_culling;

  // Enable the favicons sync datatypes (favicon images and favicon tracking).
  bool favicon_sync;

  // The number of favicons that a client is permitted to sync.
  int favicon_sync_limit;
};

}  // namespace syncer

#endif  // SYNC_UTIL_EXPERIMENTS_
