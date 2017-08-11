// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntegrityMetadata_h
#define IntegrityMetadata_h

#include "platform/PlatformExport.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/StringHasher.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class IntegrityMetadata;
enum class IntegrityAlgorithm : uint8_t;

using IntegrityMetadataPair = std::pair<String, IntegrityAlgorithm>;
using IntegrityMetadataSet = WTF::HashSet<IntegrityMetadataPair>;

class PLATFORM_EXPORT IntegrityMetadata {
 public:
  IntegrityMetadata() {}
  IntegrityMetadata(String digest, IntegrityAlgorithm);
  IntegrityMetadata(IntegrityMetadataPair);

  String Digest() const { return digest_; }
  void SetDigest(const String& digest) { digest_ = digest; }
  IntegrityAlgorithm Algorithm() const { return algorithm_; }
  void SetAlgorithm(IntegrityAlgorithm algorithm) { algorithm_ = algorithm; }

  IntegrityMetadataPair ToPair() const;

  static bool SetsEqual(const IntegrityMetadataSet& set1,
                        const IntegrityMetadataSet& set2);

 private:
  String digest_;
  IntegrityAlgorithm algorithm_;
};

enum class ResourceIntegrityDisposition : uint8_t {
  kNotChecked = 0,
  kFailed,
  kPassed
};

enum class IntegrityAlgorithm : uint8_t { kSha256, kSha384, kSha512 };

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::IntegrityAlgorithm> {
  STATIC_ONLY(DefaultHash);
  typedef IntHash<blink::IntegrityAlgorithm> Hash;
};

template <>
struct HashTraits<blink::IntegrityAlgorithm>
    : UnsignedWithZeroKeyHashTraits<blink::IntegrityAlgorithm> {
  STATIC_ONLY(HashTraits);
};

}  // namespace WTF

#endif
