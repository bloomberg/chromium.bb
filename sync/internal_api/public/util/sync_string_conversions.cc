// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/util/sync_string_conversions.h"

#define ENUM_CASE(x) case x: return #x

namespace syncer {

const char* ConnectionStatusToString(ConnectionStatus status) {
  switch (status) {
    ENUM_CASE(CONNECTION_OK);
    ENUM_CASE(CONNECTION_AUTH_ERROR);
    ENUM_CASE(CONNECTION_SERVER_ERROR);
    default:
      NOTREACHED();
      return "INVALID_CONNECTION_STATUS";
  }
}

// Helper function that converts a PassphraseRequiredReason value to a string.
const char* PassphraseRequiredReasonToString(
    PassphraseRequiredReason reason) {
  switch (reason) {
    ENUM_CASE(REASON_PASSPHRASE_NOT_REQUIRED);
    ENUM_CASE(REASON_ENCRYPTION);
    ENUM_CASE(REASON_DECRYPTION);
    default:
      NOTREACHED();
      return "INVALID_REASON";
  }
}

const char* PassphraseStateToString(PassphraseState state) {
  switch (state) {
    ENUM_CASE(IMPLICIT_PASSPHRASE);
    ENUM_CASE(KEYSTORE_PASSPHRASE);
    ENUM_CASE(FROZEN_IMPLICIT_PASSPHRASE);
    ENUM_CASE(CUSTOM_PASSPHRASE);
    default:
      NOTREACHED();
      return "INVALID_PASSPHRASE_STATE";
  }
}

}  // namespace syncer
