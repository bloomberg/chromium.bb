// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/chromeos/attestation/tpm_challenge_key_subtle.h"
#include "chromeos/dbus/constants/attestation_constants.h"

class Profile;
class AttestationFlow;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {
namespace attestation {

//========================= TpmChallengeKeyFactory =============================

class TpmChallengeKey;

class TpmChallengeKeyFactory final {
 public:
  static std::unique_ptr<TpmChallengeKey> Create();
  static void SetForTesting(std::unique_ptr<TpmChallengeKey> next_result);

 private:
  static TpmChallengeKey* next_result_for_testing_;
};

//=========================== TpmChallengeKey ==================================

// Asynchronously runs the flow to challenge a key in the caller context. This
// is a wrapper around TpmChallengeKeySubtle with an easier-to-use interface.
// TpmChallengeKeySubtle can be used directly to get more control over main
// steps to build the response.
class TpmChallengeKey {
 public:
  TpmChallengeKey(const TpmChallengeKey&) = delete;
  TpmChallengeKey& operator=(const TpmChallengeKey&) = delete;
  virtual ~TpmChallengeKey() = default;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Should be called only once for every instance. |TpmChallengeKey| object
  // should live as long as response from |BuildResponse| function via
  // |callback| is expected. On destruction it stops challenge process and
  // silently discards callback. |key_name_for_spkac| the name of the key used
  // for SignedPublicKeyAndChallenge when sending a challenge machine key
  // request with |registerKey|=true.
  virtual void BuildResponse(AttestationKeyType key_type,
                             Profile* profile,
                             TpmChallengeKeyCallback callback,
                             const std::string& challenge,
                             bool register_key,
                             const std::string& key_name_for_spkac) = 0;

 protected:
  // Use TpmChallengeKeyFactory for creation.
  TpmChallengeKey() = default;
};

//=========================== TpmChallengeKeyImpl ==============================

class TpmChallengeKeyImpl final : public TpmChallengeKey {
 public:
  // Use TpmChallengeKeyFactory for creation.
  TpmChallengeKeyImpl();
  // Use only for testing.
  explicit TpmChallengeKeyImpl(AttestationFlow* attestation_flow_for_testing);
  TpmChallengeKeyImpl(const TpmChallengeKeyImpl&) = delete;
  TpmChallengeKeyImpl& operator=(const TpmChallengeKeyImpl&) = delete;
  ~TpmChallengeKeyImpl() override;

  // TpmChallengeKey
  void BuildResponse(AttestationKeyType key_type,
                     Profile* profile,
                     TpmChallengeKeyCallback callback,
                     const std::string& challenge,
                     bool register_key,
                     const std::string& key_name_for_spkac) override;

 private:
  void OnPrepareKeyDone(const TpmChallengeKeyResult& prepare_key_result);
  void OnSignChallengeDone(const TpmChallengeKeyResult& sign_challenge_result);
  void OnRegisterKeyDone(const TpmChallengeKeyResult& challenge_response,
                         const TpmChallengeKeyResult& register_key_result);

  bool register_key_ = false;
  std::string challenge_;
  TpmChallengeKeyResult challenge_response_;
  TpmChallengeKeyCallback callback_;
  std::unique_ptr<TpmChallengeKeySubtle> tpm_challenge_key_subtle_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<TpmChallengeKeyImpl> weak_factory_{this};
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_H_
