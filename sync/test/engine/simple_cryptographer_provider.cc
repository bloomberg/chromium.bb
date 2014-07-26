// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/simple_cryptographer_provider.h"

namespace syncer {

SimpleCryptographerProvider::SimpleCryptographerProvider(
    Cryptographer* cryptographer)
    : cryptographer_(cryptographer) {
}

SimpleCryptographerProvider::~SimpleCryptographerProvider() {
}

bool SimpleCryptographerProvider::InitScopedCryptographerRef(
    ScopedCryptographerRef* scoped) {
  scoped->Initialize(new SimpleScopedCryptographerInternal(cryptographer_));
  return scoped->IsValid();
}

SimpleScopedCryptographerInternal::SimpleScopedCryptographerInternal(
    Cryptographer* cryptographer)
    : cryptographer_(cryptographer) {
}

SimpleScopedCryptographerInternal::~SimpleScopedCryptographerInternal() {
}

Cryptographer* SimpleScopedCryptographerInternal::Get() const {
  return cryptographer_;
}

}  // namespace syncer
