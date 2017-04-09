// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebIDBObservation_h
#define WebIDBObservation_h

#include "public/platform/modules/indexeddb/WebIDBKeyRange.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "public/platform/modules/indexeddb/WebIDBValue.h"

namespace blink {

struct WebIDBObservation {
  int64_t object_store_id;
  WebIDBOperationType type;
  WebIDBKeyRange key_range;
  WebIDBValue value;
};

}  // namespace blink

#endif  // WebIDBObservation_h
