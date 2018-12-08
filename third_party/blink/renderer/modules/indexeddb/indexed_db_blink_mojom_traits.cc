// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/array_traits_web_vector.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "third_party/blink/public/common/indexeddb/indexed_db_default_mojom_traits.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/platform/mojo/string16_mojom_traits.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using blink::mojom::IDBCursorDirection;
using blink::mojom::IDBDataLoss;
using blink::mojom::IDBOperationType;

namespace mojo {

// static
bool StructTraits<blink::mojom::IDBDatabaseMetadataDataView,
                  blink::IDBDatabaseMetadata>::
    Read(blink::mojom::IDBDatabaseMetadataDataView data,
         blink::IDBDatabaseMetadata* out) {
  out->id = data.id();
  String name;
  if (!data.ReadName(&name))
    return false;
  out->name = name;
  out->version = data.version();
  out->max_object_store_id = data.max_object_store_id();
  MapDataView<int64_t, blink::mojom::IDBObjectStoreMetadataDataView>
      object_stores;
  data.GetObjectStoresDataView(&object_stores);
  out->object_stores.ReserveCapacityForSize(
      SafeCast<wtf_size_t>(object_stores.size()));
  for (size_t i = 0; i < object_stores.size(); ++i) {
    const int64_t key = object_stores.keys()[i];
    scoped_refptr<blink::IDBObjectStoreMetadata> object_store;
    if (!object_stores.values().Read(i, &object_store)) {
      return false;
    }
    DCHECK(!out->object_stores.Contains(key));
    out->object_stores.insert(key, object_store);
  }
  return true;
}

// static
bool StructTraits<blink::mojom::IDBIndexKeysDataView, blink::IDBIndexKeys>::
    Read(blink::mojom::IDBIndexKeysDataView data, blink::IDBIndexKeys* out) {
  out->first = data.index_id();
  if (!data.ReadIndexKeys(&out->second))
    return false;
  return true;
}

// static
bool StructTraits<blink::mojom::IDBIndexMetadataDataView,
                  scoped_refptr<blink::IDBIndexMetadata>>::
    Read(blink::mojom::IDBIndexMetadataDataView data,
         scoped_refptr<blink::IDBIndexMetadata>* out) {
  scoped_refptr<blink::IDBIndexMetadata> value =
      blink::IDBIndexMetadata::Create();
  value->id = data.id();
  String name;
  if (!data.ReadName(&name))
    return false;
  value->name = name;
  if (!data.ReadKeyPath(&value->key_path))
    return false;
  value->unique = data.unique();
  value->multi_entry = data.multi_entry();
  *out = std::move(value);
  return true;
}

// static
blink::mojom::IDBKeyDataDataView::Tag
UnionTraits<blink::mojom::IDBKeyDataDataView, std::unique_ptr<blink::IDBKey>>::
    GetTag(const std::unique_ptr<blink::IDBKey>& key) {
  DCHECK(key.get());
  switch (key->GetType()) {
    case blink::mojom::IDBKeyType::Invalid:
      return blink::mojom::IDBKeyDataDataView::Tag::OTHER_INVALID;
    case blink::mojom::IDBKeyType::Array:
      return blink::mojom::IDBKeyDataDataView::Tag::KEY_ARRAY;
    case blink::mojom::IDBKeyType::Binary:
      return blink::mojom::IDBKeyDataDataView::Tag::BINARY;
    case blink::mojom::IDBKeyType::String:
      return blink::mojom::IDBKeyDataDataView::Tag::STRING;
    case blink::mojom::IDBKeyType::Date:
      return blink::mojom::IDBKeyDataDataView::Tag::DATE;
    case blink::mojom::IDBKeyType::Number:
      return blink::mojom::IDBKeyDataDataView::Tag::NUMBER;
    case blink::mojom::IDBKeyType::Null:
      return blink::mojom::IDBKeyDataDataView::Tag::OTHER_NULL;

    // Not used, fall through to NOTREACHED.
    case blink::mojom::IDBKeyType::Min:;
  }
  NOTREACHED();
  return blink::mojom::IDBKeyDataDataView::Tag::OTHER_INVALID;
}

// static
bool UnionTraits<
    blink::mojom::IDBKeyDataDataView,
    std::unique_ptr<blink::IDBKey>>::Read(blink::mojom::IDBKeyDataDataView data,
                                          std::unique_ptr<blink::IDBKey>* out) {
  switch (data.tag()) {
    case blink::mojom::IDBKeyDataDataView::Tag::KEY_ARRAY: {
      Vector<std::unique_ptr<blink::IDBKey>> array;
      if (!data.ReadKeyArray(&array))
        return false;
      *out = blink::IDBKey::CreateArray(std::move(array));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::BINARY: {
      ArrayDataView<uint8_t> bytes;
      data.GetBinaryDataView(&bytes);
      *out = blink::IDBKey::CreateBinary(blink::WebData(
          reinterpret_cast<const char*>(bytes.data()), bytes.size()));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::STRING: {
      String string;
      if (!data.ReadString(&string))
        return false;
      *out = blink::IDBKey::CreateString(blink::WebString(string));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::DATE:
      *out = blink::IDBKey::CreateDate(data.date());
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::NUMBER:
      *out = blink::IDBKey::CreateNumber(data.number());
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::OTHER_INVALID:
      *out = blink::IDBKey::CreateInvalid();
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::OTHER_NULL:
      *out = blink::IDBKey::CreateNull();
      return true;
  }

  return false;
}

// static
const Vector<std::unique_ptr<blink::IDBKey>>&
UnionTraits<blink::mojom::IDBKeyDataDataView, std::unique_ptr<blink::IDBKey>>::
    key_array(const std::unique_ptr<blink::IDBKey>& key) {
  return key->Array();
}

// static
Vector<uint8_t>
UnionTraits<blink::mojom::IDBKeyDataDataView, std::unique_ptr<blink::IDBKey>>::
    binary(const std::unique_ptr<blink::IDBKey>& key) {
  const blink::WebData& data = key->Binary();
  Vector<uint8_t> result;
  result.ReserveInitialCapacity(SafeCast<wtf_size_t>(data.size()));
  data.ForEachSegment([&result](const char* segment, size_t segment_size,
                                size_t segment_offset) {
    const auto& segment_span = base::make_span(segment, segment + segment_size);
    result.AppendRange(segment_span.begin(), segment_span.end());
    return true;
  });
  return result;
}

// static
const std::unique_ptr<blink::IDBKey>&
StructTraits<blink::mojom::IDBKeyDataView, std::unique_ptr<blink::IDBKey>>::
    data(const std::unique_ptr<blink::IDBKey>& key) {
  return key;
}

// static
bool StructTraits<
    blink::mojom::IDBKeyDataView,
    std::unique_ptr<blink::IDBKey>>::Read(blink::mojom::IDBKeyDataView data,
                                          std::unique_ptr<blink::IDBKey>* out) {
  return data.ReadData(out);
}

// static
blink::mojom::blink::IDBKeyPathDataPtr
StructTraits<blink::mojom::IDBKeyPathDataView, blink::IDBKeyPath>::data(
    const blink::IDBKeyPath& key_path) {
  if (key_path.GetType() == blink::mojom::IDBKeyPathType::Null)
    return nullptr;

  auto data = blink::mojom::blink::IDBKeyPathData::New();
  switch (key_path.GetType()) {
    case blink::mojom::IDBKeyPathType::String: {
      String key_path_string = key_path.GetString();
      if (key_path_string.IsNull())
        key_path_string = g_empty_string;
      data->set_string(key_path_string);
      return data;
    }
    case blink::mojom::IDBKeyPathType::Array: {
      const auto& array = key_path.Array();
      Vector<String> result;
      result.ReserveInitialCapacity(SafeCast<wtf_size_t>(array.size()));
      for (const auto& item : array)
        result.push_back(item);
      data->set_string_array(result);
      return data;
    }

    case blink::mojom::IDBKeyPathType::Null:
      break;  // Not used, NOTREACHED.
  }
  NOTREACHED();
  return data;
}

// static
bool StructTraits<blink::mojom::IDBKeyPathDataView, blink::IDBKeyPath>::Read(
    blink::mojom::IDBKeyPathDataView data,
    blink::IDBKeyPath* out) {
  blink::mojom::IDBKeyPathDataDataView data_view;
  data.GetDataDataView(&data_view);

  if (data_view.is_null()) {
    *out = blink::IDBKeyPath();
    return true;
  }

  switch (data_view.tag()) {
    case blink::mojom::IDBKeyPathDataDataView::Tag::STRING: {
      String string;
      if (!data_view.ReadString(&string))
        return false;
      *out = blink::IDBKeyPath(string);
      return true;
    }
    case blink::mojom::IDBKeyPathDataDataView::Tag::STRING_ARRAY: {
      Vector<String> array;
      if (!data_view.ReadStringArray(&array))
        return false;
      *out = blink::IDBKeyPath(array);
      return true;
    }
  }

  return false;
}

// static
bool StructTraits<blink::mojom::IDBObjectStoreMetadataDataView,
                  scoped_refptr<blink::IDBObjectStoreMetadata>>::
    Read(blink::mojom::IDBObjectStoreMetadataDataView data,
         scoped_refptr<blink::IDBObjectStoreMetadata>* out) {
  scoped_refptr<blink::IDBObjectStoreMetadata> value =
      blink::IDBObjectStoreMetadata::Create();
  value->id = data.id();
  String name;
  if (!data.ReadName(&name))
    return false;
  value->name = name;
  if (!data.ReadKeyPath(&value->key_path))
    return false;
  value->auto_increment = data.auto_increment();
  value->max_index_id = data.max_index_id();
  MapDataView<int64_t, blink::mojom::IDBIndexMetadataDataView> indexes;
  data.GetIndexesDataView(&indexes);
  value->indexes.ReserveCapacityForSize(SafeCast<wtf_size_t>(indexes.size()));
  for (size_t i = 0; i < indexes.size(); ++i) {
    const int64_t key = indexes.keys()[i];
    scoped_refptr<blink::IDBIndexMetadata> index;
    if (!indexes.values().Read(i, &index))
      return false;
    DCHECK(!value->indexes.Contains(key));
    value->indexes.insert(key, index);
  }
  *out = std::move(value);
  return true;
}

// static
blink::mojom::blink::IDBKeyRangePtr TypeConverter<
    blink::mojom::blink::IDBKeyRangePtr,
    const blink::IDBKeyRange*>::Convert(const blink::IDBKeyRange* input) {
  if (!input) {
    std::unique_ptr<blink::IDBKey> lower = blink::IDBKey::CreateNull();
    std::unique_ptr<blink::IDBKey> upper = blink::IDBKey::CreateNull();
    return blink::mojom::blink::IDBKeyRange::New(
        std::move(lower), std::move(upper), false /* lower_open */,
        false /* upper_open */);
  }

  return blink::mojom::blink::IDBKeyRange::New(
      blink::IDBKey::Clone(input->Lower()),
      blink::IDBKey::Clone(input->Upper()), input->lowerOpen(),
      input->upperOpen());
}

// static
blink::mojom::blink::IDBKeyRangePtr
TypeConverter<blink::mojom::blink::IDBKeyRangePtr,
              blink::IDBKeyRange*>::Convert(blink::IDBKeyRange* input) {
  if (!input) {
    std::unique_ptr<blink::IDBKey> lower = blink::IDBKey::CreateNull();
    std::unique_ptr<blink::IDBKey> upper = blink::IDBKey::CreateNull();
    return blink::mojom::blink::IDBKeyRange::New(
        std::move(lower), std::move(upper), false /* lower_open */,
        false /* upper_open */);
  }

  return blink::mojom::blink::IDBKeyRange::New(
      blink::IDBKey::Clone(input->Lower()),
      blink::IDBKey::Clone(input->Upper()), input->lowerOpen(),
      input->upperOpen());
}

// static
blink::IDBKeyRange*
TypeConverter<blink::IDBKeyRange*, blink::mojom::blink::IDBKeyRangePtr>::
    Convert(const blink::mojom::blink::IDBKeyRangePtr& input) {
  if (!input)
    return nullptr;

  blink::IDBKeyRange::LowerBoundType lower_type =
      blink::IDBKeyRange::kLowerBoundClosed;
  if (input->lower_open)
    lower_type = blink::IDBKeyRange::kLowerBoundOpen;

  blink::IDBKeyRange::UpperBoundType upper_type =
      blink::IDBKeyRange::kUpperBoundClosed;
  if (input->upper_open)
    upper_type = blink::IDBKeyRange::kUpperBoundOpen;

  return blink::IDBKeyRange::Create(
      std::move(input->lower), std::move(input->upper), lower_type, upper_type);
}

}  // namespace mojo
