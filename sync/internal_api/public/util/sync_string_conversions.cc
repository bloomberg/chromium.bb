// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/util/sync_string_conversions.h"

namespace syncer {

const char* ConnectionStatusToString(ConnectionStatus status) {
  switch (status) {
    case CONNECTION_OK:
      return "CONNECTION_OK";
    case CONNECTION_AUTH_ERROR:
      return "CONNECTION_AUTH_ERROR";
    case CONNECTION_SERVER_ERROR:
      return "CONNECTION_SERVER_ERROR";
    default:
      NOTREACHED();
      return "INVALID_CONNECTION_STATUS";
  }
}

// Helper function that converts a PassphraseRequiredReason value to a string.
const char* PassphraseRequiredReasonToString(
    PassphraseRequiredReason reason) {
  switch (reason) {
    case REASON_PASSPHRASE_NOT_REQUIRED:
      return "REASON_PASSPHRASE_NOT_REQUIRED";
    case REASON_ENCRYPTION:
      return "REASON_ENCRYPTION";
    case REASON_DECRYPTION:
      return "REASON_DECRYPTION";
    default:
      NOTREACHED();
      return "INVALID_REASON";
  }
}

}  // namespace syncer
