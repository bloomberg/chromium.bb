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

#ifndef ArrayBufferBuilder_h
#define ArrayBufferBuilder_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"
#include "platform/wtf/typed_arrays/ArrayBuffer.h"

namespace WTF {

// A utility class to build an ArrayBuffer instance. Validity must be checked
// by isValid() before using an instance.
class WTF_EXPORT ArrayBufferBuilder final {
  // Disallow copying since it's expensive and we don't want code to do it by
  // accident.
  WTF_MAKE_NONCOPYABLE(ArrayBufferBuilder);
  USING_FAST_MALLOC(ArrayBufferBuilder);

 public:
  // Creates an ArrayBufferBuilder using the default capacity.
  ArrayBufferBuilder();

  ArrayBufferBuilder(unsigned capacity)
      : bytes_used_(0), variable_capacity_(true) {
    buffer_ = ArrayBuffer::Create(capacity, 1);
  }

  bool IsValid() const { return buffer_.Get(); }

  // Appending empty data is not allowed.
  unsigned Append(const char* data, unsigned length);

  // Returns the accumulated data as an ArrayBuffer instance. If needed,
  // creates a new ArrayBuffer instance and copies contents from the internal
  // buffer to it. Otherwise, returns a RefPtr pointing to the internal
  // buffer.
  RefPtr<ArrayBuffer> ToArrayBuffer();

  // Converts the accumulated data into a String using the default encoding.
  String ToString();

  // Number of bytes currently accumulated.
  unsigned ByteLength() const { return bytes_used_; }

  // Number of bytes allocated.
  unsigned Capacity() const { return buffer_->ByteLength(); }

  void ShrinkToFit();

  const void* Data() const { return buffer_->Data(); }

  // If set to false, the capacity won't be expanded and when appended data
  // overflows, the overflowed part will be dropped.
  void SetVariableCapacity(bool value) { variable_capacity_ = value; }

 private:
  // Expands the size of m_buffer to size + m_bytesUsed bytes. Returns true
  // iff successful. If reallocation is needed, copies only data in
  // [0, m_bytesUsed) range.
  bool ExpandCapacity(unsigned size);

  unsigned bytes_used_;
  bool variable_capacity_;
  RefPtr<ArrayBuffer> buffer_;
};

}  // namespace WTF

using WTF::ArrayBufferBuilder;

#endif  // ArrayBufferBuilder_h
