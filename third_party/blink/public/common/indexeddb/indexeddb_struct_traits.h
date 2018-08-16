// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_INDEXEDDB_INDEXEDDB_STRUCT_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_INDEXEDDB_INDEXEDDB_STRUCT_TRAITS_H_

#include "third_party/blink/common/common_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::IDBCursorDirection, blink::WebIDBCursorDirection> {
  static blink::mojom::IDBCursorDirection ToMojom(
      blink::WebIDBCursorDirection input);
  static bool FromMojom(blink::mojom::IDBCursorDirection input,
                        blink::WebIDBCursorDirection* output);
};

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::IDBDataLoss, blink::WebIDBDataLoss> {
  static blink::mojom::IDBDataLoss ToMojom(blink::WebIDBDataLoss input);
  static bool FromMojom(blink::mojom::IDBDataLoss input,
                        blink::WebIDBDataLoss* output);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::IDBKeyDataView, blink::IndexedDBKey> {
  static blink::mojom::IDBKeyDataPtr data(const blink::IndexedDBKey& key);
  static bool Read(blink::mojom::IDBKeyDataView data, blink::IndexedDBKey* out);
};

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::IDBOperationType, blink::WebIDBOperationType> {
  static blink::mojom::IDBOperationType ToMojom(
      blink::WebIDBOperationType input);
  static bool FromMojom(blink::mojom::IDBOperationType input,
                        blink::WebIDBOperationType* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_INDEXEDDB_INDEXEDDB_STRUCT_TRAITS_H_
