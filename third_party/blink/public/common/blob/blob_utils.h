
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_BLOB_BLOB_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_BLOB_BLOB_UTILS_H_

#include <stdint.h>

#include "third_party/blink/common/common_export.h"

namespace blink {

class BlobUtils {
 public:
  // Whether the new Blob URL glue for NetworkService is enabled (i.e.,
  // the NetworkService or MojoBlobURLs feature is enabled).
  static bool BLINK_COMMON_EXPORT MojoBlobURLsEnabled();

  // Get the preferred capacity a mojo::DataPipe being used to read a blob.
  static uint32_t BLINK_COMMON_EXPORT GetDataPipeCapacity();

  // Get the preferred chunk size to use when reading a blob to copy
  // into a mojo::DataPipe.
  static uint32_t BLINK_COMMON_EXPORT GetDataPipeChunkSize();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_BLOB_BLOB_UTILS_H_
