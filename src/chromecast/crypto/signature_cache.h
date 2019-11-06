// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRYPTO_SIGNATURE_CACHE_H_
#define CHROMECAST_CRYPTO_SIGNATURE_CACHE_H_

#include <string>

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"

namespace chromecast {

// Cache the signatures of hashes corresponding to a particular private key
// (only the most recent one for which a signature was cached). We expect that
// normally the same private key will be used always, however we also
// accommodate the case where the key is changed and a new key becomes current.
// In this case we flush the cache and start over with the new key.
class SignatureCache {
 public:
  SignatureCache();
  ~SignatureCache();

  std::string Get(const std::string& wrapped_private_key,
                  const std::string& hash);

  void Put(const std::string& wrapped_private_key,
           const std::string& hash,
           const std::string& signature);

 private:
  std::string key_;
  base::Lock lock_;
  base::HashingMRUCache<std::string, std::string> contents_;

  DISALLOW_COPY_AND_ASSIGN(SignatureCache);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRYPTO_SIGNATURE_CACHE_H_
