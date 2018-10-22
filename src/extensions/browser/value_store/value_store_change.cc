// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store_change.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"

// static
std::string ValueStoreChange::ToJson(
    const ValueStoreChangeList& changes) {
  base::DictionaryValue changes_value;
  for (ValueStoreChangeList::const_iterator it = changes.begin();
      it != changes.end(); ++it) {
    std::unique_ptr<base::DictionaryValue> change_value =
        std::make_unique<base::DictionaryValue>();
    if (it->old_value()) {
      change_value->SetKey("oldValue", it->old_value()->Clone());
    }
    if (it->new_value()) {
      change_value->SetKey("newValue", it->new_value()->Clone());
    }
    changes_value.SetWithoutPathExpansion(it->key(), std::move(change_value));
  }
  std::string json;
  bool success = base::JSONWriter::Write(changes_value, &json);
  DCHECK(success);
  return json;
}

ValueStoreChange::ValueStoreChange(const std::string& key,
                                   std::unique_ptr<base::Value> old_value,
                                   std::unique_ptr<base::Value> new_value)
    : inner_(new Inner(key, std::move(old_value), std::move(new_value))) {}

ValueStoreChange::ValueStoreChange(const ValueStoreChange& other) = default;

ValueStoreChange::~ValueStoreChange() {}

const std::string& ValueStoreChange::key() const {
  DCHECK(inner_.get());
  return inner_->key_;
}

const base::Value* ValueStoreChange::old_value() const {
  DCHECK(inner_.get());
  return inner_->old_value_.get();
}

const base::Value* ValueStoreChange::new_value() const {
  DCHECK(inner_.get());
  return inner_->new_value_.get();
}

ValueStoreChange::Inner::Inner(const std::string& key,
                               std::unique_ptr<base::Value> old_value,
                               std::unique_ptr<base::Value> new_value)
    : key_(key),
      old_value_(std::move(old_value)),
      new_value_(std::move(new_value)) {}

ValueStoreChange::Inner::~Inner() {}
