// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FuzzedDataProvider_h
#define FuzzedDataProvider_h

#include "base/test/fuzzed_data_provider.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/CString.h"

namespace blink {

// This class simply wraps //base/test/fuzzed_data_provider and vends Blink
// friendly types.
class FuzzedDataProvider {
  WTF_MAKE_NONCOPYABLE(FuzzedDataProvider);

 public:
  FuzzedDataProvider(const uint8_t* bytes, size_t num_bytes);

  // Returns a string with length between minBytes and maxBytes. If the
  // length is greater than the length of the remaining data this is
  // equivalent to ConsumeRemainingBytes().
  CString ConsumeBytesInRange(uint32_t min_bytes, uint32_t max_bytes);

  // Returns a String containing all remaining bytes of the input data.
  CString ConsumeRemainingBytes();

  // Returns a bool, or false when no data remains.
  bool ConsumeBool();

  // Returns a number in the range [min, max] by consuming bytes from the input
  // data. The value might not be uniformly distributed in the given range. If
  // there's no input data left, always returns |min|. |min| must be less than
  // or equal to |max|.
  int ConsumeInt32InRange(int min, int max);

  // Returns a value from |array|, consuming as many bytes as needed to do so.
  // |array| must be a fixed-size array.
  template <typename Type, size_t size>
  Type PickValueInArray(Type (&array)[size]) {
    return array[provider_.ConsumeUint32InRange(0, size - 1)];
  }

 private:
  base::FuzzedDataProvider provider_;
};

}  // namespace blink

#endif  // FuzzedDataProvider_h
