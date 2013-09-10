/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef WTF_QuantizedAllocation_h
#define WTF_QuantizedAllocation_h

// DESCRIPTION
// QuantizedAllocation::quantizedSize() rounds up the specific size such that:
// 1) Granularity of allocation sizes is >1, and grows as a fraction of the
// allocation size. This property can be used to limit bucket fragmentation
// when sitting on top of a high-granularity allocator such as PartitionAlloc.
// 2) The sizes are likely (but not guaranteed) to correspond to exact "bucket
// sizes" of any underlying allocator that implements standard bucketing.
// This currently includes tcmalloc, Windows LFH and PartitionAlloc.

#include <limits.h>

namespace WTF {

class QuantizedAllocation {
public:
    // Allocations above 32k are rounded to 4k (which is typically the system
    // page size).
    static const size_t kMaxAllocation = 32768;
    static const size_t kMaxRounding = 4096;

    // This is the minimum granularity of allocation, currently 16 bytes.
    static const size_t kMinRounding = 16;

    // After 256 bytes, the granularity doubles, and proceeds to double again
    // as the allocation size further doubles.
    static const size_t kMinRoundingLimit = 256;
    static const size_t kTableSize = kMaxAllocation / kMinRoundingLimit;

    // Using "unsigned" is not a limitation because Chromium's max malloc() is
    // 2GB even on 64-bit.
    static const size_t kMaxUnquantizedAllocation = UINT_MAX - kMaxRounding;

    static void init();

    static size_t quantizedSize(size_t size)
    {
        size_t roundToLessOne;
        if (UNLIKELY(size >= kMaxAllocation))
            roundToLessOne = kMaxRounding - 1;
        else
            roundToLessOne = table[size / kMinRoundingLimit];
        return (size + roundToLessOne) & ~roundToLessOne;
    }

private:
    static unsigned short table[kTableSize];
};

} // namespace WTF

#endif // WTF_QuantizedAllocation_h
