// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/owner_key_util_impl.h"

#include <keythi.h>
#include <limits>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "crypto/nss_key_util.h"

namespace ownership {

OwnerKeyUtilImpl::OwnerKeyUtilImpl(const base::FilePath& public_key_file)
    : public_key_file_(public_key_file) {
}

OwnerKeyUtilImpl::~OwnerKeyUtilImpl() {
}

bool OwnerKeyUtilImpl::ImportPublicKey(std::vector<uint8_t>* output) {
  // Get the file size (must fit in a 32 bit int for NSS).
  int64_t file_size;
  if (!base::GetFileSize(public_key_file_, &file_size)) {
#if defined(OS_CHROMEOS)
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "Could not get size of " << public_key_file_.value();
#endif  // defined(OS_CHROMEOS)
    return false;
  }
  if (file_size > static_cast<int64_t>(std::numeric_limits<int>::max())) {
    LOG(ERROR) << public_key_file_.value() << "is " << file_size
               << "bytes!!!  Too big!";
    return false;
  }
  int32_t safe_file_size = static_cast<int32_t>(file_size);

  output->resize(safe_file_size);

  if (safe_file_size == 0) {
    LOG(WARNING) << "Public key file is empty. This seems wrong.";
    return false;
  }

  // Get the key data off of disk
  int data_read =
      base::ReadFile(public_key_file_, reinterpret_cast<char*>(output->data()),
                     safe_file_size);
  return data_read == safe_file_size;
}

crypto::ScopedSECKEYPrivateKey OwnerKeyUtilImpl::FindPrivateKeyInSlot(
    const std::vector<uint8_t>& key,
    PK11SlotInfo* slot) {
  if (!slot)
    return nullptr;

  crypto::ScopedSECKEYPrivateKey private_key(
      crypto::FindNSSKeyFromPublicKeyInfoInSlot(key, slot));
  if (!private_key || SECKEY_GetPrivateKeyType(private_key.get()) != rsaKey)
    return nullptr;
  return private_key;
}

bool OwnerKeyUtilImpl::IsPublicKeyPresent() {
  return base::PathExists(public_key_file_);
}

}  // namespace ownership
