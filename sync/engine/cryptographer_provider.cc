// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/cryptographer_provider.h"

namespace syncer {

CryptographerProvider::CryptographerProvider() {
}

CryptographerProvider::~CryptographerProvider() {
}

ScopedCryptographerRef::ScopedCryptographerRef() {
}

ScopedCryptographerRef::~ScopedCryptographerRef() {
}

bool ScopedCryptographerRef::Initialize(ScopedCryptographerInternal* impl) {
  scoped_cryptographer_internal_.reset(impl);
  return IsValid();
}

bool ScopedCryptographerRef::IsValid() const {
  return !!Get();
}

Cryptographer* ScopedCryptographerRef::Get() const {
  if (!scoped_cryptographer_internal_)
    return NULL;
  return scoped_cryptographer_internal_->Get();
}

ScopedCryptographerInternal::ScopedCryptographerInternal() {
}

ScopedCryptographerInternal::~ScopedCryptographerInternal() {
}

}  // namespace syncer
