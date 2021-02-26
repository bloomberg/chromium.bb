// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_KEYSTORE_SERVICE_ASH_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_KEYSTORE_SERVICE_ASH_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace attestation {
class TpmChallengeKey;
struct TpmChallengeKeyResult;
}  // namespace attestation
}  // namespace chromeos

namespace crosapi {

// This class is the ash implementation of the KeystoreService crosapi. It
// allows lacros to expose blessed extension APIs which can query or modify the
// system keystores. This class is affine to the UI thread.
class KeystoreServiceAsh : public mojom::KeystoreService {
 public:
  explicit KeystoreServiceAsh(
      mojo::PendingReceiver<mojom::KeystoreService> receiver);
  KeystoreServiceAsh(const KeystoreServiceAsh&) = delete;
  KeystoreServiceAsh& operator=(const KeystoreServiceAsh&) = delete;
  ~KeystoreServiceAsh() override;

  // mojom::KeystoreService:
  using KeystoreType = mojom::KeystoreType;
  void ChallengeAttestationOnlyKeystore(
      const std::string& challenge,
      mojom::KeystoreType type,
      bool migrate,
      ChallengeAttestationOnlyKeystoreCallback callback) override;
  void GetKeyStores(GetKeyStoresCallback callback) override;
  void GetCertificates(mojom::KeystoreType keystore,
                       GetCertificatesCallback callback) override;
  void GenerateKey(mojom::KeystoreType keystore,
                   mojom::KeystoreSigningAlgorithmPtr algorithm,
                   GenerateKeyCallback callback) override;
  void AddCertificate(mojom::KeystoreType keystore,
                      const std::vector<uint8_t>& certificate,
                      AddCertificateCallback callback) override;
  void RemoveCertificate(mojom::KeystoreType keystore,
                         const std::vector<uint8_t>& certificate,
                         RemoveCertificateCallback callback) override;

 private:
  static void OnGetTokens(
      GetKeyStoresCallback callback,
      std::unique_ptr<std::vector<chromeos::platform_keys::TokenId>>
          platform_keys_token_ids,
      chromeos::platform_keys::Status status);
  static void OnGetCertificates(GetCertificatesCallback callback,
                                std::unique_ptr<net::CertificateList> certs,
                                chromeos::platform_keys::Status status);
  static void OnGenerateKey(GenerateKeyCallback callback,
                            const std::string& public_key,
                            chromeos::platform_keys::Status status);
  static void OnImportCertificate(AddCertificateCallback callback,
                                  chromeos::platform_keys::Status status);
  static void OnRemoveCertificate(RemoveCertificateCallback callback,
                                  chromeos::platform_keys::Status status);

  // |challenge| is used as a opaque identifier to match against the unique_ptr
  // in outstanding_challenges_. It should not be dereferenced.
  void DidChallengeAttestationOnlyKeystore(
      ChallengeAttestationOnlyKeystoreCallback callback,
      void* challenge,
      const chromeos::attestation::TpmChallengeKeyResult& result);

  // Container to keep outstanding challenges alive.
  std::vector<std::unique_ptr<chromeos::attestation::TpmChallengeKey>>
      outstanding_challenges_;
  mojo::Receiver<mojom::KeystoreService> receiver_;

  base::WeakPtrFactory<KeystoreServiceAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_KEYSTORE_SERVICE_ASH_H_
