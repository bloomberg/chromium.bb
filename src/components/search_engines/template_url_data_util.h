// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_UTIL_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_UTIL_H_

#include <memory>

namespace base {
class DictionaryValue;
}

namespace TemplateURLPrepopulateData {
struct PrepopulatedEngine;
}

struct TemplateURLData;

// Deserializes a TemplateURLData from |dict|.
std::unique_ptr<TemplateURLData> TemplateURLDataFromDictionary(
    const base::DictionaryValue& dict);

// Serializes a TemplateURLData to |dict|.
std::unique_ptr<base::DictionaryValue> TemplateURLDataToDictionary(
    const TemplateURLData& turl_data);

// Create TemplateURLData structure from PrepopulatedEngine structure.
std::unique_ptr<TemplateURLData> TemplateURLDataFromPrepopulatedEngine(
    const TemplateURLPrepopulateData::PrepopulatedEngine& engine);

// Deserializes a TemplateURLData from |dict| as stored in
// kSearchProviderOverrides pref. The field names in |dict| differ from those
// used in the To/FromDictionary functions above for historical reasons.
// TODO(a-v-y) Migrate to single TemplateURLData serialization format.
std::unique_ptr<TemplateURLData> TemplateURLDataFromOverrideDictionary(
    const base::DictionaryValue& engine);

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_UTIL_H_
