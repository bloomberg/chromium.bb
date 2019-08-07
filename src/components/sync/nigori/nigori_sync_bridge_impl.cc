// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_sync_bridge_impl.h"

namespace syncer {

NigoriSyncBridgeImpl::NigoriSyncBridgeImpl() = default;

NigoriSyncBridgeImpl::~NigoriSyncBridgeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriSyncBridgeImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void NigoriSyncBridgeImpl::EnableEncryptEverything() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

bool NigoriSyncBridgeImpl::IsEncryptEverythingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return false;
}

bool NigoriSyncBridgeImpl::NeedKeystoreKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return false;
}

bool NigoriSyncBridgeImpl::SetKeystoreKeys(
    const std::vector<std::string>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return false;
}

base::Optional<ModelError> NigoriSyncBridgeImpl::MergeSyncData(
    const base::Optional<EntityData>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<ModelError> NigoriSyncBridgeImpl::ApplySyncChanges(
    const EntityData& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return base::nullopt;
}

std::unique_ptr<EntityData> NigoriSyncBridgeImpl::GetData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

ConflictResolution NigoriSyncBridgeImpl::ResolveConflict(
    const EntityData& local_data,
    const EntityData& remote_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return ConflictResolution::UseLocal();
}

void NigoriSyncBridgeImpl::ApplyDisableSyncChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

}  // namespace syncer
