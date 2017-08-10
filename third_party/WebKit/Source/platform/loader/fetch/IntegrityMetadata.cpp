// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/IntegrityMetadata.h"

namespace blink {

IntegrityMetadata::IntegrityMetadata(WTF::String digest,
                                     HashAlgorithm algorithm)
    : digest_(digest), algorithm_(algorithm) {}

IntegrityMetadata::IntegrityMetadata(std::pair<WTF::String, HashAlgorithm> pair)
    : digest_(pair.first), algorithm_(pair.second) {}

std::pair<WTF::String, HashAlgorithm> IntegrityMetadata::ToPair() const {
  return std::pair<WTF::String, HashAlgorithm>(digest_, algorithm_);
}

bool IntegrityMetadata::SetsEqual(const IntegrityMetadataSet& set1,
                                  const IntegrityMetadataSet& set2) {
  if (set1.size() != set2.size())
    return false;

  for (const IntegrityMetadataPair& metadata : set1) {
    if (!set2.Contains(metadata))
      return false;
  }

  return true;
}

}  // namespace blink
