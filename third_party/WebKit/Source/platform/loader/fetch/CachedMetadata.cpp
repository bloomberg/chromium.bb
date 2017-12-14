// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/CachedMetadata.h"

namespace blink {

CachedMetadata::CachedMetadata(const char* data, size_t size) {
  // Serialized metadata should have non-empty data.
  DCHECK_GT(size, kCachedMetaDataStart);
  DCHECK(data);

  serialized_data_.ReserveInitialCapacity(size);
  serialized_data_.Append(data, size);
}

CachedMetadata::CachedMetadata(uint32_t data_type_id,
                               const char* data,
                               size_t size) {
  // Don't allow an ID of 0, it is used internally to indicate errors.
  DCHECK(data_type_id);
  DCHECK(data);

  serialized_data_.ReserveInitialCapacity(sizeof(uint32_t) + size);
  serialized_data_.Append(reinterpret_cast<const char*>(&data_type_id),
                          sizeof(uint32_t));
  serialized_data_.Append(data, size);
}

}  // namespace blink
