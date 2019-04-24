// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/padding_key.h"

#include "base/no_destructor.h"

using crypto::SymmetricKey;

namespace storage {

namespace {

const SymmetricKey::Algorithm kPaddingKeyAlgorithm = SymmetricKey::AES;

std::unique_ptr<SymmetricKey>* GetPaddingKey() {
  static base::NoDestructor<std::unique_ptr<SymmetricKey>> s_padding_key([] {
    return SymmetricKey::GenerateRandomKey(kPaddingKeyAlgorithm, 128);
  }());
  return s_padding_key.get();
}

}  // namespace

std::unique_ptr<SymmetricKey> CopyDefaultPaddingKey() {
  return SymmetricKey::Import(kPaddingKeyAlgorithm, (*GetPaddingKey())->key());
}

std::unique_ptr<SymmetricKey> DeserializePaddingKey(
    const std::string& raw_key) {
  return SymmetricKey::Import(kPaddingKeyAlgorithm, raw_key);
}

std::string SerializeDefaultPaddingKey() {
  return (*GetPaddingKey())->key();
}

void ResetPaddingKeyForTesting() {
  *GetPaddingKey() = SymmetricKey::GenerateRandomKey(kPaddingKeyAlgorithm, 128);
}

}  // namespace storage
