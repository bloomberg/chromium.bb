// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_DICT_FIELD_H_
#define PPAPI_CPP_EXTENSIONS_DICT_FIELD_H_

#include <string>

#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/extensions/from_var_converter.h"
#include "ppapi/cpp/extensions/optional.h"
#include "ppapi/cpp/extensions/to_var_converter.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"

namespace pp {
namespace ext {

template <class T>
class DictField {
 public:
  explicit DictField(const std::string& key) : key_(key), value_() {
  }

  ~DictField() {
  }

  const std::string& key() const { return key_; }

  // Returns the value.
  T& operator()() { return value_; }
  const T& operator()() const { return value_; }

  // Adds this field to the dictionary var.
  bool AddTo(VarDictionary* dict) const {
    if (!dict)
      return false;

    internal::ToVarConverter<T> converter(value_);
    return dict->Set(Var(key_), converter.var());
  }

  bool Populate(const VarDictionary& dict) {
    Var value_var = dict.Get(Var(key_));
    if (value_var.is_undefined())
      return false;

    internal::FromVarConverter<T> converter(value_var.pp_var());
    value_ = converter.value();
    return true;
  }

 private:
  std::string key_;
  T value_;
};

template <class T>
class OptionalDictField {
 public:
  explicit OptionalDictField(const std::string& key) : key_(key) {
  }

  ~OptionalDictField() {
  }

  const std::string& key() const { return key_; }

  // Returns the value.
  Optional<T>& operator()() { return value_; }
  const Optional<T>& operator()() const { return value_; }

  // Adds this field to the dictionary var, if |value| has been set.
  bool MayAddTo(VarDictionary* dict) const {
    if (!dict)
      return false;
    if (!value_.IsSet())
      return true;

    internal::ToVarConverter<T> converter(*value_);
    return dict->Set(Var(key_), converter.var());
  }

  bool Populate(const VarDictionary& dict) {
    Var value_var = dict.Get(Var(key_));
    internal::FromVarConverter<Optional<T> > converter(value_var.pp_var());
    value_.Swap(&converter.value());
    return true;
  }

 private:
  std::string key_;
  Optional<T> value_;
};

}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_DICT_FIELD_H_
