// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
#define CONTENT_RENDERER_INDEXED_DB_WEBIDBFACTORY_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/renderer/indexed_db/indexed_db_callbacks_impl.h"
#include "content/renderer/indexed_db/indexed_db_database_callbacks_impl.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_factory.h"

namespace blink {
class WebSecurityOrigin;
class WebString;
}

namespace content {

class WebIDBFactoryImpl : public blink::WebIDBFactory {
 public:
  explicit WebIDBFactoryImpl(blink::mojom::IDBFactoryPtrInfo factory_info);
  ~WebIDBFactoryImpl() override;

  // See WebIDBFactory.h for documentation on these functions.
  void GetDatabaseNames(
      blink::WebIDBCallbacks* callbacks,
      const blink::WebSecurityOrigin& origin,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void Open(const blink::WebString& name,
            long long version,
            long long transaction_id,
            blink::WebIDBCallbacks* callbacks,
            blink::WebIDBDatabaseCallbacks* databaseCallbacks,
            const blink::WebSecurityOrigin& origin,
            scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void DeleteDatabase(
      const blink::WebString& name,
      blink::WebIDBCallbacks* callbacks,
      const blink::WebSecurityOrigin& origin,
      bool force_close,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;

 private:
  blink::mojom::IDBCallbacksAssociatedPtrInfo GetCallbacksProxy(
      std::unique_ptr<IndexedDBCallbacksImpl> callbacks);
  blink::mojom::IDBDatabaseCallbacksAssociatedPtrInfo GetDatabaseCallbacksProxy(
      std::unique_ptr<IndexedDBDatabaseCallbacksImpl> callbacks);

  blink::mojom::IDBFactoryPtr factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
