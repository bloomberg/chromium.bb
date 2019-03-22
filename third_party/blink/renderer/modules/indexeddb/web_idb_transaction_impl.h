// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_TRANSACTION_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_TRANSACTION_IMPL_H_

#include <stdint.h>
#include <set>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_transaction.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT WebIDBTransactionImpl : public WebIDBTransaction {
 public:
  WebIDBTransactionImpl(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~WebIDBTransactionImpl() override;

  // WebIDBTransaction
  void CreateObjectStore(int64_t objectstore_id,
                         const String& name,
                         const IDBKeyPath&,
                         bool auto_increment) override;

  mojom::blink::IDBTransactionAssociatedRequest CreateRequest() override;

 private:
  mojom::blink::IDBTransactionAssociatedPtr transaction_;
  mojom::blink::IDBTransactionAssociatedRequest transaction_request_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_TRANSACTION_IMPL_H_
