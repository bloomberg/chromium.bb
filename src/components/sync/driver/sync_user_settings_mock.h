// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_MOCK_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_MOCK_H_

#include <string>
#include <vector>

#include "components/sync/driver/sync_user_settings.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class SyncUserSettingsMock : public SyncUserSettings {
 public:
  SyncUserSettingsMock();
  ~SyncUserSettingsMock() override;
  MOCK_METHOD(bool, IsSyncRequested, (), (const override));
  MOCK_METHOD(void, SetSyncRequested, (bool), (override));
  MOCK_METHOD(bool, IsSyncAllowedByPlatform, (), (const override));
  MOCK_METHOD(void, SetSyncAllowedByPlatform, (bool), (override));
  MOCK_METHOD(bool, IsFirstSetupComplete, (), (const override));
  MOCK_METHOD(void,
              SetFirstSetupComplete,
              (SyncFirstSetupCompleteSource),
              (override));
  MOCK_METHOD(bool, IsSyncEverythingEnabled, (), (const override));
  MOCK_METHOD(UserSelectableTypeSet, GetSelectedTypes, (), (const override));
  MOCK_METHOD(void,
              SetSelectedTypes,
              (bool, UserSelectableTypeSet),
              (override));
  MOCK_METHOD(UserSelectableTypeSet,
              GetRegisteredSelectableTypes,
              (),
              (const override));

#if defined(OS_CHROMEOS)
  MOCK_METHOD(bool, IsSyncAllOsTypesEnabled, (), (const override));
  MOCK_METHOD(UserSelectableOsTypeSet,
              GetSelectedOsTypes,
              (),
              (const override));
  MOCK_METHOD(void,
              SetSelectedOsTypes,
              (bool, UserSelectableOsTypeSet),
              (override));
  MOCK_METHOD(UserSelectableOsTypeSet,
              GetRegisteredSelectableOsTypes,
              (),
              (const override));
  MOCK_METHOD(bool, IsOsSyncFeatureEnabled, (), (const override));
  MOCK_METHOD(void, SetOsSyncFeatureEnabled, (bool), (override));
#endif
  MOCK_METHOD(bool, IsEncryptEverythingAllowed, (), (const override));
  MOCK_METHOD(bool, IsEncryptEverythingEnabled, (), (const override));
  MOCK_METHOD(ModelTypeSet, GetEncryptedDataTypes, (), (const override));
  MOCK_METHOD(bool, IsPassphraseRequired, (), (const override));
  MOCK_METHOD(bool,
              IsPassphraseRequiredForPreferredDataTypes,
              (),
              (const override));
  MOCK_METHOD(bool, IsTrustedVaultKeyRequired, (), (const override));
  MOCK_METHOD(bool,
              IsTrustedVaultKeyRequiredForPreferredDataTypes,
              (),
              (const override));
  MOCK_METHOD(bool, IsTrustedVaultRecoverabilityDegraded, (), (const override));
  MOCK_METHOD(bool, IsUsingSecondaryPassphrase, (), (const override));
  MOCK_METHOD(base::Time, GetExplicitPassphraseTime, (), (const override));
  MOCK_METHOD(PassphraseType, GetPassphraseType, (), (const override));
  MOCK_METHOD(void, SetEncryptionPassphrase, (const std::string&), (override));
  MOCK_METHOD(bool, SetDecryptionPassphrase, (const std::string&), (override));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_MOCK_H_
