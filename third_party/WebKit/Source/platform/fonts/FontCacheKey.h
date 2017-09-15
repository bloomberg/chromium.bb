/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FontCacheKey_h
#define FontCacheKey_h

#include "platform/fonts/FontFaceCreationParams.h"
#include "platform/fonts/opentype/FontSettings.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashTableDeletedValueType.h"
#include "platform/wtf/text/AtomicStringHash.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

// Multiplying the floating point size by 100 gives two decimal point
// precision which should be sufficient.
static const unsigned kFontSizePrecisionMultiplier = 100;

struct FontCacheKey {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  FontCacheKey() : creation_params_(), font_size_(0), options_(0) {}
  FontCacheKey(FontFaceCreationParams creation_params,
               float font_size,
               unsigned options,
               RefPtr<FontVariationSettings> variation_settings)
      : creation_params_(creation_params),
        font_size_(font_size * kFontSizePrecisionMultiplier),
        options_(options),
        variation_settings_(std::move(variation_settings)) {}

  FontCacheKey(WTF::HashTableDeletedValueType)
      : font_size_(HashTableDeletedSize()) {}

  unsigned GetHash() const {
    unsigned hash_codes[4] = {
        creation_params_.GetHash(), font_size_, options_,
        variation_settings_ ? variation_settings_->GetHash() : 0};
    return StringHasher::HashMemory<sizeof(hash_codes)>(hash_codes);
  }

  bool operator==(const FontCacheKey& other) const {
    return creation_params_ == other.creation_params_ &&
           font_size_ == other.font_size_ && options_ == other.options_ &&
           variation_settings_ == other.variation_settings_;
  }

  bool IsHashTableDeletedValue() const {
    return font_size_ == HashTableDeletedSize();
  }

  static unsigned PrecisionMultiplier() { return kFontSizePrecisionMultiplier; }

  void ClearFontSize() { font_size_ = 0; }

 private:
  static unsigned HashTableDeletedSize() { return 0xFFFFFFFFU; }

  FontFaceCreationParams creation_params_;
  unsigned font_size_;
  unsigned options_;
  RefPtr<FontVariationSettings> variation_settings_;
};

struct FontCacheKeyHash {
  STATIC_ONLY(FontCacheKeyHash);
  static unsigned GetHash(const FontCacheKey& key) { return key.GetHash(); }

  static bool Equal(const FontCacheKey& a, const FontCacheKey& b) {
    return a == b;
  }

  static const bool safe_to_compare_to_empty_or_deleted = true;
};

struct FontCacheKeyTraits : WTF::SimpleClassHashTraits<FontCacheKey> {
  STATIC_ONLY(FontCacheKeyTraits);
};

}  // namespace blink

#endif  // FontCacheKey_h
