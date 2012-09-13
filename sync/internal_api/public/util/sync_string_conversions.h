// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_UTIL_SYNC_STRING_CONVERSIONS_H_
#define SYNC_INTERNAL_API_PUBLIC_UTIL_SYNC_STRING_CONVERSIONS_H_

#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/internal_api/public/sync_manager.h"

namespace syncer {

const char* ConnectionStatusToString(ConnectionStatus status);

// Returns the string representation of a PassphraseRequiredReason value.
const char* PassphraseRequiredReasonToString(
    PassphraseRequiredReason reason);

const char* PassphraseTypeToString(PassphraseType type);

const char* BootstrapTokenTypeToString(BootstrapTokenType type);
}

#endif  // SYNC_INTERNAL_API_PUBLIC_UTIL_SYNC_STRING_CONVERSIONS_H_
