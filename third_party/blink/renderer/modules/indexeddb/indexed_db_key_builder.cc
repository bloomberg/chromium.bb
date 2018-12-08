// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_key_builder.h"

#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"

namespace blink {

// static
IDBKeyRange* IDBKeyRangeBuilder::Build(const IDBKey* key) {
  std::unique_ptr<IDBKey> lower_key = IDBKey::Clone(key);
  std::unique_ptr<IDBKey> upper_key = IDBKey::Clone(key);
  return IDBKeyRange::Create(std::move(lower_key), std::move(upper_key),
                             IDBKeyRange::kLowerBoundClosed,
                             IDBKeyRange::kUpperBoundClosed);
}

}  // namespace blink
