// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_SUBTLE_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_SUBTLE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/attestation/tpm_challenge_key_result.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

class Profile;

namespace chromeos {
namespace attestation {

//==================== TpmChallengeKeySubtleFactory ============================

class TpmChallengeKeySubtle;

class TpmChallengeKeySubtleFactory final {
 public:
  static std::unique_ptr<TpmChallengeKeySubtle> Create();

  // Recreates an object as it would be after |StartPrepareKeyStep| method call.
  // It is the caller's responsibility to guarantee that |StartPrepareKeyStep|
  // has successfully finished before and that only one call of
  // |StartSignChallengeStep| and/or |StartRegisterKeyStep| for a prepared key
  // pair will ever happen.
  static std::unique_ptr<TpmChallengeKeySubtle> CreateForPreparedKey(
      AttestationKeyType key_type,
      const std::string& key_name,
      Profile* profile,
      const std::string& key_name_for_spkac);

  static void SetForTesting(std::unique_ptr<TpmChallengeKeySubtle> next_result);
  static bool WillReturnTestingInstance();

 private:
  static TpmChallengeKeySubtle* next_result_for_testing_;
};

//===================== TpmChallengeKeySubtle ==================================

using TpmChallengeKeyCallback =
    base::OnceCallback<void(const TpmChallengeKeyResult& result)>;

// Asynchronously runs the flow to challenge a key in the caller context.
// Consider using |TpmChallengeKey| class for simple cases.
// This class provides a detailed API for calculating Verified Access challenge
// response and manipulating keys that are used for that.
//
// The order of calling methods is important. Expected usage:
// 1. |StartPrepareKeyStep| should always be called first.
// 2. After that, if the object is destroyed, it can be recreated by using
// |TpmChallengeKeySubtleFactory::CreateForPreparedKey|.
// 3. |StartSignChallengeStep| allows to calculate challenge response, can be
// skipped.
// 4. As a last step, |StartRegisterKeyStep| allows change key type so it cannot
// sign challenges anymore, but can be used for general puprose cryptographic
// operations (via PlatformKeysService).
class TpmChallengeKeySubtle {
 public:
  TpmChallengeKeySubtle(const TpmChallengeKeySubtle&) = delete;
  TpmChallengeKeySubtle& operator=(const TpmChallengeKeySubtle&) = delete;
  virtual ~TpmChallengeKeySubtle() = default;

  // Checks that it is allowed to generate a VA challenge response and generates
  // a new key pair if necessary. Returns result via |callback|. In case of
  // success |TpmChallengeKeyResult::public_key| will be filled. If
  // key_name_for_spkac was specified, will return the public key for the key
  // included in SPKAC. Otherwise will return the public key for the challenged
  // key.
  virtual void StartPrepareKeyStep(AttestationKeyType key_type,
                                   const std::string& key_name,
                                   Profile* profile,
                                   const std::string& key_name_for_spkac,
                                   TpmChallengeKeyCallback callback) = 0;

  // Generates a VA challenge response using the key pair prepared by
  // |PrepareKey| method. Returns VA challenge response via |callback|. In case
  // of success |TpmChallengeKeyResult::challenge_response| will be filled.
  virtual void StartSignChallengeStep(const std::string& challenge,
                                      bool include_signed_public_key,
                                      TpmChallengeKeyCallback callback) = 0;

  // Registers the key that makes it available for general purpose cryptographic
  // operations.
  virtual void StartRegisterKeyStep(TpmChallengeKeyCallback callback) = 0;

 protected:
  // Allow access to |RestorePrepareKeyResult| method.
  friend class TpmChallengeKeySubtleFactory;

  // Use TpmChallengeKeySubtleFactory for creation.
  TpmChallengeKeySubtle() = default;

