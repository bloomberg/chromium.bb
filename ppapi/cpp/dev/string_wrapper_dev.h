// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_STRING_WRAPPER_DEV_H_
#define PPAPI_CPP_DEV_STRING_WRAPPER_DEV_H_

#include <string>

#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/dev/may_own_ptr_dev.h"

namespace pp {
namespace internal {

// An optional string backed by a PP_Var. When the string is not set, the type
// of the PP_Var is set to PP_VARTYPE_UNDEFINED; otherwise, it is set to
// PP_VARTYPE_STRING.
class OptionalStringWrapper {
 public:
  OptionalStringWrapper();

  explicit OptionalStringWrapper(const std::string& value);

  // Creates an accessor to |storage| but doesn't take ownership of the memory.
  // |storage| must live longer than this object.
  //
  // Although this object doesn't own the memory of |storage|, it manages the
  // ref count and sets the var to undefined when destructed.
  OptionalStringWrapper(PP_Var* storage, NotOwned);

  OptionalStringWrapper(const OptionalStringWrapper& other);

  ~OptionalStringWrapper();

  OptionalStringWrapper& operator=(const OptionalStringWrapper& other);
  OptionalStringWrapper& operator=(const PP_Var& other);

  bool is_set() const;
  void unset();
  std::string get() const;
  void set(const std::string& value);

  const PP_Var& ToVar() const { return *storage_; }

  // Sets the underlying PP_Var to undefined before returning it.
  PP_Var* StartRawUpdate();
  void EndRawUpdate();

 private:
  MayOwnPtr<PP_Var> storage_;
};

// A string backed by a PP_Var whose type is PP_VARTYPE_STRING.
class StringWrapper {
 public:
  StringWrapper();

  explicit StringWrapper(const std::string& value);

  // Creates an accessor to |storage| but doesn't take ownership of the memory.
  // |storage| must live longer than this object.
  //
  // Although this object doesn't own the memory of |storage|, it manages the
  // ref count and sets the var to undefined when destructed.
  StringWrapper(PP_Var* storage, NotOwned);

  StringWrapper(const StringWrapper& other);

  ~StringWrapper();

  StringWrapper& operator=(const StringWrapper& other);
  StringWrapper& operator=(const PP_Var& other);

  std::string get() const { return storage_.get(); }
  void set(const std::string& value) { return storage_.set(value); }

  const PP_Var& ToVar() const { return storage_.ToVar(); }

  // Sets the underlying PP_Var to undefined before returning it.
  PP_Var* StartRawUpdate();
  // If the underlying PP_Var wasn't updated to a valid string, sets it to an
  // empty string.
  void EndRawUpdate();

 private:
  OptionalStringWrapper storage_;
};

}  // namespace internal
}  // namespace pp

#endif  // PPAPI_CPP_DEV_STRING_WRAPPER_DEV_H_
