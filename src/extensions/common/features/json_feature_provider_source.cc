// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/json_feature_provider_source.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

JSONFeatureProviderSource::JSONFeatureProviderSource(const std::string& name)
    : name_(name) {
}

JSONFeatureProviderSource::~JSONFeatureProviderSource() {
}

void JSONFeatureProviderSource::LoadJSON(int resource_id) {
  const base::StringPiece features_file =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
  base::JSONReader::ValueWithError result =
      base::JSONReader::ReadAndReturnValueWithError(features_file);

  DCHECK(result.value) << "Could not load features: " << name_ << " "
                       << result.error_message;

  std::unique_ptr<base::DictionaryValue> value_as_dict;
  if (result.value) {
    CHECK(result.value->is_dict()) << name_;
    value_as_dict = base::DictionaryValue::From(
        base::Value::ToUniquePtrValue(std::move(*result.value)));
  } else {
    // There was some error loading the features file.
    // http://crbug.com/176381
    value_as_dict.reset(new base::DictionaryValue());
  }

  // Ensure there are no key collisions.
  for (base::DictionaryValue::Iterator iter(*value_as_dict); !iter.IsAtEnd();
       iter.Advance()) {
    if (dictionary_.GetWithoutPathExpansion(iter.key(), NULL))
      LOG(FATAL) << "Key " << iter.key() << " is defined in " << name_
                 << " JSON feature files more than once.";
  }

  // Merge.
  dictionary_.MergeDictionary(value_as_dict.get());
}

}  // namespace extensions
