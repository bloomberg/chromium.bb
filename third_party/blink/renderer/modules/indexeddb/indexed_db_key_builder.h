// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_KEY_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_KEY_BUILDER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class IDBKey;
class WebIDBKeyRange;

class MODULES_EXPORT WebIDBKeyRangeBuilder {
 public:
  // Builds a point range (containing a single key).
  static WebIDBKeyRange Build(const IDBKey* key);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIDBKeyRangeBuilder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_KEY_BUILDER_H_