  // Restores internal state of the object as if it would be after
  // |StartPrepareKeyStep|.
  virtual void RestorePreparedKeyState(
      AttestationKeyType key_type,
      const std::string& key_name,
      Profile* profile,
      const std::string& key_name_for_spkac) = 0;
};

//================= TpmChallengeKeySubtleImpl ==================================

class TpmChallengeKeySubtleImpl final : public TpmChallengeKeySubtle {
 public:
  // Use TpmChallengeKeySubtleFactory for creation.
  TpmChallengeKeySubtleImpl();
  // Use only for testing.
  explicit TpmChallengeKeySubtleImpl(
      AttestationFlow* attestation_flow_for_testing);

  TpmChallengeKeySubtleImpl(const TpmChallengeKeySubtleImpl&) = delete;
  TpmChallengeKeySubtleImpl& operator=(const TpmChallengeKeySubtleImpl&) =
      delete;
  ~TpmChallengeKeySubtleImpl() override;

  // TpmChallengeKeySubtle
  void StartPrepareKeyStep(AttestationKeyType key_type,
                           const std::string& key_name,
                           Profile* profile,
                           const std::string& key_name_for_spkac,
                           TpmChallengeKeyCallback callback) override;
  void StartSignChallengeStep(const std::string& challenge,
                              bool include_signed_public_key,
                              TpmChallengeKeyCallback callback) override;
  void StartRegisterKeyStep(TpmChallengeKeyCallback callback) override;

 private:
  // TpmChallengeKeySubtle
  void RestorePreparedKeyState(AttestationKeyType key_type,
                               const std::string& key_name,
                               Profile* profile,
                               const std::string& key_name_for_spkac) override;

  void PrepareUserKey();
  void PrepareMachineKey();

  // Returns true if the user is managed and is affiliated with the domain the
  // device is enrolled to.
  bool IsUserAffiliated() const;
  // Returns true if remote attestation is allowed and the setting is managed.
  bool IsRemoteAttestationEnabledForUser() const;

  // Returns the enterprise domain the device is enrolled to or user email.
  std::string GetEmail() const;
  AttestationCertificateProfile GetCertificateProfile() const;
  std::string GetKeyNameForRegister() const;
  const user_manager::User* GetUser() const;
  AccountId GetAccountId() const;

  // Actually prepares a key after all checks are passed.
  void PrepareKey();
  // Returns a public key (or an error) via |prepare_key_callback_|.
  void PrepareKeyFinished(
      base::Optional<CryptohomeClient::TpmAttestationDataResult>
          prepare_key_result);

  void SignChallengeCallback(bool success, const std::string& response);

  void RegisterKeyCallback(bool success, cryptohome::MountError return_code);

  // Returns a trusted value from CrosSettings indicating if the device
  // attestation is enabled.
  void GetDeviceAttestationEnabled(
      const base::RepeatingCallback<void(bool)>& callback);
  void GetDeviceAttestationEnabledCallback(bool enabled);

  void IsAttestationPreparedCallback(base::Optional<bool> result);
  void PrepareKeyErrorHandlerCallback(base::Optional<bool> is_tpm_enabled);
  void DoesKeyExistCallback(base::Optional<bool> result);
  void AskForUserConsent(base::OnceCallback<void(bool)> callback) const;
  void AskForUserConsentCallback(bool result);
  void GetCertificateCallback(AttestationStatus status,
                              const std::string& pem_certificate_chain);
  void GetPublicKey();

  // Runs |callback_| and resets it. Resetting it in this function and checking
  // it in public functions prevents simultaneous calls on the same object.
  // |this| may be destructed during the |callback_| run.
  void RunCallback(const TpmChallengeKeyResult& result);

  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  AttestationFlow* attestation_flow_ = nullptr;

  TpmChallengeKeyCallback callback_;
  Profile* profile_ = nullptr;

  AttestationKeyType key_type_ = AttestationKeyType::KEY_DEVICE;

  std::string key_name_;
  std::string key_name_for_spkac_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TpmChallengeKeySubtleImpl> weak_factory_{this};
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_SUBTLE_H_
