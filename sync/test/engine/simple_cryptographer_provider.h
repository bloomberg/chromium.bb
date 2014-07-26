// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ENGINE_SIMPLE_CRYPTOGRAPHER_PROVIDER_H_
#define SYNC_TEST_ENGINE_SIMPLE_CRYPTOGRAPHER_PROVIDER_H_

#include "sync/engine/cryptographer_provider.h"

#include "sync/util/cryptographer.h"

namespace syncer {

// A trivial cryptographer provider.  Exposes the given Cryptographer through
// the CryptographerProvider interface.  Does not take ownership of the
// |cryptographer|.
class SimpleCryptographerProvider : public CryptographerProvider {
 public:
  explicit SimpleCryptographerProvider(Cryptographer* cryptographer);
  virtual ~SimpleCryptographerProvider();

  // Implementation of CryptographerProvider.
  virtual bool InitScopedCryptographerRef(
      ScopedCryptographerRef* scoped) OVERRIDE;

 private:
  Cryptographer* cryptographer_;
};

class SimpleScopedCryptographerInternal : public ScopedCryptographerInternal {
 public:
  explicit SimpleScopedCryptographerInternal(Cryptographer* cryptographer);
  virtual ~SimpleScopedCryptographerInternal();

  // Implementation of ScopedCryptographerInternal.
  virtual Cryptographer* Get() const OVERRIDE;

 private:
  Cryptographer* cryptographer_;
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_SIMPLE_CRYPTOGRAPHER_PROVIDER_H_
