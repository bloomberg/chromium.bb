// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_MODEL_TYPE_STORE_TEST_UTIL_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_MODEL_TYPE_STORE_TEST_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "sync/api/model_type_store.h"

namespace syncer_v2 {

// Util class with several static methods to facilitate writing unit tests for
// classes that use ModelTypeStore objects.
class ModelTypeStoreTestUtil {
 public:
  // Creates an in memory store syncronously. Be aware that to do this all
  // outstanding tasks will be run as the current message loop is pumped.
  static scoped_ptr<ModelTypeStore> CreateInMemoryStoreForTest();

  // Can be curried with an owned store object to allow passing an already
  // created store to a service constructor in a unit test.
  static void MoveStoreToCallback(scoped_ptr<ModelTypeStore> store,
                                  ModelTypeStore::InitCallback callback);
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_MODEL_TYPE_STORE_TEST_UTIL_H_
