// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/rsa_key_pair.h"

#include <limits>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "crypto/rsa_private_key.h"
#include "crypto/signature_creator.h"
#include "net/cert/x509_util.h"

namespace remoting {

RsaKeyPair::RsaKeyPair(scoped_ptr<crypto::RSAPrivateKey> key)
    : key_(key.Pass()){
  DCHECK(key_);
}

RsaKeyPair::~RsaKeyPair() {}

//static
scoped_refptr<RsaKeyPair> RsaKeyPair::Generate() {
  scoped_ptr<crypto::RSAPrivateKey> key(crypto::RSAPrivateKey::Create(2048));
  if (!key) {
    LOG(ERROR) << "Cannot generate private key.";
    return NULL;
  }
  return new RsaKeyPair(key.Pass());
}

//static
scoped_refptr<RsaKeyPair> RsaKeyPair::FromString(
    const std::string& key_base64) {
  std::string key_str;
  if (!base::Base64Decode(key_base64, &key_str)) {
    LOG(ERROR) << "Failed to decode private key.";
    return NULL;
  }

  std::vector<uint8> key_buf(key_str.begin(), key_str.end());
  scoped_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_buf));
  if (!key) {
    LOG(ERROR) << "Invalid private key.";
    return NULL;
  }

  return new RsaKeyPair(key.Pass());
}

std::string RsaKeyPair::ToString() const {
  // Check that the key initialized.
  DCHECK(key_.get() != NULL);

  std::vector<uint8> key_buf;
  CHECK(key_->ExportPrivateKey(&key_buf));
  std::string key_str(key_buf.begin(), key_buf.end());
  std::string key_base64;
  base::Base64Encode(key_str, &key_base64);
  return key_base64;
}

std::string RsaKeyPair::GetPublicKey() const {
  std::vector<uint8> public_key;
  CHECK(key_->ExportPublicKey(&public_key));
  std::string public_key_str(public_key.begin(), public_key.end());
  std::string public_key_base64;
  base::Base64Encode(public_key_str, &public_key_base64);
  return public_key_base64;
}

std::string RsaKeyPair::SignMessage(const std::string& message) const {
  scoped_ptr<crypto::SignatureCreator> signature_creator(
      crypto::SignatureCreator::Create(key_.get()));
  signature_creator->Update(reinterpret_cast<const uint8*>(message.c_str()),
                            message.length());
  std::vector<uint8> signature_buf;
  signature_creator->Final(&signature_buf);
  std::string signature_str(signature_buf.begin(), signature_buf.end());
  std::string signature_base64;
  base::Base64Encode(signature_str, &signature_base64);
  return signature_base64;
}

std::string RsaKeyPair::GenerateCertificate() const {
  std::string der_cert;
  // Certificates are SHA1-signed because |key_| has likely been used to sign
  // with SHA1 previously, and you should not re-use a key for signing data with
  // multiple signature algorithms.
  net::x509_util::CreateSelfSignedCert(
      key_.get(),
      net::x509_util::DIGEST_SHA1,
      "CN=chromoting",
      base::RandInt(1, std::numeric_limits<int>::max()),
      base::Time::Now(),
      base::Time::Now() + base::TimeDelta::FromDays(1),
      &der_cert);
  return der_cert;
}

}  // namespace remoting
