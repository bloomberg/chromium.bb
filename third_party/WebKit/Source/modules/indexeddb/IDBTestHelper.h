// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBTestHelper_h
#define IDBTestHelper_h

#include "modules/indexeddb/IDBValue.h"
#include "platform/wtf/RefPtr.h"
#include "v8/include/v8.h"

namespace blink {

// The created value is an array of true. If create_wrapped_value is true, the
// IDBValue's byte array will be wrapped in a Blob, otherwise it will not be.
scoped_refptr<IDBValue> CreateIDBValueForTesting(v8::Isolate*,
                                                 bool create_wrapped_value);

}  // namespace blink

#endif  // IDBTestHelper_h
