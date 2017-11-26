/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
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

#ifndef FastSharedBufferReader_h
#define FastSharedBufferReader_h

#include "base/memory/scoped_refptr.h"
#include "platform/PlatformExport.h"
#include "platform/image-decoders/SegmentReader.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

// This class is used by image decoders to avoid memory consolidation and
// therefore minimizes the cost of memory copying when the decoders
// repeatedly read from a buffer that is continually growing due to network
// traffic.
class PLATFORM_EXPORT FastSharedBufferReader final {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(FastSharedBufferReader);

 public:
  FastSharedBufferReader(scoped_refptr<SegmentReader> data);

  void SetData(scoped_refptr<SegmentReader>);

  // Returns a consecutive buffer that carries the data starting
  // at |data_position| with |length| bytes.
  // This method returns a pointer to a memory segment stored in
  // |data_| if such a consecutive buffer can be found.
  // Otherwise copies into |buffer| and returns it.
  // Caller must ensure there are enough bytes in |data_| and |buffer|.
  const char* GetConsecutiveData(size_t data_position,
                                 size_t length,
                                 char* buffer) const;

  // Wraps SegmentReader::GetSomeData().
  size_t GetSomeData(const char*& some_data, size_t data_position) const;

  // Returns a byte at |data_position|.
  // Caller must ensure there are enough bytes in |data_|.
  inline char GetOneByte(size_t data_position) const {
    return *GetConsecutiveData(data_position, 1, nullptr);
  }

  size_t size() const { return data_->size(); }

  // This class caches the last access for faster subsequent reads. This
  // method clears that cache in case the SegmentReader has been modified
  // (e.g. with MergeSegmentsIntoBuffer on a wrapped SharedBuffer).
  void ClearCache();

 private:
  void GetSomeDataInternal(size_t data_position) const;

  scoped_refptr<SegmentReader> data_;

  // Caches the last segment of |data_| accessed, since subsequent reads are
  // likely to re-access it.
  mutable const char* segment_;
  mutable size_t segment_length_;

  // Data position in |data_| pointed to by |segment_|.
  mutable size_t data_position_;
};

}  // namespace blink

#endif
