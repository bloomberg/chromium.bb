// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_BITSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_BITSET_H_

#include <algorithm>
#include <cstring>
#include <initializer_list>

#include "base/bits.h"
#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"

namespace blink {

// A bitset designed for CSSPropertyIDs.
//
// It's different from std::bitset, in that it provides optimized traversal
// for situations where only a few bits are set (which is the common case for
// e.g. CSS declarations which apply to an element).
//
// The bitset can store a configurable amount of bits for testing purposes,
// (though not more than numCSSProperties).
template <size_t kBits>
class CORE_EXPORT CSSBitsetBase {
 public:
  static_assert(
      kBits <= kNumCSSProperties,
      "Bit count must not exceed numCSSProperties, as each bit position must "
      "be representable as a CSSPropertyID");

  static const size_t kChunks = (kBits + 63) / 64;

  CSSBitsetBase() : chunks_() {}
  CSSBitsetBase(const CSSBitsetBase<kBits>& o) { *this = o; }
  CSSBitsetBase(std::initializer_list<CSSPropertyID> list) : chunks_() {
    for (CSSPropertyID id : list)
      Set(id);
  }

  void operator=(const CSSBitsetBase& o) {
    std::memcpy(chunks_, o.chunks_, sizeof(chunks_));
  }

  bool operator==(const CSSBitsetBase& o) const {
    return std::memcmp(chunks_, o.chunks_, sizeof(chunks_)) == 0;
  }
  bool operator!=(const CSSBitsetBase& o) const { return !(*this == o); }

  inline void Set(CSSPropertyID id) {
    size_t bit = static_cast<size_t>(id);
    DCHECK_LT(bit, kBits);
    chunks_[bit / 64] |= (1ull << (bit % 64));
  }

  inline void Or(CSSPropertyID id, bool v) {
    size_t bit = static_cast<size_t>(id);
    DCHECK_LT(bit, kBits);
    chunks_[bit / 64] |= (static_cast<uint64_t>(v) << (bit % 64));
  }

  inline bool Has(CSSPropertyID id) const {
    size_t bit = static_cast<size_t>(id);
    DCHECK_LT(bit, kBits);
    return chunks_[bit / 64] & (1ull << (bit % 64));
  }

  inline bool HasAny() const {
    for (uint64_t chunk : chunks_) {
      if (chunk)
        return true;
    }
    return false;
  }

  inline void Reset() { std::memset(chunks_, 0, sizeof(chunks_)); }

  // Yields the CSSPropertyIDs which are set.
  class Iterator {
   public:
    // Only meant for internal use (from begin() or end()).
    Iterator(const uint64_t* chunks, size_t chunk_index, size_t index)
        : chunks_(chunks),
          index_(index),
          chunk_index_(chunk_index),
          chunk_(chunks_[0]) {
      DCHECK(index == 0 || index == kBits);
      if (index < kBits) {
        ++*this;  // Go to the first set bit.
      }
    }

    inline void operator++() {
      // If there are no more bits set in this chunk,
      // skip to the next nonzero chunk (if any exists).
      while (!chunk_) {
        if (++chunk_index_ >= kChunks) {
          index_ = kBits;
          return;
        }
        chunk_ = chunks_[chunk_index_];
      }
      index_ = chunk_index_ * 64 + base::bits::CountTrailingZeroBits(chunk_);
      chunk_ &= chunk_ - 1;  // Clear the lowest bit.
    }

    inline CSSPropertyID operator*() const {
      DCHECK_LT(index_, static_cast<size_t>(kNumCSSProperties));
      return static_cast<CSSPropertyID>(index_);
    }

    inline bool operator==(const Iterator& o) const {
      return index_ == o.index_;
    }
    inline bool operator!=(const Iterator& o) const {
      return index_ != o.index_;
    }

   private:
    const uint64_t* chunks_;
    // The current bit index this Iterator is pointing to. Note that this is
    // the "global" index, i.e. it has the range [0, kBits]. (It is not a local
    // index with range [0, 64]).
    //
    // Never exceeds kBits.
    size_t index_ = 0;
    // The current chunk index this Iterator is pointing to.
    // Points to kChunks if we are done.
    size_t chunk_index_ = 0;
    // The iterator works by "pre-fetching" the current chunk (corresponding
    // (to the current index), and removing its bits one by one.
    // This is not used (contains junk) for the end() iterator.
    uint64_t chunk_ = 0;
  };

  Iterator begin() const { return Iterator(chunks_, 0, 0); }
  Iterator end() const { return Iterator(chunks_, kChunks, kBits); }

 private:
  uint64_t chunks_[kChunks];
};

using CSSBitset = CSSBitsetBase<kNumCSSProperties>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_BITSET_H_
