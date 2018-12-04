// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_BLINK_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_BLINK_MOJOM_TRAITS_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/map_traits_wtf_hash_map.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/modules/indexeddb/indexed_db_key_builder.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key_range.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

template <>
struct MODULES_EXPORT StructTraits<blink::mojom::IDBDatabaseMetadataDataView,
                                   blink::IDBDatabaseMetadata> {
  static int64_t id(const blink::IDBDatabaseMetadata& metadata) {
    return metadata.id;
  }
  static WTF::String name(const blink::IDBDatabaseMetadata& metadata) {
    if (metadata.name.IsNull())
      return g_empty_string;
    return metadata.name;
  }
  static int64_t version(const blink::IDBDatabaseMetadata& metadata) {
    return metadata.version;
  }
  static int64_t max_object_store_id(
      const blink::IDBDatabaseMetadata& metadata) {
    return metadata.max_object_store_id;
  }
  static const HashMap<int64_t, scoped_refptr<blink::IDBObjectStoreMetadata>>&
  object_stores(const blink::IDBDatabaseMetadata& metadata) {
    return metadata.object_stores;
  }
  static bool Read(blink::mojom::IDBDatabaseMetadataDataView data,
                   blink::IDBDatabaseMetadata* out);
};

template <>
struct MODULES_EXPORT
    StructTraits<blink::mojom::IDBIndexKeysDataView, blink::WebIDBIndexKeys> {
  static int64_t index_id(const blink::WebIDBIndexKeys& index_keys) {
    return index_keys.first;
  }
  static const blink::WebVector<blink::WebIDBKey>& index_keys(
      const blink::WebIDBIndexKeys& index_keys) {
    return index_keys.second;
  }
  static bool Read(blink::mojom::IDBIndexKeysDataView data,
                   blink::WebIDBIndexKeys* out);
};

template <>
struct MODULES_EXPORT StructTraits<blink::mojom::IDBIndexMetadataDataView,
                                   scoped_refptr<blink::IDBIndexMetadata>> {
  static int64_t id(const scoped_refptr<blink::IDBIndexMetadata>& metadata) {
    return metadata->id;
  }
  static WTF::String name(
      const scoped_refptr<blink::IDBIndexMetadata>& metadata) {
    if (metadata->name.IsNull())
      return g_empty_string;
    return metadata->name;
  }
  static const blink::IDBKeyPath& key_path(
      const scoped_refptr<blink::IDBIndexMetadata>& metadata) {
    return metadata->key_path;
  }
  static bool unique(const scoped_refptr<blink::IDBIndexMetadata>& metadata) {
    return metadata->unique;
  }
  static bool multi_entry(
      const scoped_refptr<blink::IDBIndexMetadata>& metadata) {
    return metadata->multi_entry;
  }
  static bool Read(blink::mojom::IDBIndexMetadataDataView data,
                   scoped_refptr<blink::IDBIndexMetadata>* out);
};

template <>
struct MODULES_EXPORT
    UnionTraits<blink::mojom::IDBKeyDataDataView, blink::WebIDBKey> {
  static blink::mojom::IDBKeyDataDataView::Tag GetTag(
      const blink::WebIDBKey& key);
  static bool Read(blink::mojom::IDBKeyDataDataView data,
                   blink::WebIDBKey* out);
  static const blink::WebVector<blink::WebIDBKey> key_array(
      const blink::WebIDBKey& key);
  static const Vector<uint8_t> binary(const blink::WebIDBKey& key);
  static const WTF::String string(const blink::WebIDBKey& key) {
    String key_string = key.View().String();
    if (key_string.IsNull())
      key_string = g_empty_string;
    return key_string;
  }
  static double date(const blink::WebIDBKey& key) { return key.View().Date(); }
  static double number(const blink::WebIDBKey& key) {
    return key.View().Number();
  }
  static bool other_invalid(const blink::WebIDBKey& key) {
    return key.View().KeyType() == blink::kWebIDBKeyTypeInvalid;
  }
  static bool other_null(const blink::WebIDBKey& key) {
    return key.View().KeyType() == blink::kWebIDBKeyTypeNull;
  }
};

template <>
struct MODULES_EXPORT
    StructTraits<blink::mojom::IDBKeyDataView, blink::WebIDBKey> {
  static const blink::WebIDBKey& data(const blink::WebIDBKey& key);
  static bool Read(blink::mojom::IDBKeyDataView data, blink::WebIDBKey* out);
};

template <>
struct MODULES_EXPORT
    StructTraits<blink::mojom::IDBKeyPathDataView, blink::IDBKeyPath> {
  static blink::mojom::blink::IDBKeyPathDataPtr data(
      const blink::IDBKeyPath& key_path);
  static bool Read(blink::mojom::IDBKeyPathDataView data,
                   blink::IDBKeyPath* out);
};

template <>
struct MODULES_EXPORT
    StructTraits<blink::mojom::IDBKeyRangeDataView, blink::WebIDBKeyRange> {
  static blink::WebIDBKey lower(const blink::WebIDBKeyRange& key_range) {
    return blink::WebIDBKeyBuilder::Build(key_range.Lower());
  }
  static blink::WebIDBKey upper(const blink::WebIDBKeyRange& key_range) {
    return blink::WebIDBKeyBuilder::Build(key_range.Upper());
  }
  static bool lower_open(const blink::WebIDBKeyRange& key_range) {
    return key_range.LowerOpen();
  }
  static bool upper_open(const blink::WebIDBKeyRange& key_range) {
    return key_range.UpperOpen();
  }
  static bool Read(blink::mojom::IDBKeyRangeDataView data,
                   blink::WebIDBKeyRange* out);
};

template <>
struct MODULES_EXPORT
    StructTraits<blink::mojom::IDBObjectStoreMetadataDataView,
                 scoped_refptr<blink::IDBObjectStoreMetadata>> {
  static int64_t id(
      const scoped_refptr<blink::IDBObjectStoreMetadata>& metadata) {
    return metadata->id;
  }
  static WTF::String name(
      const scoped_refptr<blink::IDBObjectStoreMetadata>& metadata) {
    if (metadata->name.IsNull())
      return g_empty_string;
    return metadata->name;
  }
  static const blink::IDBKeyPath& key_path(
      const scoped_refptr<blink::IDBObjectStoreMetadata>& metadata) {
    return metadata->key_path;
  }
  static bool auto_increment(
      const scoped_refptr<blink::IDBObjectStoreMetadata>& metadata) {
    return metadata->auto_increment;
  }
  static int64_t max_index_id(
      const scoped_refptr<blink::IDBObjectStoreMetadata>& metadata) {
    return metadata->max_index_id;
  }
  static const HashMap<int64_t, scoped_refptr<blink::IDBIndexMetadata>>&
  indexes(const scoped_refptr<blink::IDBObjectStoreMetadata>& metadata) {
    return metadata->indexes;
  }
  static bool Read(blink::mojom::IDBObjectStoreMetadataDataView data,
                   scoped_refptr<blink::IDBObjectStoreMetadata>* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_BLINK_MOJOM_TRAITS_H_
