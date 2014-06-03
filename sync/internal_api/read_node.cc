// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/read_node.h"

#include "base/logging.h"
#include "sync/internal_api/public/base_transaction.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/syncable_base_transaction.h"
#include "sync/syncable/syncable_util.h"

namespace syncer {

//////////////////////////////////////////////////////////////////////////
// ReadNode member definitions
ReadNode::ReadNode(const BaseTransaction* transaction)
    : entry_(NULL), transaction_(transaction) {
  DCHECK(transaction);
}

ReadNode::ReadNode() {
  entry_ = NULL;
  transaction_ = NULL;
}

ReadNode::~ReadNode() {
  delete entry_;
}

void ReadNode::InitByRootLookup() {
  DCHECK(!entry_) << "Init called twice";
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_ID, trans->root_id());
  if (!entry_->good())
    DCHECK(false) << "Could not lookup root node for reading.";
}

BaseNode::InitByLookupResult ReadNode::InitByIdLookup(int64 id) {
  DCHECK(!entry_) << "Init called twice";
  DCHECK_NE(id, kInvalidId);
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_HANDLE, id);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  ModelType model_type = GetModelType();
  LOG_IF(WARNING, model_type == UNSPECIFIED || model_type == TOP_LEVEL_FOLDER)
      << "SyncAPI InitByIdLookup referencing unusual object.";
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

BaseNode::InitByLookupResult ReadNode::InitByClientTagLookup(
    ModelType model_type,
    const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return INIT_FAILED_PRECONDITION;

  const std::string hash = syncable::GenerateSyncableHash(model_type, tag);

  entry_ = new syncable::Entry(transaction_->GetWrappedTrans(),
                               syncable::GET_BY_CLIENT_TAG, hash);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

const syncable::Entry* ReadNode::GetEntry() const {
  return entry_;
}

const BaseTransaction* ReadNode::GetTransaction() const {
  return transaction_;
}

BaseNode::InitByLookupResult ReadNode::InitByTagLookupForBookmarks(
    const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return INIT_FAILED_PRECONDITION;
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_SERVER_TAG, tag);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  ModelType model_type = GetModelType();
  DCHECK_EQ(model_type, BOOKMARKS)
      << "InitByTagLookup deprecated for all types except bookmarks.";
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

BaseNode::InitByLookupResult ReadNode::InitTypeRoot(ModelType type) {
  DCHECK(!entry_) << "Init called twice";
  if (!IsRealDataType(type))
    return INIT_FAILED_PRECONDITION;
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_TYPE_ROOT, type);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  ModelType found_model_type = GetModelType();
  LOG_IF(WARNING, found_model_type == UNSPECIFIED ||
                  found_model_type == TOP_LEVEL_FOLDER)
      << "SyncAPI InitTypeRoot referencing unusually typed object.";
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

}  // namespace syncer
