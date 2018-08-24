// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexeddb_struct_traits.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

namespace mojo {

using blink::mojom::IDBCursorDirection;
using blink::mojom::IDBDataLoss;
using blink::mojom::IDBOperationType;

// static
IDBCursorDirection
EnumTraits<IDBCursorDirection, blink::WebIDBCursorDirection>::ToMojom(
    blink::WebIDBCursorDirection input) {
  switch (input) {
    case blink::kWebIDBCursorDirectionNext:
      return IDBCursorDirection::Next;
    case blink::kWebIDBCursorDirectionNextNoDuplicate:
      return IDBCursorDirection::NextNoDuplicate;
    case blink::kWebIDBCursorDirectionPrev:
      return IDBCursorDirection::Prev;
    case blink::kWebIDBCursorDirectionPrevNoDuplicate:
      return IDBCursorDirection::PrevNoDuplicate;
  }
  NOTREACHED();
  return IDBCursorDirection::Next;
}

// static
bool EnumTraits<IDBCursorDirection, blink::WebIDBCursorDirection>::FromMojom(
    IDBCursorDirection input,
    blink::WebIDBCursorDirection* output) {
  switch (input) {
    case IDBCursorDirection::Next:
      *output = blink::kWebIDBCursorDirectionNext;
      return true;
    case IDBCursorDirection::NextNoDuplicate:
      *output = blink::kWebIDBCursorDirectionNextNoDuplicate;
      return true;
    case IDBCursorDirection::Prev:
      *output = blink::kWebIDBCursorDirectionPrev;
      return true;
    case IDBCursorDirection::PrevNoDuplicate:
      *output = blink::kWebIDBCursorDirectionPrevNoDuplicate;
      return true;
  }
  return false;
}

// static
IDBDataLoss EnumTraits<IDBDataLoss, blink::WebIDBDataLoss>::ToMojom(
    blink::WebIDBDataLoss input) {
  switch (input) {
    case blink::kWebIDBDataLossNone:
      return IDBDataLoss::None;
    case blink::kWebIDBDataLossTotal:
      return IDBDataLoss::Total;
  }
  NOTREACHED();
  return IDBDataLoss::None;
}

// static
bool EnumTraits<IDBDataLoss, blink::WebIDBDataLoss>::FromMojom(
    IDBDataLoss input,
    blink::WebIDBDataLoss* output) {
  switch (input) {
    case IDBDataLoss::None:
      *output = blink::kWebIDBDataLossNone;
      return true;
    case IDBDataLoss::Total:
      *output = blink::kWebIDBDataLossTotal;
      return true;
  }
  return false;
}

// static
blink::mojom::IDBKeyDataPtr
StructTraits<blink::mojom::IDBKeyDataView, blink::IndexedDBKey>::data(
    const blink::IndexedDBKey& key) {
  auto data = blink::mojom::IDBKeyData::New();
  switch (key.type()) {
    case blink::kWebIDBKeyTypeInvalid:
      data->set_other(blink::mojom::IDBDatalessKeyType::Invalid);
      return data;
    case blink::kWebIDBKeyTypeArray:
      data->set_key_array(key.array());
      return data;
    case blink::kWebIDBKeyTypeBinary:
      data->set_binary(std::vector<uint8_t>(
          key.binary().data(), key.binary().data() + key.binary().size()));
      return data;
    case blink::kWebIDBKeyTypeString:
      data->set_string(key.string());
      return data;
    case blink::kWebIDBKeyTypeDate:
      data->set_date(key.date());
      return data;
    case blink::kWebIDBKeyTypeNumber:
      data->set_number(key.number());
      return data;
    case blink::kWebIDBKeyTypeNull:
      data->set_other(blink::mojom::IDBDatalessKeyType::Null);
      return data;
    case blink::kWebIDBKeyTypeMin:
      break;
  }
  NOTREACHED();
  return data;
}

// static
bool StructTraits<blink::mojom::IDBKeyDataView, blink::IndexedDBKey>::Read(
    blink::mojom::IDBKeyDataView data,
    blink::IndexedDBKey* out) {
  blink::mojom::IDBKeyDataDataView data_view;
  data.GetDataDataView(&data_view);

  switch (data_view.tag()) {
    case blink::mojom::IDBKeyDataDataView::Tag::KEY_ARRAY: {
      std::vector<blink::IndexedDBKey> array;
      if (!data_view.ReadKeyArray(&array))
        return false;
      *out = blink::IndexedDBKey(array);
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::BINARY: {
      std::vector<uint8_t> binary;
      if (!data_view.ReadBinary(&binary))
        return false;
      *out = blink::IndexedDBKey(
          std::string(binary.data(), binary.data() + binary.size()));
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::STRING: {
      base::string16 string;
      if (!data_view.ReadString(&string))
        return false;
      *out = blink::IndexedDBKey(string);
      return true;
    }
    case blink::mojom::IDBKeyDataDataView::Tag::DATE:
      *out = blink::IndexedDBKey(data_view.date(), blink::kWebIDBKeyTypeDate);
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::NUMBER:
      *out =
          blink::IndexedDBKey(data_view.number(), blink::kWebIDBKeyTypeNumber);
      return true;
    case blink::mojom::IDBKeyDataDataView::Tag::OTHER:
      switch (data_view.other()) {
        case blink::mojom::IDBDatalessKeyType::Invalid:
          *out = blink::IndexedDBKey(blink::kWebIDBKeyTypeInvalid);
          return true;
        case blink::mojom::IDBDatalessKeyType::Null:
          *out = blink::IndexedDBKey(blink::kWebIDBKeyTypeNull);
          return true;
      }
  }

  return false;
}

// static
blink::mojom::IDBKeyPathDataPtr
StructTraits<blink::mojom::IDBKeyPathDataView, blink::IndexedDBKeyPath>::data(
    const blink::IndexedDBKeyPath& key_path) {
  if (key_path.IsNull())
    return nullptr;

  auto data = blink::mojom::IDBKeyPathData::New();
  switch (key_path.type()) {
    case blink::kWebIDBKeyPathTypeString:
      data->set_string(key_path.string());
      return data;
    case blink::kWebIDBKeyPathTypeArray:
      data->set_string_array(key_path.array());
      return data;

    // The following key path types are not used.
    case blink::kWebIDBKeyPathTypeNull:;  // No-op, fall out of switch block to
                                          // NOTREACHED().
  }
  NOTREACHED();
  return data;
}

// static
bool StructTraits<blink::mojom::IDBKeyPathDataView, blink::IndexedDBKeyPath>::
    Read(blink::mojom::IDBKeyPathDataView data, blink::IndexedDBKeyPath* out) {
  blink::mojom::IDBKeyPathDataDataView data_view;
  data.GetDataDataView(&data_view);

  if (data_view.is_null()) {
    *out = blink::IndexedDBKeyPath();
    return true;
  }

  switch (data_view.tag()) {
    case blink::mojom::IDBKeyPathDataDataView::Tag::STRING: {
      base::string16 string;
      if (!data_view.ReadString(&string))
        return false;
      *out = blink::IndexedDBKeyPath(string);
      return true;
    }
    case blink::mojom::IDBKeyPathDataDataView::Tag::STRING_ARRAY: {
      std::vector<base::string16> array;
      if (!data_view.ReadStringArray(&array))
        return false;
      *out = blink::IndexedDBKeyPath(array);
      return true;
    }
  }

  return false;
}

// static
IDBOperationType
EnumTraits<IDBOperationType, blink::WebIDBOperationType>::ToMojom(
    blink::WebIDBOperationType input) {
  switch (input) {
    case blink::kWebIDBAdd:
      return IDBOperationType::Add;
    case blink::kWebIDBPut:
      return IDBOperationType::Put;
    case blink::kWebIDBDelete:
      return IDBOperationType::Delete;
    case blink::kWebIDBClear:
      return IDBOperationType::Clear;
    case blink::kWebIDBOperationTypeCount:
      // WebIDBOperationTypeCount is not a valid option.
      break;
  }
  NOTREACHED();
  return IDBOperationType::Add;
}

// static
bool EnumTraits<IDBOperationType, blink::WebIDBOperationType>::FromMojom(
    IDBOperationType input,
    blink::WebIDBOperationType* output) {
  switch (input) {
    case IDBOperationType::Add:
      *output = blink::kWebIDBAdd;
      return true;
    case IDBOperationType::Put:
      *output = blink::kWebIDBPut;
      return true;
    case IDBOperationType::Delete:
      *output = blink::kWebIDBDelete;
      return true;
    case IDBOperationType::Clear:
      *output = blink::kWebIDBClear;
      return true;
  }
  return false;
}

}  // namespace mojo
