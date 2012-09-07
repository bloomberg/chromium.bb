// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/fake_invalidator.h"

namespace syncer {

FakeInvalidator::FakeInvalidator() {}

FakeInvalidator::~FakeInvalidator() {}

bool FakeInvalidator::IsHandlerRegistered(InvalidationHandler* handler) const {
  return registrar_.IsHandlerRegisteredForTest(handler);
}

ObjectIdSet FakeInvalidator::GetRegisteredIds(
    InvalidationHandler* handler) const {
  return registrar_.GetRegisteredIds(handler);
}

const std::string& FakeInvalidator::GetUniqueId() const {
  return unique_id_;
}

const std::string& FakeInvalidator::GetStateDeprecated() const {
  return state_;
}

const std::string& FakeInvalidator::GetCredentialsEmail() const {
  return email_;
}

const std::string& FakeInvalidator::GetCredentialsToken() const {
  return token_;
}

const ObjectIdStateMap& FakeInvalidator::GetLastSentIdStateMap() const {
  return last_sent_id_state_map_;
}

void FakeInvalidator::EmitOnInvalidatorStateChange(InvalidatorState state) {
  registrar_.UpdateInvalidatorState(state);
}

void FakeInvalidator::EmitOnIncomingInvalidation(
    const ObjectIdStateMap& id_state_map,
    IncomingInvalidationSource source) {
  registrar_.DispatchInvalidationsToHandlers(id_state_map, source);
}

void FakeInvalidator::RegisterHandler(InvalidationHandler* handler) {
  registrar_.RegisterHandler(handler);
}

void FakeInvalidator::UpdateRegisteredIds(InvalidationHandler* handler,
                                           const ObjectIdSet& ids) {
  registrar_.UpdateRegisteredIds(handler, ids);
}

void FakeInvalidator::UnregisterHandler(InvalidationHandler* handler) {
  registrar_.UnregisterHandler(handler);
}

InvalidatorState FakeInvalidator::GetInvalidatorState() const {
  return registrar_.GetInvalidatorState();
}

void FakeInvalidator::SetUniqueId(const std::string& unique_id) {
  unique_id_ = unique_id;
}

void FakeInvalidator::SetStateDeprecated(const std::string& state) {
  state_ = state;
}

void FakeInvalidator::UpdateCredentials(
    const std::string& email, const std::string& token) {
  email_ = email;
  token_ = token;
}

void FakeInvalidator::SendInvalidation(const ObjectIdStateMap& id_state_map) {
  last_sent_id_state_map_ = id_state_map;
}

}  // namespace syncer
