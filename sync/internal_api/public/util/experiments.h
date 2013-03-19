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
const char kFullHistorySyncTag[] = "history_delete_directives";
const char kFullHistorySyncFlag[] = "full-history-sync";
const char kFaviconSyncTag[] = "favicon_sync";
const char kFaviconSyncFlag[] = "enable-sync-favicons";

// A structure to hold the enable status of experimental sync features.
struct Experiments {
  Experiments() : keystore_encryption(false),
                  autofill_culling(false),
                  full_history_sync(false),
                  favicon_sync(false) {}

  bool Matches(const Experiments& rhs) {
    return (keystore_encryption == rhs.keystore_encryption &&
            autofill_culling == rhs.autofill_culling &&
            full_history_sync == rhs.full_history_sync &&
            favicon_sync == rhs.favicon_sync);
  }

  // Enable keystore encryption logic and the new encryption UI.
  bool keystore_encryption;

  // Enable deletion of expired autofill entries (if autofill sync is enabled).
  bool autofill_culling;

  // Enable full history sync (and history delete directives) for this client.
  bool full_history_sync;

  // Enable the favicons sync datatypes (favicon images and favicon tracking).
  bool favicon_sync;
};

}  // namespace syncer

#endif  // SYNC_UTIL_EXPERIMENTS_
