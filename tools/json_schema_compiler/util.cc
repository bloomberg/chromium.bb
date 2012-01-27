// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/util.h"

#include "base/values.h"

namespace json_schema_compiler {
namespace util {

bool GetStrings(
    const base::DictionaryValue& from,
    const std::string& name,
    std::vector<std::string>* out) {
  base::ListValue* list = NULL;
  if (!from.GetListWithoutPathExpansion(name, &list))
    return false;

  std::string string;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    if (!list->GetString(i, &string))
      return false;
    out->push_back(string);
  }

  return true;
}

bool GetOptionalStrings(
    const base::DictionaryValue& from,
    const std::string& name,
    scoped_ptr<std::vector<std::string> >* out) {
  base::ListValue* list = NULL;
  {
    base::Value* maybe_list = NULL;
    // Since |name| is optional, its absence is acceptable. However, anything
    // other than a ListValue is not.
    if (!from.GetWithoutPathExpansion(name, &maybe_list))
      return true;
    if (!maybe_list->IsType(base::Value::TYPE_LIST))
      return false;
    list = static_cast<base::ListValue*>(maybe_list);
  }

  out->reset(new std::vector<std::string>());
  std::string string;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    if (!list->GetString(i, &string)) {
      out->reset();
      return false;
    }
    (*out)->push_back(string);
  }

  return true;
}

void SetStrings(
    const std::vector<std::string>& from,
    const std::string& name,
    base::DictionaryValue* out) {
  base::ListValue* list = new base::ListValue();
  out->SetWithoutPathExpansion(name, list);
  for (std::vector<std::string>::const_iterator it = from.begin();
      it != from.end(); ++it) {
    list->Append(base::Value::CreateStringValue(*it));
  }
}

void SetOptionalStrings(
    const scoped_ptr<std::vector<std::string> >& from,
    const std::string& name,
    base::DictionaryValue* out) {
  if (!from.get())
    return;

  SetStrings(*from, name, out);
}

}  // namespace api_util
}  // namespace extensions
