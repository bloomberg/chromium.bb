// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/opentype/FontSettings.h"

#include "wtf/HashFunctions.h"
#include "wtf/StringHasher.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

namespace blink {

uint32_t atomicStringToFourByteTag(AtomicString tag) {
  DCHECK_EQ(tag.length(), 4u);
  return (((tag[0]) << 24) | ((tag[1]) << 16) | ((tag[2]) << 8) | (tag[3]));
}

static inline void addToHash(unsigned& hash, unsigned key) {
  hash = ((hash << 5) + hash) + key;  // Djb2
};

static inline void addFloatToHash(unsigned& hash, float value) {
  addToHash(hash, WTF::FloatHash<float>::hash(value));
};

unsigned FontVariationSettings::hash() const {
  unsigned computedHash = size() ? 5381 : 0;
  unsigned numFeatures = size();
  for (unsigned i = 0; i < numFeatures; ++i) {
    StringHasher stringHasher;
    const AtomicString& tag = at(i).tag();
    for (unsigned j = 0; j < tag.length(); j++) {
      stringHasher.addCharacter(tag[j]);
    }
    addToHash(computedHash, stringHasher.hash());
    addFloatToHash(computedHash, at(i).value());
  }
  return computedHash;
}

}  // namespace blink
