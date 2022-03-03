// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_SIGNATURE_PROVIDER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_SIGNATURE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace crypto {
class RSAPrivateKey;
}  // namespace crypto

namespace policy {

// Provides access to predefined test signing keys and allows for data signing
// using those keys. Keys are indexed and retrieved by 1-based key versions.
class SignatureProvider {
 public:
  // Provides access to a predefined test signing key.
  class SigningKey {
   public:
    SigningKey(std::unique_ptr<crypto::RSAPrivateKey> private_key,
               const std::map<std::string, std::string>& signatures);
    SigningKey(SigningKey&& signing_key);
    SigningKey& operator=(SigningKey&& signing_key);
    ~SigningKey();

    // Looks up the domain's signature in the passed dictionary. Returns true if
    // domain is in |signatures_| or false otherwise.
    bool GetSignatureForDomain(const std::string& domain,
                               std::string* signature) const;

    // Signs |str| using the private key.
    bool Sign(const std::string& str, std::string* signature) const;

    const std::string& public_key() const { return public_key_; }

   private:
    // The key used for signing.
    std::unique_ptr<crypto::RSAPrivateKey> private_key_;

    // The public key corresponding to |private_key_|.
    std::string public_key_;

    // Maps domains to the corresponding signatures.
    std::map<std::string, std::string> signatures_;
  };

  // Domains with pre-computed signatures.
  static constexpr char kTestDomain1[] = "example.com";
  static constexpr char kTestDomain2[] = "chromepolicytest.com";
  static constexpr char kTestDomain3[] = "managedchrome.com";

  SignatureProvider();
  SignatureProvider(SignatureProvider&& signature_provider);
  SignatureProvider& operator=(SignatureProvider&& signature_provider);
  virtual ~SignatureProvider();

  // Returns the key corresponding to |key_version| (1-based) or nullptr if
  // |key_version| is out-of-bounds. Used when a key version is specified by the
  // client.
  const SigningKey* GetKeyByVersion(int key_version) const;

  // Shortcut for |GetKeyByVersion(current_key_version_)|, used when the client
  // doesn't specify the key version to be used.
  const SigningKey* GetCurrentKey() const;

  const std::vector<SigningKey>& signing_keys() const { return signing_keys_; }
  void set_signing_keys(std::vector<SigningKey> signing_keys) {
    signing_keys_ = std::move(signing_keys);
  }

  int current_key_version() const { return current_key_version_; }
  void set_current_key_version(int current_key_version) {
    current_key_version_ = current_key_version;
  }

 private:
  std::vector<SigningKey> signing_keys_;

  // The key version to be used if no key version is defined by the client.
  int current_key_version_ = 1;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_SIGNATURE_PROVIDER_H_
