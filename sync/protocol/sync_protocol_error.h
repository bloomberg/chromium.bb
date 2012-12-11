// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SYNC_PROTOCOL_SYNC_PROTOCOL_ERROR_H_
#define SYNC_PROTOCOL_SYNC_PROTOCOL_ERROR_H_

#include <string>

#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer{

enum SyncProtocolErrorType {
  // Success case.
  SYNC_SUCCESS,

  // Birthday does not match that of the server.
  NOT_MY_BIRTHDAY,

  // Server is busy. Try later.
  THROTTLED,

  // Clear user data is being currently executed by the server.
  CLEAR_PENDING,

  // Server cannot service the request now.
  TRANSIENT_ERROR,

  // Server does not wish the client to retry any more until the action has
  // been taken.
  NON_RETRIABLE_ERROR,

  // Indicates the datatypes have been migrated and the client should resync
  // them to get the latest progress markers.
  MIGRATION_DONE,

  // Invalid Credential.
  INVALID_CREDENTIAL,

  // The default value.
  UNKNOWN_ERROR
};

enum ClientAction {
  // Upgrade the client to latest version.
  UPGRADE_CLIENT,

  // Clear user data and setup sync again.
  CLEAR_USER_DATA_AND_RESYNC,

  // Set the bit on the account to enable sync.
  ENABLE_SYNC_ON_ACCOUNT,

  // Stop sync and restart sync.
  STOP_AND_RESTART_SYNC,

  // Wipe this client of any sync data.
  DISABLE_SYNC_ON_CLIENT,

  // The default. No action.
  UNKNOWN_ACTION
};

struct SYNC_EXPORT SyncProtocolError {
  SyncProtocolErrorType error_type;
  std::string error_description;
  std::string url;
  ClientAction action;
  ModelTypeSet error_data_types;
  SyncProtocolError();
  ~SyncProtocolError();
  DictionaryValue* ToValue() const;
};

SYNC_EXPORT const char* GetSyncErrorTypeString(SyncProtocolErrorType type);
SYNC_EXPORT const char* GetClientActionString(ClientAction action);
}  // namespace syncer
#endif  // SYNC_PROTOCOL_SYNC_PROTOCOL_ERROR_H_

