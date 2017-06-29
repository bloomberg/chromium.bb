// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/embedder/manifest_utils.h"

#include "base/memory/ptr_util.h"

namespace service_manager {

namespace {

// Similar to base::DictionaryValue::MergeDictionary(), except concatenates
// ListValue contents.
// This is explicitly not part of base::DictionaryValue at brettw's request.
void MergeDictionary(base::DictionaryValue* target,
                     const base::DictionaryValue* source) {
  for (base::DictionaryValue::Iterator it(*source); !it.IsAtEnd();
       it.Advance()) {
    const base::Value* merge_value = &it.value();
    // Check whether we have to merge dictionaries.
    if (merge_value->IsType(base::Value::Type::DICTIONARY)) {
      base::DictionaryValue* sub_dict;
      if (target->GetDictionaryWithoutPathExpansion(it.key(), &sub_dict)) {
        MergeDictionary(sub_dict,
                        static_cast<const base::DictionaryValue*>(merge_value));
        continue;
      }
    }
    if (merge_value->IsType(base::Value::Type::LIST)) {
      const base::ListValue* merge_list = nullptr;
      if (merge_value->GetAsList(&merge_list)) {
        base::ListValue* target_list = nullptr;
        if (target->GetListWithoutPathExpansion(it.key(), &target_list)) {
          for (size_t i = 0; i < merge_list->GetSize(); ++i) {
            const base::Value* element = nullptr;
            CHECK(merge_list->Get(i, &element));
            target_list->Append(element->CreateDeepCopy());
          }
          continue;
        }
      }
    }
    // All other cases: Make a copy and hook it up.
    target->SetWithoutPathExpansion(
        it.key(), base::MakeUnique<base::Value>(*merge_value));
  }
}

}  // namespace

void MergeManifestWithOverlay(base::Value* manifest, base::Value* overlay) {
  if (!overlay)
    return;

  base::DictionaryValue* manifest_dictionary = nullptr;
  bool result = manifest->GetAsDictionary(&manifest_dictionary);
  DCHECK(result);
  base::DictionaryValue* overlay_dictionary = nullptr;
  result = overlay->GetAsDictionary(&overlay_dictionary);
  DCHECK(result);
  MergeDictionary(manifest_dictionary, overlay_dictionary);
}

}  // namespace service_manager
