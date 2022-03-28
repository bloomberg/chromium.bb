// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/values_mojom_traits.h"

#include <memory>
#include <utility>

#include "base/strings/string_piece.h"

namespace mojo {

bool StructTraits<
    mojo_base::mojom::DictionaryValueDataView,
    base::Value::Dict>::Read(mojo_base::mojom::DictionaryValueDataView data,
                             base::Value::Dict* out) {
  mojo::MapDataView<mojo::StringDataView, mojo_base::mojom::ValueDataView> view;
  data.GetStorageDataView(&view);
  for (size_t i = 0; i < view.size(); ++i) {
    base::StringPiece key;
    base::Value value;
    if (!view.keys().Read(i, &key) || !view.values().Read(i, &value))
      return false;
    out->Set(key, std::move(value));
  }
  return true;
}

bool StructTraits<mojo_base::mojom::ListValueDataView, base::Value::List>::Read(
    mojo_base::mojom::ListValueDataView data,
    base::Value::List* out) {
  mojo::ArrayDataView<mojo_base::mojom::ValueDataView> view;
  data.GetStorageDataView(&view);
  base::Value element;
  for (size_t i = 0; i < view.size(); ++i) {
    if (!view.Read(i, &element))
      return false;
    out->Append(std::move(element));
  }
  return true;
}

bool StructTraits<
    mojo_base::mojom::DeprecatedDictionaryValueDataView,
    base::Value>::Read(mojo_base::mojom::DeprecatedDictionaryValueDataView data,
                       base::Value* value_out) {
  mojo::MapDataView<mojo::StringDataView, mojo_base::mojom::ValueDataView> view;
  data.GetStorageDataView(&view);
  std::vector<base::Value::DictStorage::value_type> dict_storage;
  dict_storage.reserve(view.size());
  for (size_t i = 0; i < view.size(); ++i) {
    base::StringPiece key;
    base::Value value;
    if (!view.keys().Read(i, &key) || !view.values().Read(i, &value))
      return false;
    dict_storage.emplace_back(std::string(key), std::move(value));
  }
  *value_out = base::Value(base::Value::DictStorage(std::move(dict_storage)));
  return true;
}

bool StructTraits<mojo_base::mojom::DeprecatedListValueDataView, base::Value>::
    Read(mojo_base::mojom::DeprecatedListValueDataView data,
         base::Value* value_out) {
  mojo::ArrayDataView<mojo_base::mojom::ValueDataView> view;
  data.GetStorageDataView(&view);
  base::Value::ListStorage list_storage(view.size());
  for (size_t i = 0; i < view.size(); ++i) {
    if (!view.Read(i, &list_storage[i]))
      return false;
  }
  *value_out = base::Value(std::move(list_storage));
  return true;
}

bool UnionTraits<mojo_base::mojom::ValueDataView, base::Value>::Read(
    mojo_base::mojom::ValueDataView data,
    base::Value* value_out) {
  switch (data.tag()) {
    case mojo_base::mojom::ValueDataView::Tag::NULL_VALUE: {
      *value_out = base::Value();
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::BOOL_VALUE: {
      *value_out = base::Value(data.bool_value());
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::INT_VALUE: {
      *value_out = base::Value(data.int_value());
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::DOUBLE_VALUE: {
      *value_out = base::Value(data.double_value());
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::STRING_VALUE: {
      base::StringPiece string_piece;
      if (!data.ReadStringValue(&string_piece))
        return false;
      *value_out = base::Value(string_piece);
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::BINARY_VALUE: {
      mojo::ArrayDataView<uint8_t> binary_data_view;
      data.GetBinaryValueDataView(&binary_data_view);
      const char* data_pointer =
          reinterpret_cast<const char*>(binary_data_view.data());
      base::Value::BlobStorage blob_storage(
          data_pointer, data_pointer + binary_data_view.size());
      *value_out = base::Value(std::move(blob_storage));
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::DICTIONARY_VALUE: {
      base::Value::Dict dict;
      if (!data.ReadDictionaryValue(&dict))
        return false;
      *value_out = base::Value(std::move(dict));
      return true;
    }
    case mojo_base::mojom::ValueDataView::Tag::LIST_VALUE: {
      base::Value::List list;
      if (!data.ReadListValue(&list))
        return false;
      *value_out = base::Value(std::move(list));
      return true;
    }
  }
  return false;
}

}  // namespace mojo
