// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_CRYPTOGRAPHER_PROVIDER_H_
#define SYNC_ENGINE_CRYPTOGRAPHER_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"

namespace syncer {

class Cryptographer;
class ScopedCryptographerRef;
class ScopedCryptographerInternal;

// An interface for providing clients with a ScopedCryptographerRef.
//
// Used to expose the syncable::Directory's cryptographer to clients that
// would otherwise not have access to the Directory.
class SYNC_EXPORT_PRIVATE CryptographerProvider {
 public:
  CryptographerProvider();
  virtual ~CryptographerProvider();

  virtual bool InitScopedCryptographerRef(ScopedCryptographerRef* scoped) = 0;
};

// A concrete class representing a reference to a cryptographer.
//
// Access to the cryptographer is lost when this class goes out of scope.
class SYNC_EXPORT_PRIVATE ScopedCryptographerRef {
 public:
  ScopedCryptographerRef();
  ~ScopedCryptographerRef();

  bool Initialize(ScopedCryptographerInternal* impl);
  bool IsValid() const;
  Cryptographer* Get() const;

 private:
  scoped_ptr<ScopedCryptographerInternal> scoped_cryptographer_internal_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCryptographerRef);
};

// An interface class used in the implementation of the ScopedCryptographerRef.
//
// We use this class to allow different implementations of
// CryptographerProvider to implement InitScopedCryptographer in different
// ways.  The ScopedCryptographerRef itself must be stack-allocated, so it
// can't vary depending on which kind of CryptographerProvider is used to
// intialize it.
class SYNC_EXPORT_PRIVATE ScopedCryptographerInternal {
 public:
  ScopedCryptographerInternal();
  virtual ~ScopedCryptographerInternal();

  virtual Cryptographer* Get() const = 0;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_CRYPTOGRAPHER_PROVIDER_H_
