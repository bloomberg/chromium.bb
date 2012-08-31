// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/invalidation_state.h"

#include "base/values.h"

namespace syncer {

bool AckHandle::Equals(const AckHandle& other) const {
  return true;
}

scoped_ptr<base::Value> AckHandle::ToValue() const {
  return scoped_ptr<base::Value>(new DictionaryValue());
}

bool AckHandle::ResetFromValue(const base::Value& value) {
  return true;
}

bool InvalidationState::Equals(const InvalidationState& other) const {
  return (payload == other.payload) && ack_handle.Equals(other.ack_handle);
}

scoped_ptr<base::DictionaryValue> InvalidationState::ToValue() const {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString("payload", payload);
  value->Set("ackHandle", ack_handle.ToValue().release());
  return value.Pass();
}

bool InvalidationState::ResetFromValue(const base::DictionaryValue& value) {
  const base::Value* ack_handle_value = NULL;
  return
      value.GetString("payload", &payload) &&
      value.Get("ackHandle", &ack_handle_value) &&
      ack_handle.ResetFromValue(*ack_handle_value);
}

}  // namespace syncer
