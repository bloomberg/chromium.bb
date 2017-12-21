// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/MockWebIDBDatabase.h"

#include <memory>

#include "base/memory/ptr_util.h"

namespace blink {

MockWebIDBDatabase::MockWebIDBDatabase() {}

MockWebIDBDatabase::~MockWebIDBDatabase() {}

std::unique_ptr<MockWebIDBDatabase> MockWebIDBDatabase::Create() {
  return base::WrapUnique(new MockWebIDBDatabase());
}

}  // namespace blink
