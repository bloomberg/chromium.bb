// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_TO_VAR_CONVERTOR_H_
#define PPAPI_CPP_EXTENSIONS_TO_VAR_CONVERTOR_H_

#include <string>

#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/extensions/optional.h"
#include "ppapi/cpp/var.h"

namespace pp {
namespace ext {
namespace internal {

class ToVarConverterBase {
 public:
  PP_Var pp_var() const {
    return var_.pp_var();
  }

  const Var& var() const {
    return var_;
  }

 protected:
  ToVarConverterBase() {
  }

  explicit ToVarConverterBase(const PP_Var& var) : var_(var) {
  }

  explicit ToVarConverterBase(const Var& var): var_(var) {
  }

  ~ToVarConverterBase() {
  }

  Var var_;
};

template <class T>
class ToVarConverter : public ToVarConverterBase {
 public:
  explicit ToVarConverter(const T& object)
      : ToVarConverterBase(object.CreateVar()) {
  }

  ~ToVarConverter() {
  }
};

template <class T>
class ToVarConverter<Optional<T> > : public ToVarConverterBase {
 public:
  explicit ToVarConverter(const Optional<T>& object)
      : ToVarConverterBase(
          object.IsSet() ? ToVarConverter<T>(*object).pp_var() :
                           PP_MakeUndefined()) {
  }

  ~ToVarConverter() {
  }
};

template <>
class ToVarConverter<std::string> : public ToVarConverterBase {
 public:
  explicit ToVarConverter(const std::string& object)
      : ToVarConverterBase(Var(object)) {
  }

  ~ToVarConverter() {
  }
};

template <>
class ToVarConverter<double> : public ToVarConverterBase {
 public:
  explicit ToVarConverter(double object) : ToVarConverterBase(Var(object)) {
  }

  ~ToVarConverter() {
  }
};

}  // namespace internal
}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_TO_VAR_CONVERTOR_H_
