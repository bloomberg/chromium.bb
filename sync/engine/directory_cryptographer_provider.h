// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_DIRECTORY_CRYPTOGRAPHER_PROVIDER_H_
#define SYNC_ENGINE_DIRECTORY_CRYPTOGRAPHER_PROVIDER_H_

#include "sync/engine/cryptographer_provider.h"

#include "sync/syncable/syncable_read_transaction.h"

namespace syncer {

namespace syncable {
class Directory;
}

// Provides access to the Directory's cryptographer through the
// CryptographerProvider interface.
class DirectoryCryptographerProvider : public CryptographerProvider {
 public:
  DirectoryCryptographerProvider(syncable::Directory* dir);
  virtual ~DirectoryCryptographerProvider();

  virtual bool InitScopedCryptographerRef(
      ScopedCryptographerRef* scoped) OVERRIDE;

 private:
  syncable::Directory* dir_;
};

// An implementation detail of the DirectoryCryptographerProvider.  Contains
// the ReadTransaction used to safely access the Cryptographer.
class ScopedDirectoryCryptographerInternal
    : public ScopedCryptographerInternal {
 public:
  ScopedDirectoryCryptographerInternal(syncable::Directory* dir);
  virtual ~ScopedDirectoryCryptographerInternal();

  virtual Cryptographer* Get() const OVERRIDE;

 private:
  syncable::Directory* dir_;
  syncable::ReadTransaction trans_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDirectoryCryptographerInternal);
};

}  // namespace syncer

#endif  // SYNC_ENGINE_DIRECTORY_CRYPTOGRAPHER_PROVIDER_H_
