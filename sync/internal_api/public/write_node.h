// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_WRITE_NODE_H_
#define SYNC_INTERNAL_API_PUBLIC_WRITE_NODE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base_node.h"

namespace sync_pb {
class AppSpecifics;
class AutofillSpecifics;
class AutofillProfileSpecifics;
class BookmarkSpecifics;
class EntitySpecifics;
class ExtensionSpecifics;
class SessionSpecifics;
class NigoriSpecifics;
class PasswordSpecificsData;
class ThemeSpecifics;
class TypedUrlSpecifics;
}

namespace syncer {

class Cryptographer;
class WriteTransaction;

namespace syncable {
class Entry;
class MutableEntry;
}

// WriteNode extends BaseNode to add mutation, and wraps
// syncable::MutableEntry. A WriteTransaction is needed to create a WriteNode.
class SYNC_EXPORT WriteNode : public BaseNode {
 public:
  enum InitUniqueByCreationResult {
    INIT_SUCCESS,
    // The tag passed into this method was empty.
    INIT_FAILED_EMPTY_TAG,
    // An entry with this tag already exists.
    INIT_FAILED_ENTRY_ALREADY_EXISTS,
    // The constructor for a new MutableEntry with the specified data failed.
    INIT_FAILED_COULD_NOT_CREATE_ENTRY,
    // Setting the predecessor failed
    INIT_FAILED_SET_PREDECESSOR,
  };

  // Create a WriteNode using the given transaction.
  explicit WriteNode(WriteTransaction* transaction);
  virtual ~WriteNode();

  // A client must use one (and only one) of the following Init variants to
  // populate the node.

  // BaseNode implementation.
  virtual InitByLookupResult InitByIdLookup(int64 id) OVERRIDE;
  virtual InitByLookupResult InitByClientTagLookup(
      ModelType model_type,
      const std::string& tag) OVERRIDE;

  // Create a new node with the specified parent and predecessor.  |model_type|
  // dictates the type of the item, and controls which EntitySpecifics proto
  // extension can be used with this item.  Use a NULL |predecessor|
  // to indicate that this is to be the first child.
  // |predecessor| must be a child of |new_parent| or NULL. Returns false on
  // failure.
  bool InitByCreation(ModelType model_type,
                      const BaseNode& parent,
                      const BaseNode* predecessor);

  // Create nodes using this function if they're unique items that
  // you want to fetch using client_tag. Note that the behavior of these
  // items is slightly different than that of normal items.
  // Most importantly, if it exists locally, this function will
  // actually undelete it
  // Client unique tagged nodes must NOT be folders.
  InitUniqueByCreationResult InitUniqueByCreation(
      ModelType model_type,
      const BaseNode& parent,
      const std::string& client_tag);

  // Each server-created permanent node is tagged with a unique string.
  // Look up the node with the particular tag.  If it does not exist,
  // return false.
  InitByLookupResult InitByTagLookup(const std::string& tag);

  // These Set() functions correspond to the Get() functions of BaseNode.
  void SetIsFolder(bool folder);
  void SetTitle(const std::wstring& title);

  // External ID is a client-only field, so setting it doesn't cause the item to
  // be synced again.
  void SetExternalId(int64 external_id);

  // Remove this node and its children.
  void Remove();

  // Set a new parent and position.  Position is specified by |predecessor|; if
  // it is NULL, the node is moved to the first position.  |predecessor| must
  // be a child of |new_parent| or NULL.  Returns false on failure..
  bool SetPosition(const BaseNode& new_parent, const BaseNode* predecessor);

  // Set the bookmark specifics (url and favicon).
  // Should only be called if GetModelType() == BOOKMARK.
  void SetBookmarkSpecifics(const sync_pb::BookmarkSpecifics& specifics);

  // Generic set specifics method. Will extract the model type from |specifics|.
  void SetEntitySpecifics(const sync_pb::EntitySpecifics& specifics);

  // Resets the EntitySpecifics for this node based on the unencrypted data.
  // Will encrypt if necessary.
  void ResetFromSpecifics();

  // TODO(sync): Remove the setters below when the corresponding data
  // types are ported to the new sync service API.

  // Set the app specifics (id, update url, enabled state, etc).
  // Should only be called if GetModelType() == APPS.
  void SetAppSpecifics(const sync_pb::AppSpecifics& specifics);

  // Set the autofill specifics (name and value).
  // Should only be called if GetModelType() == AUTOFILL.
  void SetAutofillSpecifics(const sync_pb::AutofillSpecifics& specifics);

  void SetAutofillProfileSpecifics(
      const sync_pb::AutofillProfileSpecifics& specifics);

  // Set the nigori specifics.
  // Should only be called if GetModelType() == NIGORI.
  void SetNigoriSpecifics(const sync_pb::NigoriSpecifics& specifics);

  // Set the password specifics.
  // Should only be called if GetModelType() == PASSWORD.
  void SetPasswordSpecifics(const sync_pb::PasswordSpecificsData& specifics);

  // Set the theme specifics (name and value).
  // Should only be called if GetModelType() == THEME.
  void SetThemeSpecifics(const sync_pb::ThemeSpecifics& specifics);

  // Set the typed_url specifics (url, title, typed_count, etc).
  // Should only be called if GetModelType() == TYPED_URLS.
  void SetTypedUrlSpecifics(const sync_pb::TypedUrlSpecifics& specifics);

  // Set the extension specifics (id, update url, enabled state, etc).
  // Should only be called if GetModelType() == EXTENSIONS.
  void SetExtensionSpecifics(const sync_pb::ExtensionSpecifics& specifics);

  // Set the session specifics (windows, tabs, navigations etc.).
  // Should only be called if GetModelType() == SESSIONS.
  void SetSessionSpecifics(const sync_pb::SessionSpecifics& specifics);

  // Set the device info specifics.
  // Should only be called if GetModelType() == DEVICE_INFO.
  void SetDeviceInfoSpecifics(const sync_pb::DeviceInfoSpecifics& specifics);

  // Set the experiments specifics.
  // Should only be called if GetModelType() == EXPERIMENTS.
  void SetExperimentsSpecifics(const sync_pb::ExperimentsSpecifics& specifics);

  // Set the priority preference specifics.
  // Should only be called if GetModelType() == PRIORITY_PREFERENCE.
  void SetPriorityPreferenceSpecifics(
      const sync_pb::PriorityPreferenceSpecifics& specifics);

  // Implementation of BaseNode's abstract virtual accessors.
  virtual const syncable::Entry* GetEntry() const OVERRIDE;

  virtual const BaseTransaction* GetTransaction() const OVERRIDE;

  syncable::MutableEntry* GetMutableEntryForTest();

 private:
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, EncryptBookmarksWithLegacyData);

  void* operator new(size_t size);  // Node is meant for stack use only.

  // Helper to set model type. This will clear any specifics data.
  void PutModelType(ModelType model_type);

  // Helper to set the previous node.
  bool PutPredecessor(const BaseNode* predecessor) WARN_UNUSED_RESULT;

  // Sets IS_UNSYNCED and SYNCING to ensure this entry is considered in an
  // upcoming commit pass.
  void MarkForSyncing();

  // The underlying syncable object which this class wraps.
  syncable::MutableEntry* entry_;

  // The sync API transaction that is the parent of this node.
  WriteTransaction* transaction_;

  DISALLOW_COPY_AND_ASSIGN(WriteNode);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_WRITE_NODE_H_
