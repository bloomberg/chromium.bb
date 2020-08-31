// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_STORE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"

namespace policy {

// Implements a cloud policy store that stores policy for machine level user
// cloud policy. This is used on (non-chromeos) platforms that do no have a
// secure storage implementation.
class POLICY_EXPORT MachineLevelUserCloudPolicyStore
    : public DesktopCloudPolicyStore {
 public:
  MachineLevelUserCloudPolicyStore(
      const DMToken& machine_dm_token,
      const std::string& machine_client_id,
      const base::FilePath& external_policy_path,
      const base::FilePath& policy_path,
      const base::FilePath& key_path,
      bool cloud_policy_has_priority,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~MachineLevelUserCloudPolicyStore() override;

  // Creates a MachineLevelUserCloudPolicyStore instance. |external_policy_path|
  // must be a secure location because no signature validations are made on it.
  static std::unique_ptr<MachineLevelUserCloudPolicyStore> Create(
      const DMToken& machine_dm_token,
      const std::string& machine_client_id,
      const base::FilePath& external_policy_path,
      const base::FilePath& policy_dir,
      bool cloud_policy_has_priority,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // override DesktopCloudPolicyStore
  void LoadImmediately() override;
  void Load() override;

  // override UserCloudPolicyStoreBase
  std::unique_ptr<UserCloudPolicyValidator> CreateValidator(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      CloudPolicyValidatorBase::ValidateTimestampOption option) override;

  // Setup global |dm_token| and |client_id| in store for the validation purpose
  // before policy refresh.
  void SetupRegistration(const DMToken& machine_dm_token,
                         const std::string& machine_client_id);

  // No DM token can be fetched from server or read from disk. Finish
  // initialization with empty policy data.
  void InitWithoutToken();

 private:
  // Function used as a PolicyLoadFilter to use external policies if they are
  // newer than the ones previously written by the browser.
  static PolicyLoadResult MaybeUseExternalCachedPolicies(
      const base::FilePath& path,
      PolicyLoadResult default_cached_policy_load_result);
  // override DesktopCloudPolicyStore
  void Validate(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
      std::unique_ptr<enterprise_management::PolicySigningKey> key,
      bool validate_in_background,
      UserCloudPolicyValidator::CompletionCallback callback) override;

  DMToken machine_dm_token_;
  std::string machine_client_id_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyStore);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MACHINE_LEVEL_USER_CLOUD_POLICY_STORE_H_
