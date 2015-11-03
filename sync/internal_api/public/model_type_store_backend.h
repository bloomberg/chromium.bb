// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_STORE_BACKEND_H_
#define SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_STORE_BACKEND_H_

#include "base/threading/non_thread_safe.h"
#include "sync/api/model_type_store.h"

namespace syncer_v2 {

// ModelTypeStoreBackend handles operations with leveldb. It is oblivious of the
// fact that it is called from separate thread (with the exception of ctor),
// meaning it shouldn't deal with callbacks and task_runners.
class ModelTypeStoreBackend : public base::NonThreadSafe {
 public:
  ModelTypeStoreBackend();
  ~ModelTypeStoreBackend();

  ModelTypeStore::Result Init();
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_STORE_BACKEND_H_
