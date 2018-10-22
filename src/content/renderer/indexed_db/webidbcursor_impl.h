// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INDEXED_DB_WEBIDBCURSOR_IMPL_H_
#define CONTENT_RENDERER_INDEXED_DB_WEBIDBCURSOR_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_value.h"

namespace content {

class IndexedDBCallbacksImpl;

class CONTENT_EXPORT WebIDBCursorImpl : public blink::WebIDBCursor {
 public:
  WebIDBCursorImpl(blink::mojom::IDBCursorAssociatedPtrInfo cursor,
                   int64_t transaction_id);
  ~WebIDBCursorImpl() override;

  void Advance(unsigned long count, blink::WebIDBCallbacks* callback) override;
  void CursorContinue(blink::WebIDBKeyView key,
                      blink::WebIDBKeyView primary_key,
                      blink::WebIDBCallbacks* callback) override;
  void PostSuccessHandlerCallback() override;

  void SetPrefetchData(const std::vector<blink::IndexedDBKey>& keys,
                       const std::vector<blink::IndexedDBKey>& primary_keys,
                       std::vector<blink::WebIDBValue> values);

  void CachedAdvance(unsigned long count, blink::WebIDBCallbacks* callbacks);
  void CachedContinue(blink::WebIDBCallbacks* callbacks);

  // This method is virtual so it can be overridden in unit tests.
  virtual void ResetPrefetchCache();

  int64_t transaction_id() const { return transaction_id_; }

 private:
  blink::mojom::IDBCallbacksAssociatedPtrInfo GetCallbacksProxy(
      std::unique_ptr<IndexedDBCallbacksImpl> callbacks);

  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorReset);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorTransactionId);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, AdvancePrefetchTest);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, PrefetchReset);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, PrefetchTest);

  enum { kInvalidCursorId = -1 };
  enum { kPrefetchContinueThreshold = 2 };
  enum { kMinPrefetchAmount = 5 };
  enum { kMaxPrefetchAmount = 100 };

  int64_t transaction_id_;

  blink::mojom::IDBCursorAssociatedPtr cursor_;

  // Prefetch cache.
  base::circular_deque<blink::IndexedDBKey> prefetch_keys_;
  base::circular_deque<blink::IndexedDBKey> prefetch_primary_keys_;
  base::circular_deque<blink::WebIDBValue> prefetch_values_;

  // Number of continue calls that would qualify for a pre-fetch.
  int continue_count_;

  // Number of items used from the last prefetch.
  int used_prefetches_;

  // Number of onsuccess handlers we are waiting for.
  int pending_onsuccess_callbacks_;

  // Number of items to request in next prefetch.
  int prefetch_amount_;

  base::WeakPtrFactory<WebIDBCursorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebIDBCursorImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INDEXED_DB_WEBIDBCURSOR_IMPL_H_
