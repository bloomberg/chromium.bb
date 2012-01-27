// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_JSON_SCHEMA_COMPILER_UTIL_H__
#define TOOLS_JSON_SCHEMA_COMPILER_UTIL_H__
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

namespace json_schema_compiler {
namespace util {

// Creates a new vector containing the strings |from|.|name| at |out|. Returns
// false if there is no list at the specified key or if the list has anything
// other than strings.
bool GetStrings(
    const base::DictionaryValue& from,
    const std::string& name,
    std::vector<std::string>* out);

// Creates a new vector containing the strings |from|.|name| at |out|. Returns
// true on success or if there is nothing at the specified key. Returns false
// if anything other than a list of strings is at the specified key.
bool GetOptionalStrings(
    const base::DictionaryValue& from,
    const std::string& name,
    scoped_ptr<std::vector<std::string> >* out);

// Puts the each string in |from| into a new ListValue at |out|.|name|.
void SetStrings(
    const std::vector<std::string>& from,
    const std::string& name,
    base::DictionaryValue* out);

// If from is non-NULL, puts each string in |from| into a new ListValue at
// |out|.|name|.
void SetOptionalStrings(
    const scoped_ptr<std::vector<std::string> >& from,
    const std::string& name,
    base::DictionaryValue* out);

}  // namespace api_util
}  // namespace extensions

#endif // TOOLS_JSON_SCHEMA_COMPILER_UTIL_H__
