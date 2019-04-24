// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"

namespace blink {
namespace cache_storage {

// TODO(crbug.com/877737): Remove this once the cache.addAll() duplicate
// rejection finally ships.
// static
const char kDuplicateOperationBaseMessage[] = "duplicate requests";

}  // namespace cache_storage
}  // namespace blink
