// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base_node.h"

#include <stack>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "sync/internal_api/public/base_transaction.h"
#include "sync/internal_api/syncapi_internal.h"
#include "sync/protocol/app_specifics.pb.h"
#include "sync/protocol/autofill_specifics.pb.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/extension_specifics.pb.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/protocol/theme_specifics.pb.h"
#include "sync/protocol/typed_url_specifics.pb.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/syncable_id.h"
#include "sync/util/time.h"

using sync_pb::AutofillProfileSpecifics;

namespace syncer {

using syncable::SPECIFICS;

// Helper function to look up the int64 metahandle of an object given the ID
// string.
static int64 IdToMetahandle(syncable::BaseTransaction* trans,
                            const syncable::Id& id) {
  syncable::Entry entry(trans, syncable::GET_BY_ID, id);
  if (!entry.good())
    return kInvalidId;
  return entry.GetMetahandle();
}

BaseNode::BaseNode() : password_data_(new sync_pb::PasswordSpecificsData) {}

BaseNode::~BaseNode() {}

bool BaseNode::DecryptIfNecessary() {
  if (!GetEntry()->GetUniqueServerTag().empty())
      return true;  // Ignore unique folders.
  const sync_pb::EntitySpecifics& specifics =
      GetEntry()->GetSpecifics();
  if (specifics.has_password()) {
    // Passwords have their own legacy encryption structure.
    scoped_ptr<sync_pb::PasswordSpecificsData> data(DecryptPasswordSpecifics(
        specifics, GetTransaction()->GetCryptographer()));
    if (!data) {
      LOG(ERROR) << "Failed to decrypt password specifics.";
      return false;
    }
    password_data_.swap(data);
    return true;
  }

  // We assume any node with the encrypted field set has encrypted data and if
  // not we have no work to do, with the exception of bookmarks. For bookmarks
  // we must make sure the bookmarks data has the title field supplied. If not,
  // we fill the unencrypted_data_ with a copy of the bookmark specifics that
  // follows the new bookmarks format.
  if (!specifics.has_encrypted()) {
    if (GetModelType() == BOOKMARKS &&
        !specifics.bookmark().has_title() &&
        !GetTitle().empty()) {  // Last check ensures this isn't a new node.
      // We need to fill in the title.
      std::string title = GetTitle();
      std::string server_legal_title;
      SyncAPINameToServerName(title, &server_legal_title);
      DVLOG(1) << "Reading from legacy bookmark, manually returning title "
               << title;
      unencrypted_data_.CopyFrom(specifics);
      unencrypted_data_.mutable_bookmark()->set_title(
          server_legal_title);
    }
    return true;
  }

  const sync_pb::EncryptedData& encrypted = specifics.encrypted();
  std::string plaintext_data = GetTransaction()->GetCryptographer()->
      DecryptToString(encrypted);
  if (plaintext_data.length() == 0) {
    LOG(ERROR) << "Failed to decrypt encrypted node of type "
               << ModelTypeToString(GetModelType()) << ".";
    // Debugging for crbug.com/123223. We failed to decrypt the data, which
    // means we applied an update without having the key or lost the key at a
    // later point.
    CHECK(false);
    return false;
  } else if (!unencrypted_data_.ParseFromString(plaintext_data)) {
    // Debugging for crbug.com/123223. We should never succeed in decrypting
    // but fail to parse into a protobuf.
    CHECK(false);
    return false;
  }
  DVLOG(2) << "Decrypted specifics of type "
           << ModelTypeToString(GetModelType())
           << " with content: " << plaintext_data;
  return true;
}

const sync_pb::EntitySpecifics& BaseNode::GetUnencryptedSpecifics(
    const syncable::Entry* entry) const {
  const sync_pb::EntitySpecifics& specifics = entry->GetSpecifics();
  if (specifics.has_encrypted()) {
    DCHECK_NE(GetModelTypeFromSpecifics(unencrypted_data_), UNSPECIFIED);
    return unencrypted_data_;
  } else {
    // Due to the change in bookmarks format, we need to check to see if this is
    // a legacy bookmarks (and has no title field in the proto). If it is, we
    // return the unencrypted_data_, which was filled in with the title by
    // DecryptIfNecessary().
    if (GetModelType() == BOOKMARKS) {
      const sync_pb::BookmarkSpecifics& bookmark_specifics =
          specifics.bookmark();
      if (bookmark_specifics.has_title() ||
          GetTitle().empty() ||  // For the empty node case
          !GetEntry()->GetUniqueServerTag().empty()) {
        // It's possible we previously had to convert and set
        // |unencrypted_data_| but then wrote our own data, so we allow
        // |unencrypted_data_| to be non-empty.
        return specifics;
      } else {
        DCHECK_EQ(GetModelTypeFromSpecifics(unencrypted_data_), BOOKMARKS);
        return unencrypted_data_;
      }
    } else {
      DCHECK_EQ(GetModelTypeFromSpecifics(unencrypted_data_), UNSPECIFIED);
      return specifics;
    }
  }
}

int64 BaseNode::GetParentId() const {
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(),
                        GetEntry()->GetParentId());
}

int64 BaseNode::GetId() const {
  return GetEntry()->GetMetahandle();
}

base::Time BaseNode::GetModificationTime() const {
  return GetEntry()->GetMtime();
}

bool BaseNode::GetIsFolder() const {
  return GetEntry()->GetIsDir();
}

