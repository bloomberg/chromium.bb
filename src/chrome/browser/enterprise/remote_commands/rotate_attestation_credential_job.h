// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_ROTATE_ATTESTATION_CREDENTIAL_JOB_H_
#define CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_ROTATE_ATTESTATION_CREDENTIAL_JOB_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "content/public/browser/browsing_data_remover.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_commands {

// A remote command for rotating the device's existing attestation credential.
class RotateAttestationCredentialJob : public policy::RemoteCommandJob {
 public:
  explicit RotateAttestationCredentialJob(
      enterprise_connectors::DeviceTrustKeyManager* key_manager);
  ~RotateAttestationCredentialJob() override;

 private:
  class ResultPayload : public RemoteCommandJob::ResultPayload {
   public:
    explicit ResultPayload(
        enterprise_connectors::DeviceTrustKeyManager::KeyRotationResult result);

    ResultPayload(const ResultPayload&) = delete;
    ResultPayload& operator=(const ResultPayload&) = delete;

    ~ResultPayload() override = default;

    bool IsSuccess() const {
      return result_ == enterprise_connectors::DeviceTrustKeyManager::
                            KeyRotationResult::SUCCESS;
    }

    // RemoteCommandJob::ResultPayload:
    std::unique_ptr<std::string> Serialize() override;

   private:
    enterprise_connectors::DeviceTrustKeyManager::KeyRotationResult result_;
    std::string payload_;
  };

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult succeeded_callback,
               CallbackWithResult failed_callback) override;

  void OnKeyRotated(
      CallbackWithResult succeeded_callback,
      CallbackWithResult failed_callback,
      enterprise_connectors::DeviceTrustKeyManager::KeyRotationResult
          rotation_result);

  absl::optional<std::string> nonce_;

  // Non-owned pointer to the DeviceTrustKeyManager of the current browser
  // process.
  raw_ptr<enterprise_connectors::DeviceTrustKeyManager> key_manager_;

  base::WeakPtrFactory<RotateAttestationCredentialJob> weak_factory_{this};
};

}  // namespace enterprise_commands

#endif  // CHROME_BROWSER_ENTERPRISE_REMOTE_COMMANDS_ROTATE_ATTESTATION_CREDENTIAL_JOB_H_
