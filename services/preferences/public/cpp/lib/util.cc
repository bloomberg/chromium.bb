// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/lib/util.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace prefs {

void SetValue(base::DictionaryValue* dictionary_value,
              const std::vector<base::StringPiece>& path_components,
              std::unique_ptr<base::Value> value) {
  for (size_t i = 0; i < path_components.size() - 1; ++i) {
    if (!dictionary_value->GetDictionaryWithoutPathExpansion(
            path_components[i], &dictionary_value)) {
      auto new_dict_value_owner = base::MakeUnique<base::DictionaryValue>();
      auto* new_dict_value = new_dict_value_owner.get();
      dictionary_value->SetWithoutPathExpansion(
          path_components[i], std::move(new_dict_value_owner));
      dictionary_value = new_dict_value;
    }
  }
  const auto& key = path_components.back();
  if (value)
    dictionary_value->SetWithoutPathExpansion(key, std::move(value));
  else
    dictionary_value->RemoveWithoutPathExpansion(key, nullptr);
}

}  // namespace prefs
