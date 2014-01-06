// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_OPTIONAL_DEV_H_
#define PPAPI_CPP_DEV_OPTIONAL_DEV_H_

#include "ppapi/c/dev/pp_optional_structs_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/dev/may_own_ptr_dev.h"
#include "ppapi/cpp/dev/string_wrapper_dev.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/var.h"

namespace pp {

template <typename T>
class Optional;

template <>
class Optional<double> {
 public:
  typedef const PP_Optional_Double_Dev* CInputType;

  Optional() {}

  Optional(double value) { set(value); }

  // Creates an accessor to |storage| but doesn't take ownership of the memory.
  // |storage| must live longer than this object.
  Optional(PP_Optional_Double_Dev* storage, NotOwned)
      : storage_(storage, NOT_OWNED) {
  }

  Optional(const Optional<double>& other) { *storage_ = *other.storage_; }

  Optional<double>& operator=(const Optional<double>& other) {
    return operator=(*other.storage_);
  }

  Optional<double>& operator=(const PP_Optional_Double_Dev& other) {
    if (storage_.get() == &other)
      return *this;

    *storage_ = other;
    return *this;
  }

  bool is_set() const { return PP_ToBool(storage_->is_set); }
  void unset() { storage_->is_set = PP_FALSE; }

  double get() const {
    PP_DCHECK(is_set());
    return storage_->value;
  }

  void set(double value) {
    storage_->value = value;
    storage_->is_set = PP_TRUE;
  }

  const PP_Optional_Double_Dev* ToCInput() const { return storage_.get(); }

  PP_Optional_Double_Dev* StartRawUpdate() { return storage_.get(); }
  void EndRawUpdate() {}

 private:
  internal::MayOwnPtr<PP_Optional_Double_Dev> storage_;
};

template <>
class Optional<std::string> {
 public:
  typedef const PP_Var& CInputType;

  Optional() {}

  Optional(const std::string& value) : wrapper_(value) {}

  Optional(const char* value) : wrapper_(value) {}

  // Creates an accessor to |storage| but doesn't take ownership of the memory.
  // |storage| must live longer than this object.
  //
  // Although this object doesn't own the memory of |storage|, it manages the
  // ref count and sets the var to undefined when destructed.
  Optional(PP_Var* storage, NotOwned) : wrapper_(storage, NOT_OWNED) {}

  Optional(const Optional<std::string>& other) : wrapper_(other.wrapper_) {}

  Optional<std::string>& operator=(const Optional<std::string>& other) {
    wrapper_ = other.wrapper_;
    return *this;
  }

  Optional<std::string>& operator=(const PP_Var& other) {
    wrapper_ = other;
    return *this;
  }

  bool is_set() const { return wrapper_.is_set(); }
  void unset() { wrapper_.unset(); }
  std::string get() const { return wrapper_.get(); }
  void set(const std::string& value) { wrapper_.set(value); }

  const PP_Var& ToCInput() const { return wrapper_.ToVar(); }

  PP_Var* StartRawUpdate() { return wrapper_.StartRawUpdate(); }
  void EndRawUpdate() { wrapper_.EndRawUpdate(); }

 private:
  internal::OptionalStringWrapper wrapper_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_OPTIONAL_DEV_H_
