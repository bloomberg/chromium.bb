/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef CachedMetadata_h
#define CachedMetadata_h

#include <stdint.h>
#include "platform/PlatformExport.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

// Metadata retrieved from the embedding application's cache.
//
// Serialized data is NOT portable across architectures. However, reading the
// data type ID will reject data generated with a different byte-order.
class PLATFORM_EXPORT CachedMetadata : public RefCounted<CachedMetadata> {
 public:
  static RefPtr<CachedMetadata> Create(uint32_t data_type_id,
                                       const char* data,
                                       size_t size) {
    return AdoptRef(new CachedMetadata(data_type_id, data, size));
  }

  static RefPtr<CachedMetadata> CreateFromSerializedData(const char* data,
                                                         size_t size) {
    return AdoptRef(new CachedMetadata(data, size));
  }

  ~CachedMetadata() {}

  const Vector<char>& SerializedData() const { return serialized_data_; }

  uint32_t DataTypeID() const {
    // We need to define a local variable to use the constant in DCHECK.
    constexpr auto kDataStart = CachedMetadata::kDataStart;
    DCHECK_GE(serialized_data_.size(), kDataStart);
    return *reinterpret_cast_ptr<uint32_t*>(
        const_cast<char*>(serialized_data_.data()));
  }

  const char* Data() const {
    constexpr auto kDataStart = CachedMetadata::kDataStart;
    DCHECK_GE(serialized_data_.size(), kDataStart);
    return serialized_data_.data() + kDataStart;
  }

  size_t size() const {
    constexpr auto kDataStart = CachedMetadata::kDataStart;
    DCHECK_GE(serialized_data_.size(), kDataStart);
    return serialized_data_.size() - kDataStart;
  }

 private:
  CachedMetadata(const char* data, size_t);
  CachedMetadata(uint32_t data_type_id, const char* data, size_t);

  // Since the serialization format supports random access, storing it in
  // serialized form avoids need for a copy during serialization.
  Vector<char> serialized_data_;

  // |m_serializedData| consists of 32 bits type ID and and actual data.
  static constexpr size_t kDataStart = sizeof(uint32_t);
};

}  // namespace blink

#endif
