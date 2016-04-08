// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IndexedDBClient.h"
#include "wtf/Atomics.h"

namespace blink {

static void* idbClientCreateFunction = nullptr;

void setIndexedDBClientCreateFunction(CreateIndexedDBClient createFunction)
{
    // See web/IndexedDBClientImpl.h comment for some context. As the initialization
    // of this IndexedDB client constructor now happens as context is set up for
    // threads, it is possible that setIndexedDBClientCreateFunction() will be
    // called more than once. Hence update atomicity is needed.
#if ENABLE(ASSERT)
    CreateIndexedDBClient* currentFunction = reinterpret_cast<CreateIndexedDBClient*>(acquireLoad(&idbClientCreateFunction));
    ASSERT(!currentFunction || currentFunction == createFunction);
#endif
    releaseStore(&idbClientCreateFunction, reinterpret_cast<void*>(createFunction));
}

IndexedDBClient* IndexedDBClient::create()
{
    CreateIndexedDBClient* createFunction = reinterpret_cast<CreateIndexedDBClient*>(acquireLoad(&idbClientCreateFunction));
    ASSERT(createFunction);
    // There's no reason why we need to allocate a new proxy each time, but
    // there's also no strong reason not to.
    return createFunction();
}

} // namespace blink
