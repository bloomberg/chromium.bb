// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/nigori_util.h"

#include <queue>
#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/nigori_handler.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/util/cryptographer.h"

namespace syncer {
namespace syncable {

bool ProcessUnsyncedChangesForEncryption(
    WriteTransaction* const trans) {
  NigoriHandler* nigori_handler = trans->directory()->GetNigoriHandler();
  ModelTypeSet encrypted_types = nigori_handler->GetEncryptedTypes(trans);
  Cryptographer* cryptographer = trans->directory()->GetCryptographer(trans);
  DCHECK(cryptographer->is_ready());

  // Get list of all datatypes with unsynced changes. It's possible that our
  // local changes need to be encrypted if encryption for that datatype was
  // just turned on (and vice versa).
  // Note: we do not attempt to re-encrypt data with a new key here as key
  // changes in this code path are likely due to consistency issues (we have
  // to be updated to a key we already have, e.g. an old key).
  std::vector<int64> handles;
  GetUnsyncedEntries(trans, &handles);
  for (size_t i = 0; i < handles.size(); ++i) {
    MutableEntry entry(trans, GET_BY_HANDLE, handles[i]);
    const sync_pb::EntitySpecifics& specifics = entry.Get(SPECIFICS);
    // Ignore types that don't need encryption or entries that are already
    // encrypted.
    if (!SpecificsNeedsEncryption(encrypted_types, specifics))
      continue;
    if (!UpdateEntryWithEncryption(trans, specifics, &entry))
      return false;
  }
  return true;
}

bool VerifyUnsyncedChangesAreEncrypted(
    BaseTransaction* const trans,
    ModelTypeSet encrypted_types) {
  std::vector<int64> handles;
  GetUnsyncedEntries(trans, &handles);
  for (size_t i = 0; i < handles.size(); ++i) {
    Entry entry(trans, GET_BY_HANDLE, handles[i]);
    if (!entry.good()) {
      NOTREACHED();
      return false;
    }
    if (EntryNeedsEncryption(encrypted_types, entry))
      return false;
  }
  return true;
}

bool EntryNeedsEncryption(ModelTypeSet encrypted_types,
                          const Entry& entry) {
  if (!entry.Get(UNIQUE_SERVER_TAG).empty())
    return false;  // We don't encrypt unique server nodes.
  ModelType type = entry.GetModelType();
  if (type == PASSWORDS || IsControlType(type))
    return false;
  // Checking NON_UNIQUE_NAME is not necessary for the correctness of encrypting
  // the data, nor for determining if data is encrypted. We simply ensure it has
  // been overwritten to avoid any possible leaks of sensitive data.
  return SpecificsNeedsEncryption(encrypted_types, entry.Get(SPECIFICS)) ||
         (encrypted_types.Has(type) &&
          entry.Get(NON_UNIQUE_NAME) != kEncryptedString);
}

bool SpecificsNeedsEncryption(ModelTypeSet encrypted_types,
                              const sync_pb::EntitySpecifics& specifics) {
  const ModelType type = GetModelTypeFromSpecifics(specifics);
  if (type == PASSWORDS || IsControlType(type))
    return false;  // These types have their own encryption schemes.
  if (!encrypted_types.Has(type))
    return false;  // This type does not require encryption
  return !specifics.has_encrypted();
}

// Mainly for testing.
bool VerifyDataTypeEncryptionForTest(
    BaseTransaction* const trans,
    ModelType type,
    bool is_encrypted) {
  Cryptographer* cryptographer = trans->directory()->GetCryptographer(trans);
  if (type == PASSWORDS || IsControlType(type)) {
    NOTREACHED();
    return true;
  }
  std::string type_tag = ModelTypeToRootTag(type);
  Entry type_root(trans, GET_BY_SERVER_TAG, type_tag);
  if (!type_root.good()) {
    NOTREACHED();
    return false;
  }

  std::queue<Id> to_visit;
  Id id_string;
  if (!trans->directory()->GetFirstChildId(
          trans, type_root.Get(ID), &id_string)) {
    NOTREACHED();
    return false;
  }
  to_visit.push(id_string);
  while (!to_visit.empty()) {
    id_string = to_visit.front();
    to_visit.pop();
    if (id_string.IsRoot())
      continue;

    Entry child(trans, GET_BY_ID, id_string);
    if (!child.good()) {
      NOTREACHED();
      return false;
    }
    if (child.Get(IS_DIR)) {
      Id child_id_string;
      if (!trans->directory()->GetFirstChildId(
              trans, child.Get(ID), &child_id_string)) {
        NOTREACHED();
        return false;
      }
      // Traverse the children.
      to_visit.push(child_id_string);
    }
    const sync_pb::EntitySpecifics& specifics = child.Get(SPECIFICS);
    DCHECK_EQ(type, child.GetModelType());
    DCHECK_EQ(type, GetModelTypeFromSpecifics(specifics));
    // We don't encrypt the server's permanent items.
    if (child.Get(UNIQUE_SERVER_TAG).empty()) {
      if (specifics.has_encrypted() != is_encrypted)
        return false;
      if (specifics.has_encrypted()) {
        if (child.Get(NON_UNIQUE_NAME) != kEncryptedString)
          return false;
        if (!cryptographer->CanDecryptUsingDefaultKey(specifics.encrypted()))
          return false;
      }
    }
    // Push the successor.
    to_visit.push(child.GetSuccessorId());
  }
  return true;
}

bool UpdateEntryWithEncryption(
    BaseTransaction* const trans,
    const sync_pb::EntitySpecifics& new_specifics,
    syncable::MutableEntry* entry) {
  NigoriHandler* nigori_handler = trans->directory()->GetNigoriHandler();
  Cryptographer* cryptographer = trans->directory()->GetCryptographer(trans);
  ModelType type = GetModelTypeFromSpecifics(new_specifics);
  DCHECK_GE(type, FIRST_REAL_MODEL_TYPE);
  const sync_pb::EntitySpecifics& old_specifics = entry->Get(SPECIFICS);
  const ModelTypeSet encrypted_types = nigori_handler->GetEncryptedTypes(trans);
  // It's possible the nigori lost the set of encrypted types. If the current
  // specifics are already encrypted, we want to ensure we continue encrypting.
  bool was_encrypted = old_specifics.has_encrypted();
  sync_pb::EntitySpecifics generated_specifics;
  if (new_specifics.has_encrypted()) {
    NOTREACHED() << "New specifics already has an encrypted blob.";
    return false;
  }
  if ((!SpecificsNeedsEncryption(encrypted_types, new_specifics) &&
       !was_encrypted) ||
      !cryptographer->is_initialized()) {
    // No encryption required or we are unable to encrypt.
    generated_specifics.CopyFrom(new_specifics);
  } else {
    // Encrypt new_specifics into generated_specifics.
    if (VLOG_IS_ON(2)) {
      scoped_ptr<DictionaryValue> value(entry->ToValue(NULL));
      std::string info;
      base::JSONWriter::WriteWithOptions(value.get(),
                                         base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                         &info);
      DVLOG(2) << "Encrypting specifics of type "
               << ModelTypeToString(type)
               << " with content: "
               << info;
    }
    // Only copy over the old specifics if it is of the right type and already
    // encrypted. The first time we encrypt a node we start from scratch, hence
    // removing all the unencrypted data, but from then on we only want to
    // update the node if the data changes or the encryption key changes.
    if (GetModelTypeFromSpecifics(old_specifics) == type &&
        was_encrypted) {
      generated_specifics.CopyFrom(old_specifics);
    } else {
      AddDefaultFieldValue(type, &generated_specifics);
    }
    // Does not change anything if underlying encrypted blob was already up
    // to date and encrypted with the default key.
    if (!cryptographer->Encrypt(new_specifics,
                                generated_specifics.mutable_encrypted())) {
      NOTREACHED() << "Could not encrypt data for node of type "
                   << ModelTypeToString(type);
      return false;
    }
  }

  // It's possible this entry was encrypted but didn't properly overwrite the
  // non_unique_name (see crbug.com/96314).
  bool encrypted_without_overwriting_name = (was_encrypted &&
      entry->Get(syncable::NON_UNIQUE_NAME) != kEncryptedString);

  // If we're encrypted but the name wasn't overwritten properly we still want
  // to rewrite the entry, irrespective of whether the specifics match.
  if (!encrypted_without_overwriting_name &&
      old_specifics.SerializeAsString() ==
          generated_specifics.SerializeAsString()) {
    DVLOG(2) << "Specifics of type " << ModelTypeToString(type)
             << " already match, dropping change.";
    return true;
  }

  if (generated_specifics.has_encrypted()) {
    // Overwrite the possibly sensitive non-specifics data.
    entry->Put(syncable::NON_UNIQUE_NAME, kEncryptedString);
    // For bookmarks we actually put bogus data into the unencrypted specifics,
    // else the server will try to do it for us.
    if (type == BOOKMARKS) {
      sync_pb::BookmarkSpecifics* bookmark_specifics =
          generated_specifics.mutable_bookmark();
      if (!entry->Get(syncable::IS_DIR))
        bookmark_specifics->set_url(kEncryptedString);
      bookmark_specifics->set_title(kEncryptedString);
    }
  }
  entry->Put(syncable::SPECIFICS, generated_specifics);
  DVLOG(1) << "Overwriting specifics of type "
           << ModelTypeToString(type)
           << " and marking for syncing.";
  syncable::MarkForSyncing(entry);
  return true;
}

void UpdateNigoriFromEncryptedTypes(ModelTypeSet encrypted_types,
                                    bool encrypt_everything,
                                    sync_pb::NigoriSpecifics* nigori) {
  nigori->set_encrypt_everything(encrypt_everything);
  COMPILE_ASSERT(26 == MODEL_TYPE_COUNT, UpdateEncryptedTypes);
  nigori->set_encrypt_bookmarks(
      encrypted_types.Has(BOOKMARKS));
  nigori->set_encrypt_preferences(
      encrypted_types.Has(PREFERENCES));
  nigori->set_encrypt_autofill_profile(
      encrypted_types.Has(AUTOFILL_PROFILE));
  nigori->set_encrypt_autofill(encrypted_types.Has(AUTOFILL));
  nigori->set_encrypt_themes(encrypted_types.Has(THEMES));
  nigori->set_encrypt_typed_urls(
      encrypted_types.Has(TYPED_URLS));
  nigori->set_encrypt_extension_settings(
      encrypted_types.Has(EXTENSION_SETTINGS));
  nigori->set_encrypt_extensions(
      encrypted_types.Has(EXTENSIONS));
  nigori->set_encrypt_search_engines(
      encrypted_types.Has(SEARCH_ENGINES));
  nigori->set_encrypt_sessions(encrypted_types.Has(SESSIONS));
  nigori->set_encrypt_app_settings(
      encrypted_types.Has(APP_SETTINGS));
  nigori->set_encrypt_apps(encrypted_types.Has(APPS));
  nigori->set_encrypt_app_notifications(
      encrypted_types.Has(APP_NOTIFICATIONS));
  nigori->set_encrypt_dictionary(encrypted_types.Has(DICTIONARY));
  nigori->set_encrypt_favicon_images(encrypted_types.Has(FAVICON_IMAGES));
  nigori->set_encrypt_favicon_tracking(encrypted_types.Has(FAVICON_TRACKING));
}

ModelTypeSet GetEncryptedTypesFromNigori(
    const sync_pb::NigoriSpecifics& nigori) {
  if (nigori.encrypt_everything())
    return ModelTypeSet::All();

  ModelTypeSet encrypted_types;
  COMPILE_ASSERT(26 == MODEL_TYPE_COUNT, UpdateEncryptedTypes);
  if (nigori.encrypt_bookmarks())
    encrypted_types.Put(BOOKMARKS);
  if (nigori.encrypt_preferences())
    encrypted_types.Put(PREFERENCES);
  if (nigori.encrypt_autofill_profile())
    encrypted_types.Put(AUTOFILL_PROFILE);
  if (nigori.encrypt_autofill())
    encrypted_types.Put(AUTOFILL);
  if (nigori.encrypt_themes())
    encrypted_types.Put(THEMES);
  if (nigori.encrypt_typed_urls())
    encrypted_types.Put(TYPED_URLS);
  if (nigori.encrypt_extension_settings())
    encrypted_types.Put(EXTENSION_SETTINGS);
  if (nigori.encrypt_extensions())
    encrypted_types.Put(EXTENSIONS);
  if (nigori.encrypt_search_engines())
    encrypted_types.Put(SEARCH_ENGINES);
  if (nigori.encrypt_sessions())
    encrypted_types.Put(SESSIONS);
  if (nigori.encrypt_app_settings())
    encrypted_types.Put(APP_SETTINGS);
  if (nigori.encrypt_apps())
    encrypted_types.Put(APPS);
  if (nigori.encrypt_app_notifications())
    encrypted_types.Put(APP_NOTIFICATIONS);
  if (nigori.encrypt_dictionary())
    encrypted_types.Put(DICTIONARY);
  if (nigori.encrypt_favicon_images())
    encrypted_types.Put(FAVICON_IMAGES);
  if (nigori.encrypt_favicon_tracking())
    encrypted_types.Put(FAVICON_TRACKING);
  return encrypted_types;
}

}  // namespace syncable
}  // namespace syncer
