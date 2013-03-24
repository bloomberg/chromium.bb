// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/util/data_encryption_win.h"

#include <windows.h>
#include <wincrypt.h>

#include <cstddef>

#include "base/logging.h"

#pragma comment(lib, "crypt32.lib")

// TODO(akalin): Merge this with similar code in
// components/webdata/encryptor/encryptor_win.cc.  Preferably, all
// this stuff would live in crypto/.

namespace syncer {

std::vector<uint8> EncryptData(const std::string& data) {
  DATA_BLOB unencrypted_data = { 0 };
  unencrypted_data.pbData = (BYTE*)(data.data());
  unencrypted_data.cbData = data.size();
  DATA_BLOB encrypted_data = { 0 };

  if (!CryptProtectData(&unencrypted_data, L"", NULL, NULL, NULL, 0,
                        &encrypted_data))
    LOG(ERROR) << "Encryption fails: " << data;

  std::vector<uint8> result(encrypted_data.pbData,
                            encrypted_data.pbData + encrypted_data.cbData);
  LocalFree(encrypted_data.pbData);
  return result;
}

bool DecryptData(const std::vector<uint8>& in_data, std::string* out_data) {
  DATA_BLOB encrypted_data, decrypted_data;
  encrypted_data.pbData =
    (in_data.empty() ? NULL : const_cast<BYTE*>(&in_data[0]));
  encrypted_data.cbData = in_data.size();
  LPWSTR descrip = L"";

  if (!CryptUnprotectData(&encrypted_data, &descrip, NULL, NULL, NULL, 0,
                          &decrypted_data)) {
    LOG(ERROR) << "Decryption fails: ";
    return false;
  } else {
    out_data->assign(reinterpret_cast<const char*>(decrypted_data.pbData),
                     decrypted_data.cbData);
    LocalFree(decrypted_data.pbData);
    return true;
  }
}

}  // namespace syncer
