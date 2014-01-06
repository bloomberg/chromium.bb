// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/string_wrapper_dev.h"

#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/var.h"

namespace pp {
namespace internal {

OptionalStringWrapper::OptionalStringWrapper() {
}

OptionalStringWrapper::OptionalStringWrapper(const std::string& value) {
  *storage_ = Var(value).Detach();
}

OptionalStringWrapper::OptionalStringWrapper(PP_Var* storage, NotOwned)
    : storage_(storage, NOT_OWNED) {
  PP_DCHECK(storage_->type == PP_VARTYPE_STRING ||
            storage_->type == PP_VARTYPE_UNDEFINED);
}

OptionalStringWrapper::OptionalStringWrapper(
    const OptionalStringWrapper& other) {
  // Add one ref.
  *storage_ = Var(*other.storage_).Detach();
}

OptionalStringWrapper::~OptionalStringWrapper() {
  unset();
}

OptionalStringWrapper& OptionalStringWrapper::operator=(
    const OptionalStringWrapper& other) {
  return operator=(*other.storage_);
}

OptionalStringWrapper& OptionalStringWrapper::operator=(const PP_Var& other) {
  if (storage_.get() == &other)
    return *this;

  Var auto_release(PASS_REF, *storage_);
  // Add one ref.
  *storage_ = Var(other).Detach();
  return *this;
}

bool OptionalStringWrapper::is_set() const {
  PP_DCHECK(storage_->type == PP_VARTYPE_STRING ||
            storage_->type == PP_VARTYPE_UNDEFINED);
  return storage_->type == PP_VARTYPE_STRING;
}

void OptionalStringWrapper::unset() {
  Var auto_release(PASS_REF, *storage_);
  *storage_ = PP_MakeUndefined();
}

std::string OptionalStringWrapper::get() const {
  // TODO(yzshen): consider adding a cache.
  Var var(*storage_);
  if (var.is_string()) {
    return var.AsString();
  } else {
    PP_NOTREACHED();
    return std::string();
  }
}

void OptionalStringWrapper::set(const std::string& value) {
  Var auto_release(PASS_REF, *storage_);
  *storage_ = Var(value).Detach();
}

PP_Var* OptionalStringWrapper::StartRawUpdate() {
  unset();
  return storage_.get();
}

void OptionalStringWrapper::EndRawUpdate() {
  PP_DCHECK(storage_->type == PP_VARTYPE_STRING ||
            storage_->type == PP_VARTYPE_UNDEFINED);
}

StringWrapper::StringWrapper() : storage_(std::string()) {
}

StringWrapper::StringWrapper(const std::string& value) : storage_(value) {
}

StringWrapper::StringWrapper(PP_Var* storage, NotOwned)
    : storage_(storage, NOT_OWNED) {
  if (!storage_.is_set())
    storage_.set(std::string());
}

StringWrapper::StringWrapper(const StringWrapper& other)
    : storage_(other.storage_) {
}

StringWrapper::~StringWrapper() {
}

StringWrapper& StringWrapper::operator=(const StringWrapper& other) {
  storage_ = other.storage_;
  return *this;
}

StringWrapper& StringWrapper::operator=(const PP_Var& other) {
  PP_DCHECK(other.type == PP_VARTYPE_STRING);
  storage_ = other;
  return *this;
}

PP_Var* StringWrapper::StartRawUpdate() {
  return storage_.StartRawUpdate();
}

void StringWrapper::EndRawUpdate() {
  storage_.EndRawUpdate();
  if (!storage_.is_set())
    storage_.set(std::string());
}

}  // namespace internal
}  // namespace pp
