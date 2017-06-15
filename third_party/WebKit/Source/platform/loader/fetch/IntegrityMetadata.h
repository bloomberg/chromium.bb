// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntegrityMetadata_h
#define IntegrityMetadata_h

#include "platform/Crypto.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/StringHasher.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class IntegrityMetadata;

using IntegrityMetadataPair = std::pair<WTF::String, HashAlgorithm>;
using IntegrityMetadataSet = WTF::HashSet<IntegrityMetadataPair>;

class PLATFORM_EXPORT IntegrityMetadata {
 public:
  IntegrityMetadata() {}
  IntegrityMetadata(WTF::String digest, HashAlgorithm);
  IntegrityMetadata(std::pair<WTF::String, HashAlgorithm>);

  WTF::String Digest() const { return digest_; }
  void SetDigest(const WTF::String& digest) { digest_ = digest; }
  HashAlgorithm Algorithm() const { return algorithm_; }
  void SetAlgorithm(HashAlgorithm algorithm) { algorithm_ = algorithm; }

  IntegrityMetadataPair ToPair() const;

  static bool SetsEqual(const IntegrityMetadataSet& set1,
                        const IntegrityMetadataSet& set2);

 private:
  WTF::String digest_;
  HashAlgorithm algorithm_;
};

enum class ResourceIntegrityDisposition : uint8_t {
  kNotChecked = 0,
  kFailed,
  kPassed
};

}  // namespace blink

#endif
