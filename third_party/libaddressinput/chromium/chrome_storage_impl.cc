// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

#include "base/prefs/writeable_pref_store.h"
#include "base/values.h"

namespace autofill {

ChromeStorageImpl::ChromeStorageImpl(WriteablePrefStore* store)
    : backing_store_(store),
      scoped_observer_(this) {
  scoped_observer_.Add(backing_store_);
}

ChromeStorageImpl::~ChromeStorageImpl() {}

void ChromeStorageImpl::Put(const std::string& key,
                            scoped_ptr<std::string> data) {
  scoped_ptr<base::StringValue> string_value(
      new base::StringValue(std::string()));
  string_value->GetString()->swap(*data);
  backing_store_->SetValue(key, string_value.release());
}

void ChromeStorageImpl::Get(
    const std::string& key,
    scoped_ptr<Storage::Callback> data_ready) const {
  // |Get()| should not be const, so this is just a thunk that fixes that.
  const_cast<ChromeStorageImpl*>(this)->DoGet(key, data_ready.Pass());
}

void ChromeStorageImpl::OnPrefValueChanged(const std::string& key) {}

void ChromeStorageImpl::OnInitializationCompleted(bool succeeded) {
  for (std::vector<Request*>::iterator iter =
           outstanding_requests_.begin();
       iter != outstanding_requests_.end(); ++iter) {
    DoGet((*iter)->key, (*iter)->callback.Pass());
  }

  outstanding_requests_.clear();
}

void ChromeStorageImpl::DoGet(
    const std::string& key,
    scoped_ptr<Storage::Callback> data_ready) {
  if (!backing_store_->IsInitializationComplete()) {
    outstanding_requests_.push_back(
        new Request(key, data_ready.Pass()));
    return;
  }

  const base::Value* value = NULL;
  const base::StringValue* string_value = NULL;
  if (backing_store_->GetValue(key, &value) &&
      value->GetAsString(&string_value)) {
    (*data_ready)(true, key, string_value->GetString());
  } else {
    (*data_ready)(false, key, std::string());
  }
}

ChromeStorageImpl::Request::Request(const std::string& key,
                                    scoped_ptr<Storage::Callback> callback)
    : key(key),
      callback(callback.Pass()) {}

}  // namespace autofill
