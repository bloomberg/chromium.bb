/*
 * Copyright 2019 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_UTILS_ENTROPY_DECODER_H_
#define LIBGAV1_SRC_UTILS_ENTROPY_DECODER_H_

#include <cstddef>
#include <cstdint>

#include "src/utils/bit_reader.h"
#include "src/utils/compiler_attributes.h"

namespace libgav1 {

class DaalaBitReader : public BitReader {
 public:
  DaalaBitReader(const uint8_t* data, size_t size, bool allow_update_cdf);
  ~DaalaBitReader() override = default;

  // Move only.
  DaalaBitReader(DaalaBitReader&& rhs) noexcept;
  DaalaBitReader& operator=(DaalaBitReader&& rhs) noexcept;

  int ReadBit() final;
  int64_t ReadLiteral(int num_bits) override;
  // ReadSymbol() calls for which the |symbol_count| is only known at runtime
  // will use this variant.
  int ReadSymbol(uint16_t* cdf, int symbol_count);
  // ReadSymbol() calls for which the |symbol_count| is equal to 2 (boolean
  // symbols) will use this variant.
  bool ReadSymbol(uint16_t* cdf);
  bool ReadSymbolWithoutCdfUpdate(uint16_t* cdf);
  // Use either linear search or binary search for decoding the symbol depending
  // on |symbol_count|. ReadSymbol calls for which the |symbol_count| is known
  // at compile time will use this variant.
  template <int symbol_count>
  int ReadSymbol(uint16_t* cdf);

 private:
  // WindowSize must be an unsigned integer type with at least 32 bits. Use the
  // largest type with fast arithmetic. size_t should meet these requirements.
  static_assert(sizeof(size_t) == sizeof(void*), "");
  using WindowSize = size_t;
  static constexpr uint32_t kWindowSize =
      static_cast<uint32_t>(sizeof(WindowSize)) * 8;
  static_assert(kWindowSize >= 32, "");

  // Reads a symbol using the |cdf| table which contains the probabilities of
  // each symbol. On a high level, this function does the following:
  //   1) Scale the |cdf| values.
  //   2) Find the index in the |cdf| array where the scaled CDF value crosses
  //   the modified |window_diff_| threshold.
  //   3) That index is the symbol that has been decoded.
  //   4) Update |window_diff_| and |values_in_range_| based on the symbol that
  //   has been decoded.
  inline int ReadSymbolImpl(const uint16_t* cdf, int symbol_count);
  // Similar to ReadSymbolImpl but it uses binary search to perform step 2 in
  // the comment above. As of now, this function is called when |symbol_count|
  // is greater than or equal to 8.
  inline int ReadSymbolImplBinarySearch(const uint16_t* cdf, int symbol_count);
  // Specialized implementation of ReadSymbolImpl based on the fact that
  // symbol_count == 2.
  inline int ReadSymbolImpl(const uint16_t* cdf);
  // ReadSymbolN is a specialization of ReadSymbol for symbol_count == N.
  int ReadSymbol4(uint16_t* cdf);
  // ReadSymbolImplN is a specialization of ReadSymbolImpl for
  // symbol_count == N.
  LIBGAV1_ALWAYS_INLINE int ReadSymbolImpl8(const uint16_t* cdf);
  inline void PopulateBits();
  // Normalizes the range so that 32768 <= |values_in_range_| < 65536. Also
  // calls PopulateBits() if necessary.
  inline void NormalizeRange();

  const uint8_t* const data_;
  const size_t size_;
  size_t data_index_;
  const bool allow_update_cdf_;
  // Number of bits of data in the current value.
  int bits_;
  // Number of values in the current range. Declared as uint32_t for better
  // performance but only the lower 16 bits are used.
  uint32_t values_in_range_;
  // The difference between the high end of the current range and the coded
  // value minus 1. The 16 most significant bits of this variable is used to
  // decode the next symbol. It is filled in whenever |bits_| is less than 0.
  WindowSize window_diff_;
};

}  // namespace libgav1

#endif  // LIBGAV1_SRC_UTILS_ENTROPY_DECODER_H_
