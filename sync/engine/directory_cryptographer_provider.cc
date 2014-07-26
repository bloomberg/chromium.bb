// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/directory_cryptographer_provider.h"

#include "sync/syncable/directory.h"
#include "sync/syncable/syncable_read_transaction.h"

namespace syncer {

DirectoryCryptographerProvider::DirectoryCryptographerProvider(
    syncable::Directory* dir)
    : dir_(dir) {
}

DirectoryCryptographerProvider::~DirectoryCryptographerProvider() {
}

bool DirectoryCryptographerProvider::InitScopedCryptographerRef(
    ScopedCryptographerRef* scoped) {
  scoped->Initialize(new ScopedDirectoryCryptographerInternal(dir_));
  return scoped->IsValid();
}

ScopedDirectoryCryptographerInternal::ScopedDirectoryCryptographerInternal(
    syncable::Directory* dir)
    : dir_(dir), trans_(FROM_HERE, dir) {
}

ScopedDirectoryCryptographerInternal::~ScopedDirectoryCryptographerInternal() {
}

Cryptographer* ScopedDirectoryCryptographerInternal::Get() const {
  return dir_->GetCryptographer(&trans_);
}

}  // namespace syncer