std::string BaseNode::GetTitle() const {
  std::string result;
  // TODO(zea): refactor bookmarks to not need this functionality.
  if (BOOKMARKS == GetModelType() &&
      GetEntry()->GetSpecifics().has_encrypted()) {
    // Special case for legacy bookmarks dealing with encryption.
    ServerNameToSyncAPIName(GetBookmarkSpecifics().title(), &result);
  } else {
    ServerNameToSyncAPIName(GetEntry()->GetNonUniqueName(),
                            &result);
  }
  return result;
}

bool BaseNode::HasChildren() const {
  syncable::Directory* dir = GetTransaction()->GetDirectory();
  syncable::BaseTransaction* trans = GetTransaction()->GetWrappedTrans();
  return dir->HasChildren(trans, GetEntry()->GetId());
}

int64 BaseNode::GetPredecessorId() const {
  syncable::Id id_string = GetEntry()->GetPredecessorId();
  if (id_string.IsRoot())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

int64 BaseNode::GetSuccessorId() const {
  syncable::Id id_string = GetEntry()->GetSuccessorId();
  if (id_string.IsRoot())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

int64 BaseNode::GetFirstChildId() const {
  syncable::Id id_string = GetEntry()->GetFirstChildId();
  if (id_string.IsRoot())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

void BaseNode::GetChildIds(std::vector<int64>* result) const {
  GetEntry()->GetChildHandles(result);
}

int BaseNode::GetTotalNodeCount() const {
  return GetEntry()->GetTotalNodeCount();
}

int BaseNode::GetPositionIndex() const {
  return GetEntry()->GetPositionIndex();
}

base::DictionaryValue* BaseNode::ToValue() const {
  return GetEntry()->ToValue(GetTransaction()->GetCryptographer());
}

int64 BaseNode::GetExternalId() const {
  return GetEntry()->GetLocalExternalId();
}

const sync_pb::AppSpecifics& BaseNode::GetAppSpecifics() const {
  DCHECK_EQ(GetModelType(), APPS);
  return GetEntitySpecifics().app();
}

const sync_pb::AutofillSpecifics& BaseNode::GetAutofillSpecifics() const {
  DCHECK_EQ(GetModelType(), AUTOFILL);
  return GetEntitySpecifics().autofill();
}

const AutofillProfileSpecifics& BaseNode::GetAutofillProfileSpecifics() const {
  DCHECK_EQ(GetModelType(), AUTOFILL_PROFILE);
  return GetEntitySpecifics().autofill_profile();
}

const sync_pb::BookmarkSpecifics& BaseNode::GetBookmarkSpecifics() const {
  DCHECK_EQ(GetModelType(), BOOKMARKS);
  return GetEntitySpecifics().bookmark();
}

const sync_pb::NigoriSpecifics& BaseNode::GetNigoriSpecifics() const {
  DCHECK_EQ(GetModelType(), NIGORI);
  return GetEntitySpecifics().nigori();
}

const sync_pb::PasswordSpecificsData& BaseNode::GetPasswordSpecifics() const {
  DCHECK_EQ(GetModelType(), PASSWORDS);
  return *password_data_;
}

const sync_pb::ThemeSpecifics& BaseNode::GetThemeSpecifics() const {
  DCHECK_EQ(GetModelType(), THEMES);
  return GetEntitySpecifics().theme();
}

const sync_pb::TypedUrlSpecifics& BaseNode::GetTypedUrlSpecifics() const {
  DCHECK_EQ(GetModelType(), TYPED_URLS);
  return GetEntitySpecifics().typed_url();
}

const sync_pb::ExtensionSpecifics& BaseNode::GetExtensionSpecifics() const {
  DCHECK_EQ(GetModelType(), EXTENSIONS);
  return GetEntitySpecifics().extension();
}

const sync_pb::SessionSpecifics& BaseNode::GetSessionSpecifics() const {
  DCHECK_EQ(GetModelType(), SESSIONS);
  return GetEntitySpecifics().session();
}

const sync_pb::DeviceInfoSpecifics& BaseNode::GetDeviceInfoSpecifics() const {
  DCHECK_EQ(GetModelType(), DEVICE_INFO);
  return GetEntitySpecifics().device_info();
}

const sync_pb::ExperimentsSpecifics& BaseNode::GetExperimentsSpecifics() const {
  DCHECK_EQ(GetModelType(), EXPERIMENTS);
  return GetEntitySpecifics().experiments();
}

const sync_pb::PriorityPreferenceSpecifics&
    BaseNode::GetPriorityPreferenceSpecifics() const {
  DCHECK_EQ(GetModelType(), PRIORITY_PREFERENCES);
  return GetEntitySpecifics().priority_preference();
}

const sync_pb::EntitySpecifics& BaseNode::GetEntitySpecifics() const {
  return GetUnencryptedSpecifics(GetEntry());
}

ModelType BaseNode::GetModelType() const {
  return GetEntry()->GetModelType();
}

const syncer::AttachmentIdList BaseNode::GetAttachmentIds() const {
  AttachmentIdList result;
  const sync_pb::AttachmentMetadata& metadata =
      GetEntry()->GetAttachmentMetadata();
  for (int i = 0; i < metadata.record_size(); ++i) {
    result.push_back(AttachmentId::CreateFromProto(metadata.record(i).id()));
  }
  return result;
}

void BaseNode::SetUnencryptedSpecifics(
    const sync_pb::EntitySpecifics& specifics) {
  ModelType type = GetModelTypeFromSpecifics(specifics);
  DCHECK_NE(UNSPECIFIED, type);
  if (GetModelType() != UNSPECIFIED) {
    DCHECK_EQ(GetModelType(), type);
  }
  unencrypted_data_.CopyFrom(specifics);
}

}  // namespace syncer
