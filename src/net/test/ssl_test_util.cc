// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/ssl_test_util.h"

#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace net {

bssl::UniquePtr<SSL_ECH_KEYS> MakeTestEchKeys(
    const char* public_name,
    size_t max_name_len,
    std::vector<uint8_t>* ech_config_list) {
  bssl::ScopedEVP_HPKE_KEY key;
  if (!EVP_HPKE_KEY_generate(key.get(), EVP_hpke_x25519_hkdf_sha256())) {
    return nullptr;
  }

  uint8_t* ech_config;
  size_t ech_config_len;
  if (!SSL_marshal_ech_config(&ech_config, &ech_config_len,
                              /*config_id=*/1, key.get(), public_name,
                              max_name_len)) {
    return nullptr;
  }
  bssl::UniquePtr<uint8_t> scoped_ech_config(ech_config);

  uint8_t* ech_config_list_raw;
  size_t ech_config_list_len;
  bssl::UniquePtr<SSL_ECH_KEYS> keys(SSL_ECH_KEYS_new());
  if (!keys ||
      !SSL_ECH_KEYS_add(keys.get(), /*is_retry_config=*/1, ech_config,
                        ech_config_len, key.get()) ||
      !SSL_ECH_KEYS_marshal_retry_configs(keys.get(), &ech_config_list_raw,
                                          &ech_config_list_len)) {
    return nullptr;
  }
  bssl::UniquePtr<uint8_t> scoped_ech_config_list(ech_config_list_raw);

  ech_config_list->assign(ech_config_list_raw,
                          ech_config_list_raw + ech_config_list_len);
  return keys;
}

}  // namespace net
