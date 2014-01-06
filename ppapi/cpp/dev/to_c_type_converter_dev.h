// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_TO_C_TYPE_CONVERTER_DEV_H_
#define PPAPI_CPP_DEV_TO_C_TYPE_CONVERTER_DEV_H_

#include <string>

#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/dev/optional_dev.h"
#include "ppapi/cpp/dev/string_wrapper_dev.h"

namespace pp {
namespace internal {

// ToCTypeConverter is used to convert C++ API input parameters into C API input
// parameters.

// Generic converter for C struct wrappers.
template <typename T>
class ToCTypeConverter {
 public:
  explicit ToCTypeConverter(const T& object) : wrapper_(object) {}

  ~ToCTypeConverter() {}

  const typename T::CType* ToCInput() const { return wrapper_.ToStruct(); }

 private:
  // Disallow copying and assignment.
  ToCTypeConverter(const ToCTypeConverter<T>&);
  ToCTypeConverter<T>& operator=(const ToCTypeConverter<T>&);

  const T& wrapper_;
};

template <>
class ToCTypeConverter<std::string> {
 public:
  explicit ToCTypeConverter(const std::string& object) : wrapper_(object) {}

  ~ToCTypeConverter() {}

  const PP_Var& ToCInput() const { return wrapper_.ToVar(); }

 private:
  // Disallow copying and assignment.
  ToCTypeConverter(const ToCTypeConverter<std::string>&);
  ToCTypeConverter<std::string>& operator=(
      const ToCTypeConverter<std::string>&);

  StringWrapper wrapper_;
};

template <>
class ToCTypeConverter<double> {
 public:
  explicit ToCTypeConverter(double object) : storage_(object) {}

  ~ToCTypeConverter() {}

  double ToCInput() const { return storage_; }

 private:
  // Disallow copying and assignment.
  ToCTypeConverter(const ToCTypeConverter<double>&);
  ToCTypeConverter<double>& operator=(const ToCTypeConverter<double>&);

  double storage_;
};

template <typename T>
class ToCTypeConverter<Optional<T> > {
 public:
  explicit ToCTypeConverter(const Optional<T>& object) : wrapper_(object) {}

  ~ToCTypeConverter() {}

  typename Optional<T>::CInputType ToCInput() const {
    return wrapper_.ToCInput();
  }

 private:
  // Disallow copying and assignment.
  ToCTypeConverter(const ToCTypeConverter<Optional<T> >&);
  ToCTypeConverter<Optional<T> >& operator=(
      const ToCTypeConverter<Optional<T> >&);

  const Optional<T>& wrapper_;
};

}  // namespace internal
}  // namespace pp

#endif  // PPAPI_CPP_DEV_TO_C_TYPE_CONVERTER_DEV_H_
