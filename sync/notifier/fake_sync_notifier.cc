// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/fake_sync_notifier.h"

namespace syncer {

FakeSyncNotifier::FakeSyncNotifier() {}

FakeSyncNotifier::~FakeSyncNotifier() {}

bool FakeSyncNotifier::IsHandlerRegistered(
    SyncNotifierObserver* handler) const {
  return registrar_.IsHandlerRegisteredForTest(handler);
}

ObjectIdSet FakeSyncNotifier::GetRegisteredIds(
    SyncNotifierObserver* handler) const {
  return registrar_.GetRegisteredIdsForTest(handler);
}

void FakeSyncNotifier::RegisterHandler(SyncNotifierObserver* handler) {
  registrar_.RegisterHandler(handler);
}

const std::string& FakeSyncNotifier::GetUniqueId() const {
  return unique_id_;
}

const std::string& FakeSyncNotifier::GetStateDeprecated() const {
  return state_;
}

const std::string& FakeSyncNotifier::GetCredentialsEmail() const {
  return email_;
}

const std::string& FakeSyncNotifier::GetCredentialsToken() const {
  return token_;
}

ModelTypeSet FakeSyncNotifier::GetLastChangedTypes() const {
  return last_changed_types_;
}

void FakeSyncNotifier::UpdateRegisteredIds(SyncNotifierObserver* handler,
                                           const ObjectIdSet& ids) {
  registrar_.UpdateRegisteredIds(handler, ids);
}

void FakeSyncNotifier::UnregisterHandler(SyncNotifierObserver* handler) {
  registrar_.UnregisterHandler(handler);
}

void FakeSyncNotifier::SetUniqueId(const std::string& unique_id) {
  unique_id_ = unique_id;
}

void FakeSyncNotifier::SetStateDeprecated(const std::string& state) {
  state_ = state;
}

void FakeSyncNotifier::UpdateCredentials(
    const std::string& email, const std::string& token) {
  email_ = email;
  token_ = token;
}

void FakeSyncNotifier::SendNotification(ModelTypeSet changed_types) {
  last_changed_types_ = changed_types;
}

}  // namespace syncer
