// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment_store.h"

#include "sync/internal_api/attachments/attachment_store_test_template.h"

namespace syncer {

class InMemoryAttachmentStoreFactory {
 public:
  InMemoryAttachmentStoreFactory() {}
  ~InMemoryAttachmentStoreFactory() {}

  scoped_ptr<AttachmentStore> CreateAttachmentStore() {
    return AttachmentStore::CreateInMemoryStore();
  }
};

INSTANTIATE_TYPED_TEST_CASE_P(InMemory,
                              AttachmentStoreTest,
                              InMemoryAttachmentStoreFactory);

}  // namespace syncer
