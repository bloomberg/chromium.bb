// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_DICT_FIELD_H_
#define PPAPI_CPP_EXTENSIONS_DICT_FIELD_H_

#include <string>

#include "ppapi/cpp/extensions/optional.h"

namespace pp {

class Var;

namespace ext {

template <class T>
class DictField {
 public:
  explicit DictField(const std::string& in_key) : key(in_key), value() {
  }

  ~DictField() {
  }

  // Adds this field to the dictionary var.
  bool AddTo(Var* /* var */) const {
    // TODO(yzshen): change Var to DictionaryVar and add support.
    return true;
  }

  bool Populate(const Var& /* var */) {
    // TODO(yzshen): change Var to DictionaryVar and add support.
    return true;
  }

  const std::string key;
  T value;
};

template <class T>
class OptionalDictField {
 public:
  explicit OptionalDictField(const std::string& in_key) : key(in_key) {
  }

  ~OptionalDictField() {
  }

  // Adds this field to the dictionary var, if |value| has been set.
  bool MayAddTo(Var* /* var */) const {
    // TODO(yzshen): change Var to DictionaryVar and add support
    return true;
  }

  bool Populate(const Var& /* var */) {
    // TODO(yzshen): change Var to DictionaryVar and add support.
    return true;
  }

  const std::string key;
  Optional<T> value;
};

}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_DICT_FIELD_H_
