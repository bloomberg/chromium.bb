// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_EXTENSIONS_OUTPUT_TRAITS_H_
#define PPAPI_CPP_EXTENSIONS_OUTPUT_TRAITS_H_

#include <vector>

#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/dev/var_array_dev.h"
#include "ppapi/cpp/extensions/from_var_converter.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/var.h"

namespace pp {
namespace ext {
namespace internal {

// Base class for all those types within the pp::ext namespace that are used
// with CompletionCallbackWithOutput. This class doesn't do anything itself,
// but it affects the behavior of CallbackOutputTraits for all its subclasses.
//
// TODO(yzshen): Within pp::ext, basic types such as std::string or double may
// be used with CompletionCallbackWithOutput as well. This approach doesn't
// work for them. One way is to refactor CallbackOutputTraits to consider not
// only the output C++ object type, but also the output parameter type that the
// C interface uses. And then we can remove this class.
class OutputObjectBase {
};

template <class T>
class VarOutputAdapterWithStorage {
 public:
  VarOutputAdapterWithStorage() : pp_var_(PP_MakeUndefined()) {
  }

  ~VarOutputAdapterWithStorage() {
    PP_DCHECK(pp_var_.type == PP_VARTYPE_UNDEFINED);
  }

  PP_Var& pp_var() { return pp_var_; }

  T& output() {
    Var auto_release(PASS_REF, pp_var_);
    converter_.Set(pp_var_);
    pp_var_ = PP_MakeUndefined();
    return converter_.value();
  }

 private:
  PP_Var pp_var_;
  FromVarConverter<T> converter_;

  // Disallow copying and assignment.
  VarOutputAdapterWithStorage(const VarOutputAdapterWithStorage<T>&);
  VarOutputAdapterWithStorage<T>& operator=(
      const VarOutputAdapterWithStorage<T>&);
};

template <class T>
struct ExtensionsCallbackOutputTraits {
  typedef PP_Var* APIArgType;
  typedef VarOutputAdapterWithStorage<T> StorageType;

  static inline APIArgType StorageToAPIArg(StorageType& t) {
    return &t.pp_var();
  }

  // This must be called exactly once to consume the one PP_Var reference
  // assigned to us by the browser.
  static inline T& StorageToPluginArg(StorageType& t) {
    return t.output();
  }
};

// This class provides storage for a PP_Var and a vector of objects which are
// of type T. The PP_Var is used as an output parameter to recevie an array var
// from the browser. Each element in the array var is converted to a T object,
// using FromVarConverter, and stores in the vector.
template <class T>
class ArrayVarOutputAdapterWithStorage {
 public:
  ArrayVarOutputAdapterWithStorage() : pp_var_(PP_MakeUndefined()) {
  }

  ~ArrayVarOutputAdapterWithStorage() {
    PP_DCHECK(pp_var_.type == PP_VARTYPE_UNDEFINED);
  }

  PP_Var& pp_var() { return pp_var_; }

  std::vector<T>& output() {
    PP_DCHECK(output_storage_.empty());

    Var var(PASS_REF, pp_var_);
    pp_var_ = PP_MakeUndefined();
    if (var.is_array()) {
      VarArray_Dev array(var);

      uint32_t length = array.GetLength();
      output_storage_.reserve(length);
      for (uint32_t i = 0; i < length; ++i) {
        FromVarConverter<T> converter(array.Get(i).pp_var());
        output_storage_.push_back(converter.value());
      }
    }

    return output_storage_;
  }

 private:
  PP_Var pp_var_;
  std::vector<T> output_storage_;

  // Disallow copying and assignment.
  ArrayVarOutputAdapterWithStorage(const ArrayVarOutputAdapterWithStorage<T>&);
  ArrayVarOutputAdapterWithStorage<T>& operator=(
      const ArrayVarOutputAdapterWithStorage<T>&);
};

template <class T>
struct ExtensionsVectorCallbackOutputTraits {
  typedef PP_Var* APIArgType;
  typedef ArrayVarOutputAdapterWithStorage<T> StorageType;

  static inline APIArgType StorageToAPIArg(StorageType& t) {
    return &t.pp_var();
  }

  // This must be called exactly once to consume the one PP_Var reference
  // assigned to us by the browser.
  static inline std::vector<T>& StorageToPluginArg(StorageType& t) {
    return t.output();
  }
};

}  // namespace internal
}  // namespace ext
}  // namespace pp

#endif  // PPAPI_CPP_EXTENSIONS_OUTPUT_TRAITS_H_
