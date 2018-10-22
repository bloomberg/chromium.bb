// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_sync_service.h"

#include <vector>

#include "components/sync/base/model_type.h"
#include "components/sync/base/progress_marker_map.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/cycle/model_neutral_state.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"

namespace autofill {

TestSyncService::TestSyncService() : data_types_(syncer::ModelTypeSet::All()) {}

TestSyncService::~TestSyncService() {}

int TestSyncService::GetDisableReasons() const {
  return disable_reasons_;
}

syncer::ModelTypeSet TestSyncService::GetPreferredDataTypes() const {
  return data_types_;
}

syncer::ModelTypeSet TestSyncService::GetActiveDataTypes() const {
  return data_types_;
}

bool TestSyncService::IsFirstSetupComplete() const {
  return true;
}

bool TestSyncService::IsUsingSecondaryPassphrase() const {
  return is_using_secondary_passphrase_;
}

bool TestSyncService::IsAuthenticatedAccountPrimary() const {
  return is_authenticated_account_primary_;
}

const GoogleServiceAuthError& TestSyncService::GetAuthError() const {
  return auth_error_;
}

syncer::SyncCycleSnapshot TestSyncService::GetLastCycleSnapshot() const {
  if (sync_cycle_complete_) {
    return syncer::SyncCycleSnapshot(
        syncer::ModelNeutralState(), syncer::ProgressMarkerMap(), false, 5, 2,
        7, false, 0, base::Time::Now(), base::Time::Now(),
        std::vector<int>(syncer::MODEL_TYPE_COUNT, 0),
        std::vector<int>(syncer::MODEL_TYPE_COUNT, 0),
        sync_pb::SyncEnums::UNKNOWN_ORIGIN,
        /*short_poll_interval=*/base::TimeDelta::FromMinutes(30),
        /*long_poll_interval=*/base::TimeDelta::FromMinutes(180),
        /*has_remaining_local_changes=*/false);
  }
  return syncer::SyncCycleSnapshot();
}

syncer::SyncTokenStatus TestSyncService::GetSyncTokenStatus() const {
  syncer::SyncTokenStatus token;

  if (is_in_auth_error_) {
    token.connection_status = syncer::ConnectionStatus::CONNECTION_AUTH_ERROR;
    token.last_get_token_error =
        GoogleServiceAuthError::FromServiceError("error");
  }

  return token;
}

AccountInfo TestSyncService::GetAuthenticatedAccountInfo() const {
  return account_info_;
}

void TestSyncService::SetInAuthError(bool is_in_auth_error) {
  is_in_auth_error_ = is_in_auth_error;

  if (is_in_auth_error) {
    auth_error_ = GoogleServiceAuthError(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  } else {
    auth_error_ = GoogleServiceAuthError(GoogleServiceAuthError::NONE);
  }
}

}  // namespace autofill
