// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Stolen from chrome/browser/component_updater/component_unpacker.cc

#include "chrome/chrome_cleaner/components/component_unpacker.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "chrome/chrome_cleaner/components/crx_file.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "crypto/secure_hash.h"
#include "crypto/signature_verifier.h"
#include "third_party/zlib/google/zip.h"

using crypto::SecureHash;

namespace chrome_cleaner {

namespace {

// This class makes sure that the CRX digital signature is valid and well
// formed.
class CRXValidator {
 public:
  explicit CRXValidator(FILE* crx_file) : valid_(false) {
    CrxFile::Header header;
    size_t len = fread(&header, 1, sizeof(header), crx_file);
    if (len < sizeof(header))
      return;

    CrxFile::Error error;
    std::unique_ptr<CrxFile> crx(CrxFile::Parse(header, &error));
    if (!crx.get())
      return;
    DCHECK(!CrxFile::HeaderIsDelta(header));

    std::vector<uint8_t> key(header.key_size);
    len = fread(&key[0], sizeof(uint8_t), header.key_size, crx_file);
    if (len < header.key_size)
      return;

    std::vector<uint8_t> signature(header.signature_size);
    len =
        fread(&signature[0], sizeof(uint8_t), header.signature_size, crx_file);
    if (len < header.signature_size)
      return;

    crypto::SignatureVerifier verifier;
    if (!verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA1,
                             signature, key)) {
      // Signature verification initialization failed. This is most likely
      // caused by a public key in the wrong format (should encode algorithm).
      return;
    }

    const size_t kBufSize = 8 * 1024;
    std::vector<uint8_t> buf(kBufSize);
    while ((len = fread(buf.data(), 1, kBufSize, crx_file)) > 0)
      verifier.VerifyUpdate(buf);

    if (!verifier.VerifyFinal())
      return;

    public_key_.swap(key);
    valid_ = true;
  }

  bool valid() const { return valid_; }

  const std::vector<uint8_t>& public_key() const { return public_key_; }

 private:
  bool valid_;
  std::vector<uint8_t> public_key_;
};

}  // namespace

ComponentUnpacker::ComponentUnpacker(const std::vector<uint8_t>& pk_hash,
                                     const base::FilePath& path)
    : pk_hash_(pk_hash), path_(path) {}

bool ComponentUnpacker::Unpack(const base::FilePath& ouput_folder) {
  return Verify() && zip::Unzip(path_, ouput_folder);
}

bool ComponentUnpacker::Verify() {
  if (pk_hash_.empty() || path_.empty())
    return false;
  // First, validate the CRX header and signature. As of today this is SHA1 with
  // RSA 1024.
  base::ScopedFILE file(base::OpenFile(path_, "rb"));
  if (!file.get())
    return false;
  CRXValidator validator(file.get());
  file.reset();
  if (!validator.valid())
    return false;

  // File is valid and the digital signature matches. Now make sure the public
  // key hash matches the expected hash. If they do we fully trust this CRX.
  uint8_t hash[32] = {};
  std::unique_ptr<SecureHash> sha256(SecureHash::Create(SecureHash::SHA256));
  sha256->Update(&(validator.public_key()[0]), validator.public_key().size());
  sha256->Finish(hash, base::size(hash));

  if (!std::equal(pk_hash_.begin(), pk_hash_.end(), hash)) {
    LOG(WARNING) << "Hash mismatch: " << SanitizePath(path_);
    return false;
  }
  return true;
}

ComponentUnpacker::~ComponentUnpacker() {}

}  // namespace chrome_cleaner
