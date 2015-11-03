// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/model_type_store.h"

#include "sync/internal_api/public/model_type_store_impl.h"

namespace syncer_v2 {

// static
void ModelTypeStore::CreateInMemoryStoreForTest(const InitCallback& callback) {
  ModelTypeStoreImpl::CreateInMemoryStoreForTest(callback);
}

ModelTypeStore::~ModelTypeStore() {}

ModelTypeStore::WriteBatch::WriteBatch() {}

ModelTypeStore::WriteBatch::~WriteBatch() {}

}  // namespace syncer_v2
