// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_FROM_VAR_CONVERTOR_H_
#define PPAPI_CPP_EXTENSIONS_FROM_VAR_CONVERTOR_H_

#include <string>

#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/var.h"

namespace pp {
namespace ext {
namespace internal {

template <class T>
class FromVarConverterBase {
 public:
  T& value() { return value_; }

 protected:
  FromVarConverterBase() : value_() {
  }

  explicit FromVarConverterBase(const T& value) : value_(value) {
  }

  ~FromVarConverterBase() {
  }

  T value_;
};

template <class T>
class FromVarConverter : public FromVarConverterBase<T> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(PASS_REF, Var(var).Detach());
  }

  ~FromVarConverter() {
  }

  void Set(PassRef pass_ref, const PP_Var& var) {
    Var auto_release(pass_ref, var);

    bool succeeded = FromVarConverterBase<T>::value_.Populate(var);
    // Suppress unused variable warnings.
    static_cast<void>(succeeded);
    PP_DCHECK(succeeded);
  }
};

template <>
class FromVarConverter<std::string> : public FromVarConverterBase<std::string> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(PASS_REF, Var(var).Detach());
  }

  ~FromVarConverter() {
  }

  void Set(PassRef pass_ref, const PP_Var& var) {
    FromVarConverterBase<std::string>::value_ = Var(pass_ref, var).AsString();
  }
};

template <>
class FromVarConverter<double> : public FromVarConverterBase<double> {
 public:
  FromVarConverter() {
  }

  FromVarConverter(const PP_Var& var) {
    Set(PASS_REF, Var(var).Detach());
  }

  ~FromVarConverter() {
  }

  void Set(PassRef pass_ref, const PP_Var& var) {
    FromVarConverterBase<double>::value_ = Var(pass_ref, var).AsDouble();
  }
};

}  // namespace internal
}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_FROM_VAR_CONVERTOR_H_
