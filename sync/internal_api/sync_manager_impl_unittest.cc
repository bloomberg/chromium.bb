// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the SyncApi. Note that a lot of the underlying
// functionality is provided by the Syncable layer, which has its own
// unit tests. We'll test SyncApi specific things in this harness.

#include <cstddef>
#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/test/values_test_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "sync/engine/sync_scheduler.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/engine/polling_constants.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/http_post_provider_interface.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/test/test_entry_factory.h"
#include "sync/internal_api/public/test/test_internal_components_factory.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/internal_api/sync_encryption_handler_impl.h"
#include "sync/internal_api/sync_manager_impl.h"
#include "sync/internal_api/syncapi_internal.h"
#include "sync/js/js_arg_list.h"
#include "sync/js/js_backend.h"
#include "sync/js/js_event_handler.h"
#include "sync/js/js_reply_handler.h"
#include "sync/js/js_test_util.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/fake_invalidator.h"
#include "sync/notifier/invalidation_handler.h"
#include "sync/notifier/invalidator.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/extension_specifics.pb.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/protocol/preference_specifics.pb.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/nigori_util.h"
#include "sync/syncable/syncable_id.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/callback_counter.h"
#include "sync/test/engine/fake_sync_scheduler.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/fake_encryptor.h"
#include "sync/test/fake_extensions_activity_monitor.h"
#include "sync/util/cryptographer.h"
#include "sync/util/extensions_activity_monitor.h"
#include "sync/util/test_unrecoverable_error_handler.h"
#include "sync/util/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ExpectDictStringValue;
using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace syncer {

using sessions::SyncSessionSnapshot;
using syncable::GET_BY_HANDLE;
using syncable::IS_DEL;
using syncable::IS_UNSYNCED;
using syncable::NON_UNIQUE_NAME;
using syncable::SPECIFICS;
using syncable::kEncryptedString;

namespace {

const char kTestChromeVersion[] = "test chrome version";

void ExpectInt64Value(int64 expected_value,
                      const base::DictionaryValue& value,
                      const std::string& key) {
  std::string int64_str;
  EXPECT_TRUE(value.GetString(key, &int64_str));
  int64 val = 0;
  EXPECT_TRUE(base::StringToInt64(int64_str, &val));
  EXPECT_EQ(expected_value, val);
}

void ExpectTimeValue(const base::Time& expected_value,
                     const base::DictionaryValue& value,
                     const std::string& key) {
  std::string time_str;
  EXPECT_TRUE(value.GetString(key, &time_str));
  EXPECT_EQ(GetTimeDebugString(expected_value), time_str);
}

// Makes a non-folder child of the root node.  Returns the id of the
// newly-created node.
int64 MakeNode(UserShare* share,
               ModelType model_type,
               const std::string& client_tag) {
  WriteTransaction trans(FROM_HERE, share);
  ReadNode root_node(&trans);
  root_node.InitByRootLookup();
  WriteNode node(&trans);
  WriteNode::InitUniqueByCreationResult result =
      node.InitUniqueByCreation(model_type, root_node, client_tag);
  EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
  node.SetIsFolder(false);
  return node.GetId();
}

// Makes a folder child of a non-root node. Returns the id of the
// newly-created node.
int64 MakeFolderWithParent(UserShare* share,
                           ModelType model_type,
                           int64 parent_id,
                           BaseNode* predecessor) {
  WriteTransaction trans(FROM_HERE, share);
  ReadNode parent_node(&trans);
  EXPECT_EQ(BaseNode::INIT_OK, parent_node.InitByIdLookup(parent_id));
  WriteNode node(&trans);
  EXPECT_TRUE(node.InitBookmarkByCreation(parent_node, predecessor));
  node.SetIsFolder(true);
  return node.GetId();
}

int64 MakeBookmarkWithParent(UserShare* share,
                             int64 parent_id,
                             BaseNode* predecessor) {
  WriteTransaction trans(FROM_HERE, share);
  ReadNode parent_node(&trans);
  EXPECT_EQ(BaseNode::INIT_OK, parent_node.InitByIdLookup(parent_id));
  WriteNode node(&trans);
  EXPECT_TRUE(node.InitBookmarkByCreation(parent_node, predecessor));
  return node.GetId();
}

// Creates the "synced" root node for a particular datatype. We use the syncable
// methods here so that the syncer treats these nodes as if they were already
// received from the server.
int64 MakeServerNodeForType(UserShare* share,
                            ModelType model_type) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(model_type, &specifics);
  syncable::WriteTransaction trans(
      FROM_HERE, syncable::UNITTEST, share->directory.get());
  // Attempt to lookup by nigori tag.
  std::string type_tag = ModelTypeToRootTag(model_type);
  syncable::Id node_id = syncable::Id::CreateFromServerId(type_tag);
  syncable::MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                               node_id);
  EXPECT_TRUE(entry.good());
  entry.Put(syncable::BASE_VERSION, 1);
  entry.Put(syncable::SERVER_VERSION, 1);
  entry.Put(syncable::IS_UNAPPLIED_UPDATE, false);
  entry.Put(syncable::SERVER_PARENT_ID, syncable::GetNullId());
  entry.Put(syncable::SERVER_IS_DIR, true);
  entry.Put(syncable::IS_DIR, true);
  entry.Put(syncable::SERVER_SPECIFICS, specifics);
  entry.Put(syncable::UNIQUE_SERVER_TAG, type_tag);
  entry.Put(syncable::NON_UNIQUE_NAME, type_tag);
  entry.Put(syncable::IS_DEL, false);
  entry.Put(syncable::SPECIFICS, specifics);
  return entry.Get(syncable::META_HANDLE);
}

// Simulates creating a "synced" node as a child of the root datatype node.
int64 MakeServerNode(UserShare* share, ModelType model_type,
                     const std::string& client_tag,
                     const std::string& hashed_tag,
                     const sync_pb::EntitySpecifics& specifics) {
  syncable::WriteTransaction trans(
      FROM_HERE, syncable::UNITTEST, share->directory.get());
  syncable::Entry root_entry(&trans, syncable::GET_BY_SERVER_TAG,
                             ModelTypeToRootTag(model_type));
  EXPECT_TRUE(root_entry.good());
  syncable::Id root_id = root_entry.Get(syncable::ID);
  syncable::Id node_id = syncable::Id::CreateFromServerId(client_tag);
  syncable::MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                               node_id);
  EXPECT_TRUE(entry.good());
  entry.Put(syncable::BASE_VERSION, 1);
  entry.Put(syncable::SERVER_VERSION, 1);
  entry.Put(syncable::IS_UNAPPLIED_UPDATE, false);
  entry.Put(syncable::SERVER_PARENT_ID, root_id);
  entry.Put(syncable::PARENT_ID, root_id);
  entry.Put(syncable::SERVER_IS_DIR, false);
  entry.Put(syncable::IS_DIR, false);
  entry.Put(syncable::SERVER_SPECIFICS, specifics);
  entry.Put(syncable::NON_UNIQUE_NAME, client_tag);
  entry.Put(syncable::UNIQUE_CLIENT_TAG, hashed_tag);
  entry.Put(syncable::IS_DEL, false);
  entry.Put(syncable::SPECIFICS, specifics);
  return entry.Get(syncable::META_HANDLE);
}

}  // namespace

class SyncApiTest : public testing::Test {
 public:
  virtual void SetUp() {
    test_user_share_.SetUp();
  }

  virtual void TearDown() {
    test_user_share_.TearDown();
  }

 protected:
  MessageLoop message_loop_;
  TestUserShare test_user_share_;
};

TEST_F(SyncApiTest, SanityCheckTest) {
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    EXPECT_TRUE(trans.GetWrappedTrans());
  }
  {
    WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
    EXPECT_TRUE(trans.GetWrappedTrans());
  }
  {
    // No entries but root should exist
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode node(&trans);
    // Metahandle 1 can be root, sanity check 2
    EXPECT_EQ(BaseNode::INIT_FAILED_ENTRY_NOT_GOOD, node.InitByIdLookup(2));
  }
}

TEST_F(SyncApiTest, BasicTagWrite) {
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_EQ(root_node.GetFirstChildId(), 0);
  }

  ignore_result(MakeNode(test_user_share_.user_share(),
                         BOOKMARKS, "testtag"));

  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, "testtag"));

    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_NE(node.GetId(), 0);
    EXPECT_EQ(node.GetId(), root_node.GetFirstChildId());
  }
}

TEST_F(SyncApiTest, ModelTypesSiloed) {
  {
    WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();
    EXPECT_EQ(root_node.GetFirstChildId(), 0);
  }

  ignore_result(MakeNode(test_user_share_.user_share(),
                         BOOKMARKS, "collideme"));
  ignore_result(MakeNode(test_user_share_.user_share(),
                         PREFERENCES, "collideme"));
  ignore_result(MakeNode(test_user_share_.user_share(),
                         AUTOFILL, "collideme"));

  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());

    ReadNode bookmarknode(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              bookmarknode.InitByClientTagLookup(BOOKMARKS,
                  "collideme"));

    ReadNode prefnode(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              prefnode.InitByClientTagLookup(PREFERENCES,
                  "collideme"));

    ReadNode autofillnode(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              autofillnode.InitByClientTagLookup(AUTOFILL,
                  "collideme"));

    EXPECT_NE(bookmarknode.GetId(), prefnode.GetId());
    EXPECT_NE(autofillnode.GetId(), prefnode.GetId());
    EXPECT_NE(bookmarknode.GetId(), autofillnode.GetId());
  }
}

TEST_F(SyncApiTest, ReadMissingTagsFails) {
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_FAILED_ENTRY_NOT_GOOD,
              node.InitByClientTagLookup(BOOKMARKS,
                  "testtag"));
  }
  {
    WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_FAILED_ENTRY_NOT_GOOD,
              node.InitByClientTagLookup(BOOKMARKS,
                  "testtag"));
  }
}

// TODO(chron): Hook this all up to the server and write full integration tests
//              for update->undelete behavior.
TEST_F(SyncApiTest, TestDeleteBehavior) {
  int64 node_id;
  int64 folder_id;
  std::string test_title("test1");

  {
    WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    // we'll use this spare folder later
    WriteNode folder_node(&trans);
    EXPECT_TRUE(folder_node.InitBookmarkByCreation(root_node, NULL));
    folder_id = folder_node.GetId();

    WriteNode wnode(&trans);
    WriteNode::InitUniqueByCreationResult result =
        wnode.InitUniqueByCreation(BOOKMARKS, root_node, "testtag");
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    wnode.SetIsFolder(false);
    wnode.SetTitle(UTF8ToWide(test_title));

    node_id = wnode.GetId();
  }

  // Ensure we can delete something with a tag.
  {
    WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
    WriteNode wnode(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              wnode.InitByClientTagLookup(BOOKMARKS,
                  "testtag"));
    EXPECT_FALSE(wnode.GetIsFolder());
    EXPECT_EQ(wnode.GetTitle(), test_title);

    wnode.Tombstone();
  }

  // Lookup of a node which was deleted should return failure,
  // but have found some data about the node.
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_FAILED_ENTRY_IS_DEL,
              node.InitByClientTagLookup(BOOKMARKS,
                  "testtag"));
    // Note that for proper function of this API this doesn't need to be
    // filled, we're checking just to make sure the DB worked in this test.
    EXPECT_EQ(node.GetTitle(), test_title);
  }

  {
    WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode folder_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, folder_node.InitByIdLookup(folder_id));

    WriteNode wnode(&trans);
    // This will undelete the tag.
    WriteNode::InitUniqueByCreationResult result =
        wnode.InitUniqueByCreation(BOOKMARKS, folder_node, "testtag");
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    EXPECT_EQ(wnode.GetIsFolder(), false);
    EXPECT_EQ(wnode.GetParentId(), folder_node.GetId());
    EXPECT_EQ(wnode.GetId(), node_id);
    EXPECT_NE(wnode.GetTitle(), test_title);  // Title should be cleared
    wnode.SetTitle(UTF8ToWide(test_title));
  }

  // Now look up should work.
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS,
                    "testtag"));
    EXPECT_EQ(node.GetTitle(), test_title);
    EXPECT_EQ(node.GetModelType(), BOOKMARKS);
  }
}

TEST_F(SyncApiTest, WriteAndReadPassword) {
  KeyParams params = {"localhost", "username", "passphrase"};
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    trans.GetCryptographer()->AddKey(params);
  }
  {
    WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode password_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        password_node.InitUniqueByCreation(PASSWORDS,
                                           root_node, "foo");
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    password_node.SetPasswordSpecifics(data);
  }
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              password_node.InitByClientTagLookup(PASSWORDS, "foo"));
    const sync_pb::PasswordSpecificsData& data =
        password_node.GetPasswordSpecifics();
    EXPECT_EQ("secret", data.password_value());
  }
}

TEST_F(SyncApiTest, WriteEncryptedTitle) {
  KeyParams params = {"localhost", "username", "passphrase"};
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    trans.GetCryptographer()->AddKey(params);
  }
  test_user_share_.encryption_handler()->EnableEncryptEverything();
  int bookmark_id;
  {
    WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode bookmark_node(&trans);
    ASSERT_TRUE(bookmark_node.InitBookmarkByCreation(root_node, NULL));
    bookmark_id = bookmark_node.GetId();
    bookmark_node.SetTitle(UTF8ToWide("foo"));

    WriteNode pref_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        pref_node.InitUniqueByCreation(PREFERENCES, root_node, "bar");
    ASSERT_EQ(WriteNode::INIT_SUCCESS, result);
    pref_node.SetTitle(UTF8ToWide("bar"));
  }
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    ReadNode bookmark_node(&trans);
    ASSERT_EQ(BaseNode::INIT_OK, bookmark_node.InitByIdLookup(bookmark_id));
    EXPECT_EQ("foo", bookmark_node.GetTitle());
    EXPECT_EQ(kEncryptedString,
              bookmark_node.GetEntry()->Get(syncable::NON_UNIQUE_NAME));

    ReadNode pref_node(&trans);
    ASSERT_EQ(BaseNode::INIT_OK,
              pref_node.InitByClientTagLookup(PREFERENCES,
                                              "bar"));
    EXPECT_EQ(kEncryptedString, pref_node.GetTitle());
  }
}

TEST_F(SyncApiTest, BaseNodeSetSpecifics) {
  int64 child_id = MakeNode(test_user_share_.user_share(),
                            BOOKMARKS, "testtag");
  WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
  WriteNode node(&trans);
  EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(child_id));

  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("http://www.google.com");

  EXPECT_NE(entity_specifics.SerializeAsString(),
            node.GetEntitySpecifics().SerializeAsString());
  node.SetEntitySpecifics(entity_specifics);
  EXPECT_EQ(entity_specifics.SerializeAsString(),
            node.GetEntitySpecifics().SerializeAsString());
}

TEST_F(SyncApiTest, BaseNodeSetSpecificsPreservesUnknownFields) {
  int64 child_id = MakeNode(test_user_share_.user_share(),
                            BOOKMARKS, "testtag");
  WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
  WriteNode node(&trans);
  EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(child_id));
  EXPECT_TRUE(node.GetEntitySpecifics().unknown_fields().empty());

  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("http://www.google.com");
  entity_specifics.mutable_unknown_fields()->AddFixed32(5, 100);
  node.SetEntitySpecifics(entity_specifics);
  EXPECT_FALSE(node.GetEntitySpecifics().unknown_fields().empty());

  entity_specifics.mutable_unknown_fields()->Clear();
  node.SetEntitySpecifics(entity_specifics);
  EXPECT_FALSE(node.GetEntitySpecifics().unknown_fields().empty());
}

namespace {

void CheckNodeValue(const BaseNode& node, const base::DictionaryValue& value,
                    bool is_detailed) {
  size_t expected_field_count = 4;

  ExpectInt64Value(node.GetId(), value, "id");
  {
    bool is_folder = false;
    EXPECT_TRUE(value.GetBoolean("isFolder", &is_folder));
    EXPECT_EQ(node.GetIsFolder(), is_folder);
  }
  ExpectDictStringValue(node.GetTitle(), value, "title");

  ModelType expected_model_type = node.GetModelType();
  std::string type_str;
  EXPECT_TRUE(value.GetString("type", &type_str));
  if (expected_model_type >= FIRST_REAL_MODEL_TYPE) {
    ModelType model_type = ModelTypeFromString(type_str);
    EXPECT_EQ(expected_model_type, model_type);
  } else if (expected_model_type == TOP_LEVEL_FOLDER) {
    EXPECT_EQ("Top-level folder", type_str);
  } else if (expected_model_type == UNSPECIFIED) {
    EXPECT_EQ("Unspecified", type_str);
  } else {
    ADD_FAILURE();
  }

  if (is_detailed) {
    {
      scoped_ptr<base::DictionaryValue> expected_entry(
          node.GetEntry()->ToValue(NULL));
      const base::Value* entry = NULL;
      EXPECT_TRUE(value.Get("entry", &entry));
      EXPECT_TRUE(base::Value::Equals(entry, expected_entry.get()));
    }

    ExpectInt64Value(node.GetParentId(), value, "parentId");
    ExpectTimeValue(node.GetModificationTime(), value, "modificationTime");
    ExpectInt64Value(node.GetExternalId(), value, "externalId");
    expected_field_count += 4;

    if (value.HasKey("predecessorId")) {
      ExpectInt64Value(node.GetPredecessorId(), value, "predecessorId");
      expected_field_count++;
    }
    if (value.HasKey("successorId")) {
      ExpectInt64Value(node.GetSuccessorId(), value, "successorId");
      expected_field_count++;
    }
    if (value.HasKey("firstChildId")) {
      ExpectInt64Value(node.GetFirstChildId(), value, "firstChildId");
      expected_field_count++;
    }
  }

  EXPECT_EQ(expected_field_count, value.size());
}

}  // namespace

TEST_F(SyncApiTest, BaseNodeGetSummaryAsValue) {
  ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
  ReadNode node(&trans);
  node.InitByRootLookup();
  scoped_ptr<base::DictionaryValue> details(node.GetSummaryAsValue());
  if (details.get()) {
    CheckNodeValue(node, *details, false);
  } else {
    ADD_FAILURE();
  }
}

TEST_F(SyncApiTest, BaseNodeGetDetailsAsValue) {
  ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
  ReadNode node(&trans);
  node.InitByRootLookup();
  scoped_ptr<base::DictionaryValue> details(node.GetDetailsAsValue());
  if (details.get()) {
    CheckNodeValue(node, *details, true);
  } else {
    ADD_FAILURE();
  }
}

TEST_F(SyncApiTest, EmptyTags) {
  WriteTransaction trans(FROM_HERE, test_user_share_.user_share());
  ReadNode root_node(&trans);
  root_node.InitByRootLookup();
  WriteNode node(&trans);
  std::string empty_tag;
  WriteNode::InitUniqueByCreationResult result =
      node.InitUniqueByCreation(TYPED_URLS, root_node, empty_tag);
  EXPECT_NE(WriteNode::INIT_SUCCESS, result);
  EXPECT_EQ(BaseNode::INIT_FAILED_PRECONDITION,
            node.InitByTagLookup(empty_tag));
}

// Test counting nodes when the type's root node has no children.
TEST_F(SyncApiTest, GetTotalNodeCountEmpty) {
  int64 type_root = MakeServerNodeForType(test_user_share_.user_share(),
                                          BOOKMARKS);
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode type_root_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              type_root_node.InitByIdLookup(type_root));
    EXPECT_EQ(1, type_root_node.GetTotalNodeCount());
  }
}

// Test counting nodes when there is one child beneath the type's root.
TEST_F(SyncApiTest, GetTotalNodeCountOneChild) {
  int64 type_root = MakeServerNodeForType(test_user_share_.user_share(),
                                          BOOKMARKS);
  int64 parent = MakeFolderWithParent(test_user_share_.user_share(),
                                      BOOKMARKS,
                                      type_root,
                                      NULL);
  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode type_root_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              type_root_node.InitByIdLookup(type_root));
    EXPECT_EQ(2, type_root_node.GetTotalNodeCount());
    ReadNode parent_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              parent_node.InitByIdLookup(parent));
    EXPECT_EQ(1, parent_node.GetTotalNodeCount());
  }
}

// Test counting nodes when there are multiple children beneath the type root,
// and one of those children has children of its own.
TEST_F(SyncApiTest, GetTotalNodeCountMultipleChildren) {
  int64 type_root = MakeServerNodeForType(test_user_share_.user_share(),
                                          BOOKMARKS);
  int64 parent = MakeFolderWithParent(test_user_share_.user_share(),
                                      BOOKMARKS,
                                      type_root,
                                      NULL);
  ignore_result(MakeFolderWithParent(test_user_share_.user_share(),
                                     BOOKMARKS,
                                     type_root,
                                     NULL));
  int64 child1 = MakeFolderWithParent(
      test_user_share_.user_share(),
      BOOKMARKS,
      parent,
      NULL);
  ignore_result(MakeBookmarkWithParent(
      test_user_share_.user_share(),
      parent,
      NULL));
  ignore_result(MakeBookmarkWithParent(
      test_user_share_.user_share(),
      child1,
      NULL));

  {
    ReadTransaction trans(FROM_HERE, test_user_share_.user_share());
    ReadNode type_root_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              type_root_node.InitByIdLookup(type_root));
    EXPECT_EQ(6, type_root_node.GetTotalNodeCount());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByIdLookup(parent));
    EXPECT_EQ(4, node.GetTotalNodeCount());
  }
}

namespace {

class TestHttpPostProviderInterface : public HttpPostProviderInterface {
 public:
  virtual ~TestHttpPostProviderInterface() {}

  virtual void SetExtraRequestHeaders(const char* headers) OVERRIDE {}
  virtual void SetURL(const char* url, int port) OVERRIDE {}
  virtual void SetPostPayload(const char* content_type,
                              int content_length,
                              const char* content) OVERRIDE {}
  virtual bool MakeSynchronousPost(int* error_code, int* response_code)
      OVERRIDE {
    return false;
  }
  virtual int GetResponseContentLength() const OVERRIDE {
    return 0;
  }
  virtual const char* GetResponseContent() const OVERRIDE {
    return "";
  }
  virtual const std::string GetResponseHeaderValue(
      const std::string& name) const OVERRIDE {
    return "";
  }
  virtual void Abort() OVERRIDE {}
};

class TestHttpPostProviderFactory : public HttpPostProviderFactory {
 public:
  virtual ~TestHttpPostProviderFactory() {}
  virtual HttpPostProviderInterface* Create() OVERRIDE {
    return new TestHttpPostProviderInterface();
  }
  virtual void Destroy(HttpPostProviderInterface* http) OVERRIDE {
    delete static_cast<TestHttpPostProviderInterface*>(http);
  }
};

class SyncManagerObserverMock : public SyncManager::Observer {
 public:
  MOCK_METHOD1(OnSyncCycleCompleted,
               void(const SyncSessionSnapshot&));  // NOLINT
  MOCK_METHOD4(OnInitializationComplete,
               void(const WeakHandle<JsBackend>&,
                    const WeakHandle<DataTypeDebugInfoListener>&,
                    bool,
                    syncer::ModelTypeSet));  // NOLINT
  MOCK_METHOD1(OnConnectionStatusChange, void(ConnectionStatus));  // NOLINT
  MOCK_METHOD0(OnStopSyncingPermanently, void());  // NOLINT
  MOCK_METHOD1(OnUpdatedToken, void(const std::string&));  // NOLINT
  MOCK_METHOD1(OnActionableError,
               void(const SyncProtocolError&));  // NOLINT
};

class SyncEncryptionHandlerObserverMock
    : public SyncEncryptionHandler::Observer {
 public:
  MOCK_METHOD2(OnPassphraseRequired,
               void(PassphraseRequiredReason,
                    const sync_pb::EncryptedData&));  // NOLINT
  MOCK_METHOD0(OnPassphraseAccepted, void());  // NOLINT
  MOCK_METHOD2(OnBootstrapTokenUpdated,
               void(const std::string&, BootstrapTokenType type));  // NOLINT
  MOCK_METHOD2(OnEncryptedTypesChanged,
               void(ModelTypeSet, bool));  // NOLINT
  MOCK_METHOD0(OnEncryptionComplete, void());  // NOLINT
  MOCK_METHOD1(OnCryptographerStateChanged, void(Cryptographer*));  // NOLINT
  MOCK_METHOD2(OnPassphraseTypeChanged, void(PassphraseType,
                                             base::Time));  // NOLINT
};

}  // namespace

class SyncManagerTest : public testing::Test,
                        public SyncManager::ChangeDelegate {
 protected:
  enum NigoriStatus {
    DONT_WRITE_NIGORI,
    WRITE_TO_NIGORI
  };

  enum EncryptionStatus {
    UNINITIALIZED,
    DEFAULT_ENCRYPTION,
    FULL_ENCRYPTION
  };

  SyncManagerTest()
      : fake_invalidator_(NULL),
        sync_manager_("Test sync manager") {
    switches_.encryption_method =
        InternalComponentsFactory::ENCRYPTION_KEYSTORE;
  }

  virtual ~SyncManagerTest() {
    EXPECT_FALSE(fake_invalidator_);
  }

  // Test implementation.
  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    SyncCredentials credentials;
    credentials.email = "foo@bar.com";
    credentials.sync_token = "sometoken";

    fake_invalidator_ = new FakeInvalidator();

    sync_manager_.AddObserver(&manager_observer_);
    EXPECT_CALL(manager_observer_, OnInitializationComplete(_, _, _, _)).
        WillOnce(SaveArg<0>(&js_backend_));

    EXPECT_FALSE(js_backend_.IsInitialized());

    std::vector<ModelSafeWorker*> workers;
    ModelSafeRoutingInfo routing_info;
    GetModelSafeRoutingInfo(&routing_info);

    // Takes ownership of |fake_invalidator_|.
    sync_manager_.Init(temp_dir_.path(),
                       WeakHandle<JsEventHandler>(),
                       "bogus", 0, false,
                       scoped_ptr<HttpPostProviderFactory>(
                           new TestHttpPostProviderFactory()),
                       workers, &extensions_activity_monitor_, this,
                       credentials,
                       scoped_ptr<Invalidator>(fake_invalidator_),
                       "fake_invalidator_client_id",
                       "", "",  // bootstrap tokens
                       scoped_ptr<InternalComponentsFactory>(GetFactory()),
                       &encryptor_,
                       &handler_,
                       NULL);

    sync_manager_.GetEncryptionHandler()->AddObserver(&encryption_observer_);

    EXPECT_TRUE(js_backend_.IsInitialized());

    for (ModelSafeRoutingInfo::iterator i = routing_info.begin();
         i != routing_info.end(); ++i) {
      type_roots_[i->first] = MakeServerNodeForType(
          sync_manager_.GetUserShare(), i->first);
    }
    PumpLoop();

    EXPECT_TRUE(fake_invalidator_->IsHandlerRegistered(&sync_manager_));
  }

  void TearDown() {
    sync_manager_.RemoveObserver(&manager_observer_);
    sync_manager_.ShutdownOnSyncThread();
    // We can't assert that |sync_manager_| isn't registered with
    // |fake_invalidator_| anymore because |fake_invalidator_| is now
    // destroyed.
    fake_invalidator_ = NULL;
    PumpLoop();
  }

  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
    (*out)[NIGORI] = GROUP_PASSIVE;
    (*out)[DEVICE_INFO] = GROUP_PASSIVE;
    (*out)[EXPERIMENTS] = GROUP_PASSIVE;
    (*out)[BOOKMARKS] = GROUP_PASSIVE;
    (*out)[THEMES] = GROUP_PASSIVE;
    (*out)[SESSIONS] = GROUP_PASSIVE;
    (*out)[PASSWORDS] = GROUP_PASSIVE;
    (*out)[PREFERENCES] = GROUP_PASSIVE;
    (*out)[PRIORITY_PREFERENCES] = GROUP_PASSIVE;
  }

  virtual void OnChangesApplied(
      ModelType model_type,
      int64 model_version,
      const BaseTransaction* trans,
      const ImmutableChangeRecordList& changes) OVERRIDE {}

  virtual void OnChangesComplete(ModelType model_type) OVERRIDE {}

  // Helper methods.
  bool SetUpEncryption(NigoriStatus nigori_status,
                       EncryptionStatus encryption_status) {
    UserShare* share = sync_manager_.GetUserShare();

    // We need to create the nigori node as if it were an applied server update.
    int64 nigori_id = GetIdForDataType(NIGORI);
    if (nigori_id == kInvalidId)
      return false;

    // Set the nigori cryptographer information.
    if (encryption_status == FULL_ENCRYPTION)
      sync_manager_.GetEncryptionHandler()->EnableEncryptEverything();

    WriteTransaction trans(FROM_HERE, share);
    Cryptographer* cryptographer = trans.GetCryptographer();
    if (!cryptographer)
      return false;
    if (encryption_status != UNINITIALIZED) {
      KeyParams params = {"localhost", "dummy", "foobar"};
      cryptographer->AddKey(params);
    } else {
      DCHECK_NE(nigori_status, WRITE_TO_NIGORI);
    }
    if (nigori_status == WRITE_TO_NIGORI) {
      sync_pb::NigoriSpecifics nigori;
      cryptographer->GetKeys(nigori.mutable_encryption_keybag());
      share->directory->GetNigoriHandler()->UpdateNigoriFromEncryptedTypes(
          &nigori,
          trans.GetWrappedTrans());
      WriteNode node(&trans);
      EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(nigori_id));
      node.SetNigoriSpecifics(nigori);
    }
    return cryptographer->is_ready();
  }

  int64 GetIdForDataType(ModelType type) {
    if (type_roots_.count(type) == 0)
      return 0;
    return type_roots_[type];
  }

  void PumpLoop() {
    message_loop_.RunUntilIdle();
  }

  void SendJsMessage(const std::string& name, const JsArgList& args,
                     const WeakHandle<JsReplyHandler>& reply_handler) {
    js_backend_.Call(FROM_HERE, &JsBackend::ProcessJsMessage,
                     name, args, reply_handler);
    PumpLoop();
  }

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler) {
    js_backend_.Call(FROM_HERE, &JsBackend::SetJsEventHandler,
                     event_handler);
    PumpLoop();
  }

  // Looks up an entry by client tag and resets IS_UNSYNCED value to false.
  // Returns true if entry was previously unsynced, false if IS_UNSYNCED was
  // already false.
  bool ResetUnsyncedEntry(ModelType type,
                          const std::string& client_tag) {
    UserShare* share = sync_manager_.GetUserShare();
    syncable::WriteTransaction trans(
        FROM_HERE, syncable::UNITTEST, share->directory.get());
    const std::string hash = syncable::GenerateSyncableHash(type, client_tag);
    syncable::MutableEntry entry(&trans, syncable::GET_BY_CLIENT_TAG,
                                 hash);
    EXPECT_TRUE(entry.good());
    if (!entry.Get(IS_UNSYNCED))
      return false;
    entry.Put(IS_UNSYNCED, false);
    return true;
  }

  virtual InternalComponentsFactory* GetFactory() {
    return new TestInternalComponentsFactory(GetSwitches(), STORAGE_IN_MEMORY);
  }

  // Returns true if we are currently encrypting all sync data.  May
  // be called on any thread.
  bool EncryptEverythingEnabledForTest() {
    return sync_manager_.GetEncryptionHandler()->EncryptEverythingEnabled();
  }

  // Gets the set of encrypted types from the cryptographer
  // Note: opens a transaction.  May be called from any thread.
  ModelTypeSet GetEncryptedTypes() {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    return GetEncryptedTypesWithTrans(&trans);
  }

  ModelTypeSet GetEncryptedTypesWithTrans(BaseTransaction* trans) {
    return trans->GetDirectory()->GetNigoriHandler()->
        GetEncryptedTypes(trans->GetWrappedTrans());
  }

  void SimulateInvalidatorStateChangeForTest(InvalidatorState state) {
    DCHECK(sync_manager_.thread_checker_.CalledOnValidThread());
    sync_manager_.OnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingNotificationForTest(ModelTypeSet model_types) {
    DCHECK(sync_manager_.thread_checker_.CalledOnValidThread());
    ModelTypeInvalidationMap invalidation_map =
        ModelTypeSetToInvalidationMap(model_types, std::string());
    sync_manager_.OnIncomingInvalidation(
        ModelTypeInvalidationMapToObjectIdInvalidationMap(
            invalidation_map));
  }

  void SetProgressMarkerForType(ModelType type, bool set) {
    if (set) {
      sync_pb::DataTypeProgressMarker marker;
      marker.set_token("token");
      marker.set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
      sync_manager_.directory()->SetDownloadProgress(type, marker);
    } else {
      sync_pb::DataTypeProgressMarker marker;
      sync_manager_.directory()->SetDownloadProgress(type, marker);
    }
  }

  InternalComponentsFactory::Switches GetSwitches() const {
    return switches_;
  }

 private:
  // Needed by |sync_manager_|.
  MessageLoop message_loop_;
  // Needed by |sync_manager_|.
  base::ScopedTempDir temp_dir_;
  // Sync Id's for the roots of the enabled datatypes.
  std::map<ModelType, int64> type_roots_;
  FakeExtensionsActivityMonitor extensions_activity_monitor_;

 protected:
  FakeEncryptor encryptor_;
  TestUnrecoverableErrorHandler handler_;
  FakeInvalidator* fake_invalidator_;
  SyncManagerImpl sync_manager_;
  WeakHandle<JsBackend> js_backend_;
  StrictMock<SyncManagerObserverMock> manager_observer_;
  StrictMock<SyncEncryptionHandlerObserverMock> encryption_observer_;
  InternalComponentsFactory::Switches switches_;
};

TEST_F(SyncManagerTest, UpdateEnabledTypes) {
  ModelSafeRoutingInfo routes;
  GetModelSafeRoutingInfo(&routes);
  const ModelTypeSet enabled_types = GetRoutingInfoTypes(routes);
  sync_manager_.UpdateEnabledTypes(enabled_types);
  EXPECT_EQ(ModelTypeSetToObjectIdSet(enabled_types),
            fake_invalidator_->GetRegisteredIds(&sync_manager_));
}

TEST_F(SyncManagerTest, RegisterInvalidationHandler) {
  FakeInvalidationHandler fake_handler;
  sync_manager_.RegisterInvalidationHandler(&fake_handler);
  EXPECT_TRUE(fake_invalidator_->IsHandlerRegistered(&fake_handler));

  const ObjectIdSet& ids =
      ModelTypeSetToObjectIdSet(ModelTypeSet(BOOKMARKS, PREFERENCES));
  sync_manager_.UpdateRegisteredInvalidationIds(&fake_handler, ids);
  EXPECT_EQ(ids, fake_invalidator_->GetRegisteredIds(&fake_handler));

  sync_manager_.UnregisterInvalidationHandler(&fake_handler);
  EXPECT_FALSE(fake_invalidator_->IsHandlerRegistered(&fake_handler));
}

TEST_F(SyncManagerTest, ProcessJsMessage) {
  const JsArgList kNoArgs;

  StrictMock<MockJsReplyHandler> reply_handler;

  base::ListValue disabled_args;
  disabled_args.Append(new base::StringValue("TRANSIENT_INVALIDATION_ERROR"));

  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState",
                            HasArgsAsList(disabled_args)));

  // This message should be dropped.
  SendJsMessage("unknownMessage", kNoArgs, reply_handler.AsWeakHandle());

  SendJsMessage("getNotificationState", kNoArgs, reply_handler.AsWeakHandle());
}

TEST_F(SyncManagerTest, ProcessJsMessageGetRootNodeDetails) {
  const JsArgList kNoArgs;

  StrictMock<MockJsReplyHandler> reply_handler;

  JsArgList return_args;

  EXPECT_CALL(reply_handler,
              HandleJsReply("getRootNodeDetails", _))
      .WillOnce(SaveArg<1>(&return_args));

  SendJsMessage("getRootNodeDetails", kNoArgs, reply_handler.AsWeakHandle());

  EXPECT_EQ(1u, return_args.Get().GetSize());
  const base::DictionaryValue* node_info = NULL;
  EXPECT_TRUE(return_args.Get().GetDictionary(0, &node_info));
  if (node_info) {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    node.InitByRootLookup();
    CheckNodeValue(node, *node_info, true);
  } else {
    ADD_FAILURE();
  }
}

void CheckGetNodesByIdReturnArgs(SyncManager* sync_manager,
                                 const JsArgList& return_args,
                                 int64 id,
                                 bool is_detailed) {
  EXPECT_EQ(1u, return_args.Get().GetSize());
  const base::ListValue* nodes = NULL;
  ASSERT_TRUE(return_args.Get().GetList(0, &nodes));
  ASSERT_TRUE(nodes);
  EXPECT_EQ(1u, nodes->GetSize());
  const base::DictionaryValue* node_info = NULL;
  EXPECT_TRUE(nodes->GetDictionary(0, &node_info));
  ASSERT_TRUE(node_info);
  ReadTransaction trans(FROM_HERE, sync_manager->GetUserShare());
  ReadNode node(&trans);
  EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(id));
  CheckNodeValue(node, *node_info, is_detailed);
}

class SyncManagerGetNodesByIdTest : public SyncManagerTest {
 protected:
  virtual ~SyncManagerGetNodesByIdTest() {}

  void RunGetNodesByIdTest(const char* message_name, bool is_detailed) {
    int64 root_id = kInvalidId;
    {
      ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
      ReadNode root_node(&trans);
      root_node.InitByRootLookup();
      root_id = root_node.GetId();
    }

    int64 child_id =
        MakeNode(sync_manager_.GetUserShare(), BOOKMARKS, "testtag");

    StrictMock<MockJsReplyHandler> reply_handler;

    JsArgList return_args;

    const int64 ids[] = { root_id, child_id };

    EXPECT_CALL(reply_handler,
                HandleJsReply(message_name, _))
        .Times(arraysize(ids)).WillRepeatedly(SaveArg<1>(&return_args));

    for (size_t i = 0; i < arraysize(ids); ++i) {
      base::ListValue args;
      base::ListValue* id_values = new base::ListValue();
      args.Append(id_values);
      id_values->Append(new base::StringValue(base::Int64ToString(ids[i])));
      SendJsMessage(message_name,
                    JsArgList(&args), reply_handler.AsWeakHandle());

      CheckGetNodesByIdReturnArgs(&sync_manager_, return_args,
                                  ids[i], is_detailed);
    }
  }

  void RunGetNodesByIdFailureTest(const char* message_name) {
    StrictMock<MockJsReplyHandler> reply_handler;

    base::ListValue empty_list_args;
    empty_list_args.Append(new base::ListValue());

    EXPECT_CALL(reply_handler,
                HandleJsReply(message_name,
                                    HasArgsAsList(empty_list_args)))
        .Times(6);

    {
      base::ListValue args;
      SendJsMessage(message_name,
                    JsArgList(&args), reply_handler.AsWeakHandle());
    }

    {
      base::ListValue args;
      args.Append(new base::ListValue());
      SendJsMessage(message_name,
                    JsArgList(&args), reply_handler.AsWeakHandle());
    }

    {
      base::ListValue args;
      base::ListValue* ids = new base::ListValue();
      args.Append(ids);
      ids->Append(new base::StringValue(""));
      SendJsMessage(message_name,
                    JsArgList(&args), reply_handler.AsWeakHandle());
    }

    {
      base::ListValue args;
      base::ListValue* ids = new base::ListValue();
      args.Append(ids);
      ids->Append(new base::StringValue("nonsense"));
      SendJsMessage(message_name,
                    JsArgList(&args), reply_handler.AsWeakHandle());
    }

    {
      base::ListValue args;
      base::ListValue* ids = new base::ListValue();
      args.Append(ids);
      ids->Append(new base::StringValue("0"));
      SendJsMessage(message_name,
                    JsArgList(&args), reply_handler.AsWeakHandle());
    }

    {
      base::ListValue args;
      base::ListValue* ids = new base::ListValue();
      args.Append(ids);
      ids->Append(new base::StringValue("9999"));
      SendJsMessage(message_name,
                    JsArgList(&args), reply_handler.AsWeakHandle());
    }
  }
};

TEST_F(SyncManagerGetNodesByIdTest, GetNodeSummariesById) {
  RunGetNodesByIdTest("getNodeSummariesById", false);
}

TEST_F(SyncManagerGetNodesByIdTest, GetNodeDetailsById) {
  RunGetNodesByIdTest("getNodeDetailsById", true);
}

TEST_F(SyncManagerGetNodesByIdTest, GetNodeSummariesByIdFailure) {
  RunGetNodesByIdFailureTest("getNodeSummariesById");
}

TEST_F(SyncManagerGetNodesByIdTest, GetNodeDetailsByIdFailure) {
  RunGetNodesByIdFailureTest("getNodeDetailsById");
}

TEST_F(SyncManagerTest, GetChildNodeIds) {
  StrictMock<MockJsReplyHandler> reply_handler;

  JsArgList return_args;

  EXPECT_CALL(reply_handler,
              HandleJsReply("getChildNodeIds", _))
      .Times(1).WillRepeatedly(SaveArg<1>(&return_args));

  {
    base::ListValue args;
    args.Append(new base::StringValue("1"));
    SendJsMessage("getChildNodeIds",
                  JsArgList(&args), reply_handler.AsWeakHandle());
  }

  EXPECT_EQ(1u, return_args.Get().GetSize());
  const base::ListValue* nodes = NULL;
  ASSERT_TRUE(return_args.Get().GetList(0, &nodes));
  ASSERT_TRUE(nodes);
  EXPECT_EQ(9u, nodes->GetSize());
}

TEST_F(SyncManagerTest, GetChildNodeIdsFailure) {
  StrictMock<MockJsReplyHandler> reply_handler;

  base::ListValue empty_list_args;
  empty_list_args.Append(new base::ListValue());

  EXPECT_CALL(reply_handler,
              HandleJsReply("getChildNodeIds",
                                   HasArgsAsList(empty_list_args)))
      .Times(5);

  {
    base::ListValue args;
    SendJsMessage("getChildNodeIds",
                   JsArgList(&args), reply_handler.AsWeakHandle());
  }

  {
    base::ListValue args;
    args.Append(new base::StringValue(""));
    SendJsMessage("getChildNodeIds",
                  JsArgList(&args), reply_handler.AsWeakHandle());
  }

  {
    base::ListValue args;
    args.Append(new base::StringValue("nonsense"));
    SendJsMessage("getChildNodeIds",
                  JsArgList(&args), reply_handler.AsWeakHandle());
  }

  {
    base::ListValue args;
    args.Append(new base::StringValue("0"));
    SendJsMessage("getChildNodeIds",
                  JsArgList(&args), reply_handler.AsWeakHandle());
  }

  {
    base::ListValue args;
    args.Append(new base::StringValue("9999"));
    SendJsMessage("getChildNodeIds",
                  JsArgList(&args), reply_handler.AsWeakHandle());
  }
}

TEST_F(SyncManagerTest, GetAllNodesTest) {
  StrictMock<MockJsReplyHandler> reply_handler;
  JsArgList return_args;

  EXPECT_CALL(reply_handler,
              HandleJsReply("getAllNodes", _))
      .Times(1).WillRepeatedly(SaveArg<1>(&return_args));

  {
    base::ListValue args;
    SendJsMessage("getAllNodes",
                  JsArgList(&args), reply_handler.AsWeakHandle());
  }

  // There's not much value in verifying every attribute on every node here.
  // Most of the value of this test has already been achieved: we've verified we
  // can call the above function without crashing or leaking memory.
  //
  // Let's just check the list size and a few of its elements.  Anything more
  // would make this test brittle without greatly increasing our chances of
  // catching real bugs.

  const base::ListValue* node_list;
  const base::DictionaryValue* first_result;

  // The resulting argument list should have one argument, a list of nodes.
  ASSERT_EQ(1U, return_args.Get().GetSize());
  ASSERT_TRUE(return_args.Get().GetList(0, &node_list));

  // The database creation logic depends on the routing info.
  // Refer to setup methods for more information.
  ModelSafeRoutingInfo routes;
  GetModelSafeRoutingInfo(&routes);
  size_t directory_size = routes.size() + 1;

  ASSERT_EQ(directory_size, node_list->GetSize());
  ASSERT_TRUE(node_list->GetDictionary(0, &first_result));
  EXPECT_TRUE(first_result->HasKey("ID"));
  EXPECT_TRUE(first_result->HasKey("NON_UNIQUE_NAME"));
}

// Simulate various invalidator state changes.  Those should propagate
// JS events.
TEST_F(SyncManagerTest, OnInvalidatorStateChangeJsEvents) {
  StrictMock<MockJsEventHandler> event_handler;

  base::DictionaryValue enabled_details;
  enabled_details.SetString("state", "INVALIDATIONS_ENABLED");
  base::DictionaryValue credentials_rejected_details;
  credentials_rejected_details.SetString(
      "state", "INVALIDATION_CREDENTIALS_REJECTED");
  base::DictionaryValue transient_error_details;
  transient_error_details.SetString("state", "TRANSIENT_INVALIDATION_ERROR");
  base::DictionaryValue auth_error_details;
  auth_error_details.SetString("status", "CONNECTION_AUTH_ERROR");

  EXPECT_CALL(manager_observer_,
              OnConnectionStatusChange(CONNECTION_AUTH_ERROR));

  EXPECT_CALL(
      event_handler,
      HandleJsEvent("onConnectionStatusChange",
                    HasDetailsAsDictionary(auth_error_details)));

  EXPECT_CALL(event_handler,
              HandleJsEvent("onNotificationStateChange",
                            HasDetailsAsDictionary(enabled_details)));

  EXPECT_CALL(
      event_handler,
      HandleJsEvent("onNotificationStateChange",
                    HasDetailsAsDictionary(credentials_rejected_details)))
      .Times(2);

  EXPECT_CALL(event_handler,
              HandleJsEvent("onNotificationStateChange",
                            HasDetailsAsDictionary(transient_error_details)));

  // Test needs to simulate INVALIDATION_CREDENTIALS_REJECTED with event handler
  // attached because this is the only time when CONNECTION_AUTH_ERROR
  // notification will be generated, therefore the only chance to verify that
  // "onConnectionStatusChange" event is delivered
  SetJsEventHandler(event_handler.AsWeakHandle());
  SimulateInvalidatorStateChangeForTest(INVALIDATION_CREDENTIALS_REJECTED);
  SetJsEventHandler(WeakHandle<JsEventHandler>());

  SimulateInvalidatorStateChangeForTest(INVALIDATIONS_ENABLED);
  SimulateInvalidatorStateChangeForTest(INVALIDATION_CREDENTIALS_REJECTED);
  SimulateInvalidatorStateChangeForTest(TRANSIENT_INVALIDATION_ERROR);

  SetJsEventHandler(event_handler.AsWeakHandle());
  SimulateInvalidatorStateChangeForTest(INVALIDATIONS_ENABLED);
  SimulateInvalidatorStateChangeForTest(INVALIDATION_CREDENTIALS_REJECTED);
  SimulateInvalidatorStateChangeForTest(TRANSIENT_INVALIDATION_ERROR);
  SetJsEventHandler(WeakHandle<JsEventHandler>());

  SimulateInvalidatorStateChangeForTest(INVALIDATIONS_ENABLED);
  SimulateInvalidatorStateChangeForTest(INVALIDATION_CREDENTIALS_REJECTED);
  SimulateInvalidatorStateChangeForTest(TRANSIENT_INVALIDATION_ERROR);

  // Should trigger the replies.
  PumpLoop();
}

// Simulate the invalidator's credentials being rejected.  That should
// also clear the sync token.
TEST_F(SyncManagerTest, OnInvalidatorStateChangeCredentialsRejected) {
  EXPECT_CALL(manager_observer_,
              OnConnectionStatusChange(CONNECTION_AUTH_ERROR));

  EXPECT_FALSE(sync_manager_.GetHasInvalidAuthTokenForTest());

  SimulateInvalidatorStateChangeForTest(INVALIDATION_CREDENTIALS_REJECTED);

  EXPECT_TRUE(sync_manager_.GetHasInvalidAuthTokenForTest());

  // Should trigger the replies.
  PumpLoop();
}

TEST_F(SyncManagerTest, OnIncomingNotification) {
  StrictMock<MockJsEventHandler> event_handler;

  const ModelTypeSet empty_model_types;
  const ModelTypeSet model_types(
      BOOKMARKS, THEMES);

  // Build expected_args to have a single argument with the string
  // equivalents of model_types.
  base::DictionaryValue expected_details;
  {
    base::ListValue* model_type_list = new base::ListValue();
    expected_details.SetString("source", "REMOTE_INVALIDATION");
    expected_details.Set("changedTypes", model_type_list);
    for (ModelTypeSet::Iterator it = model_types.First();
         it.Good(); it.Inc()) {
      model_type_list->Append(
          new base::StringValue(ModelTypeToString(it.Get())));
    }
  }

  EXPECT_CALL(event_handler,
              HandleJsEvent("onIncomingNotification",
                            HasDetailsAsDictionary(expected_details)));

  TriggerOnIncomingNotificationForTest(empty_model_types);
  TriggerOnIncomingNotificationForTest(model_types);

  SetJsEventHandler(event_handler.AsWeakHandle());
  TriggerOnIncomingNotificationForTest(model_types);
  SetJsEventHandler(WeakHandle<JsEventHandler>());

  TriggerOnIncomingNotificationForTest(empty_model_types);
  TriggerOnIncomingNotificationForTest(model_types);

  // Should trigger the replies.
  PumpLoop();
}

TEST_F(SyncManagerTest, RefreshEncryptionReady) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));

  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();

  const ModelTypeSet encrypted_types = GetEncryptedTypes();
  EXPECT_TRUE(encrypted_types.Has(PASSWORDS));
  EXPECT_FALSE(EncryptEverythingEnabledForTest());

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByIdLookup(GetIdForDataType(NIGORI)));
    sync_pb::NigoriSpecifics nigori = node.GetNigoriSpecifics();
    EXPECT_TRUE(nigori.has_encryption_keybag());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    EXPECT_TRUE(cryptographer->CanDecrypt(nigori.encryption_keybag()));
  }
}

// Attempt to refresh encryption when nigori not downloaded.
TEST_F(SyncManagerTest, RefreshEncryptionNotReady) {
  // Don't set up encryption (no nigori node created).

  // Should fail. Triggers an OnPassphraseRequired because the cryptographer
  // is not ready.
  EXPECT_CALL(encryption_observer_, OnPassphraseRequired(_, _)).Times(1);
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();

  const ModelTypeSet encrypted_types = GetEncryptedTypes();
  EXPECT_TRUE(encrypted_types.Has(PASSWORDS));  // Hardcoded.
  EXPECT_FALSE(EncryptEverythingEnabledForTest());
}

// Attempt to refresh encryption when nigori is empty.
TEST_F(SyncManagerTest, RefreshEncryptionEmptyNigori) {
  EXPECT_TRUE(SetUpEncryption(DONT_WRITE_NIGORI, DEFAULT_ENCRYPTION));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete()).Times(1);
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));

  // Should write to nigori.
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();

  const ModelTypeSet encrypted_types = GetEncryptedTypes();
  EXPECT_TRUE(encrypted_types.Has(PASSWORDS));  // Hardcoded.
  EXPECT_FALSE(EncryptEverythingEnabledForTest());

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByIdLookup(GetIdForDataType(NIGORI)));
    sync_pb::NigoriSpecifics nigori = node.GetNigoriSpecifics();
    EXPECT_TRUE(nigori.has_encryption_keybag());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    EXPECT_TRUE(cryptographer->CanDecrypt(nigori.encryption_keybag()));
  }
}

TEST_F(SyncManagerTest, EncryptDataTypesWithNoData) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  EXPECT_CALL(encryption_observer_,
              OnEncryptedTypesChanged(
                  HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  sync_manager_.GetEncryptionHandler()->EnableEncryptEverything();
  EXPECT_TRUE(EncryptEverythingEnabledForTest());
}

TEST_F(SyncManagerTest, EncryptDataTypesWithData) {
  size_t batch_size = 5;
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));

  // Create some unencrypted unsynced data.
  int64 folder = MakeFolderWithParent(sync_manager_.GetUserShare(),
                                      BOOKMARKS,
                                      GetIdForDataType(BOOKMARKS),
                                      NULL);
  // First batch_size nodes are children of folder.
  size_t i;
  for (i = 0; i < batch_size; ++i) {
    MakeBookmarkWithParent(sync_manager_.GetUserShare(), folder, NULL);
  }
  // Next batch_size nodes are a different type and on their own.
  for (; i < 2*batch_size; ++i) {
    MakeNode(sync_manager_.GetUserShare(), SESSIONS,
             base::StringPrintf("%"PRIuS"", i));
  }
  // Last batch_size nodes are a third type that will not need encryption.
  for (; i < 3*batch_size; ++i) {
    MakeNode(sync_manager_.GetUserShare(), THEMES,
             base::StringPrintf("%"PRIuS"", i));
  }

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    EXPECT_TRUE(GetEncryptedTypesWithTrans(&trans).Equals(
        SyncEncryptionHandler::SensitiveTypes()));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        BOOKMARKS,
        false /* not encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        SESSIONS,
        false /* not encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        THEMES,
        false /* not encrypted */));
  }

  EXPECT_CALL(encryption_observer_,
              OnEncryptedTypesChanged(
                  HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  sync_manager_.GetEncryptionHandler()->EnableEncryptEverything();
  EXPECT_TRUE(EncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    EXPECT_TRUE(GetEncryptedTypesWithTrans(&trans).Equals(
        EncryptableUserTypes()));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        BOOKMARKS,
        true /* is encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        SESSIONS,
        true /* is encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        THEMES,
        true /* is encrypted */));
  }

  // Trigger's a ReEncryptEverything with new passphrase.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_,
              OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(
      "new_passphrase", true);
  EXPECT_TRUE(EncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    EXPECT_TRUE(GetEncryptedTypesWithTrans(&trans).Equals(
        EncryptableUserTypes()));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        BOOKMARKS,
        true /* is encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        SESSIONS,
        true /* is encrypted */));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        THEMES,
        true /* is encrypted */));
  }
  // Calling EncryptDataTypes with an empty encrypted types should not trigger
  // a reencryption and should just notify immediately.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN)).Times(0);
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted()).Times(0);
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete()).Times(0);
  sync_manager_.GetEncryptionHandler()->EnableEncryptEverything();
}

// Test that when there are no pending keys and the cryptographer is not
// initialized, we add a key based on the current GAIA password.
// (case 1 in SyncManager::SyncInternal::SetEncryptionPassphrase)
TEST_F(SyncManagerTest, SetInitialGaiaPass) {
  EXPECT_FALSE(SetUpEncryption(DONT_WRITE_NIGORI, UNINITIALIZED));
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(
      "new_passphrase",
      false);
  EXPECT_EQ(IMPLICIT_PASSPHRASE,
            sync_manager_.GetEncryptionHandler()->GetPassphraseType());
  EXPECT_FALSE(EncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByTagLookup(kNigoriTag));
    sync_pb::NigoriSpecifics nigori = node.GetNigoriSpecifics();
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    EXPECT_TRUE(cryptographer->CanDecrypt(nigori.encryption_keybag()));
  }
}

// Test that when there are no pending keys and we have on the old GAIA
// password, we update and re-encrypt everything with the new GAIA password.
// (case 1 in SyncManager::SyncInternal::SetEncryptionPassphrase)
TEST_F(SyncManagerTest, UpdateGaiaPass) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  Cryptographer verifier(&encryptor_);
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    std::string bootstrap_token;
    cryptographer->GetBootstrapToken(&bootstrap_token);
    verifier.Bootstrap(bootstrap_token);
  }
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(
      "new_passphrase",
      false);
  EXPECT_EQ(IMPLICIT_PASSPHRASE,
            sync_manager_.GetEncryptionHandler()->GetPassphraseType());
  EXPECT_FALSE(EncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    // Verify the default key has changed.
    sync_pb::EncryptedData encrypted;
    cryptographer->GetKeys(&encrypted);
    EXPECT_FALSE(verifier.CanDecrypt(encrypted));
  }
}

// Sets a new explicit passphrase. This should update the bootstrap token
// and re-encrypt everything.
// (case 2 in SyncManager::SyncInternal::SetEncryptionPassphrase)
TEST_F(SyncManagerTest, SetPassphraseWithPassword) {
  Cryptographer verifier(&encryptor_);
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    // Store the default (soon to be old) key.
    Cryptographer* cryptographer = trans.GetCryptographer();
    std::string bootstrap_token;
    cryptographer->GetBootstrapToken(&bootstrap_token);
    verifier.Bootstrap(bootstrap_token);

    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode password_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        password_node.InitUniqueByCreation(PASSWORDS,
                                           root_node, "foo");
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    password_node.SetPasswordSpecifics(data);
  }
    EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_,
      OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(
      "new_passphrase",
      true);
  EXPECT_EQ(CUSTOM_PASSPHRASE,
            sync_manager_.GetEncryptionHandler()->GetPassphraseType());
  EXPECT_FALSE(EncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    // Verify the default key has changed.
    sync_pb::EncryptedData encrypted;
    cryptographer->GetKeys(&encrypted);
    EXPECT_FALSE(verifier.CanDecrypt(encrypted));

    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              password_node.InitByClientTagLookup(PASSWORDS,
                                                  "foo"));
    const sync_pb::PasswordSpecificsData& data =
        password_node.GetPasswordSpecifics();
    EXPECT_EQ("secret", data.password_value());
  }
}

// Manually set the pending keys in the cryptographer/nigori to reflect the data
// being encrypted with a new (unprovided) GAIA password, then supply the
// password.
// (case 7 in SyncManager::SyncInternal::SetDecryptionPassphrase)
TEST_F(SyncManagerTest, SupplyPendingGAIAPass) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  Cryptographer other_cryptographer(&encryptor_);
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    std::string bootstrap_token;
    cryptographer->GetBootstrapToken(&bootstrap_token);
    other_cryptographer.Bootstrap(bootstrap_token);

    // Now update the nigori to reflect the new keys, and update the
    // cryptographer to have pending keys.
    KeyParams params = {"localhost", "dummy", "passphrase2"};
    other_cryptographer.AddKey(params);
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByTagLookup(kNigoriTag));
    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    cryptographer->SetPendingKeys(nigori.encryption_keybag());
    EXPECT_TRUE(cryptographer->has_pending_keys());
    node.SetNigoriSpecifics(nigori);
  }
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  sync_manager_.GetEncryptionHandler()->SetDecryptionPassphrase("passphrase2");
  EXPECT_EQ(IMPLICIT_PASSPHRASE,
            sync_manager_.GetEncryptionHandler()->GetPassphraseType());
  EXPECT_FALSE(EncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    // Verify we're encrypting with the new key.
    sync_pb::EncryptedData encrypted;
    cryptographer->GetKeys(&encrypted);
    EXPECT_TRUE(other_cryptographer.CanDecrypt(encrypted));
  }
}

// Manually set the pending keys in the cryptographer/nigori to reflect the data
// being encrypted with an old (unprovided) GAIA password. Attempt to supply
// the current GAIA password and verify the bootstrap token is updated. Then
// supply the old GAIA password, and verify we re-encrypt all data with the
// new GAIA password.
// (cases 4 and 5 in SyncManager::SyncInternal::SetEncryptionPassphrase)
TEST_F(SyncManagerTest, SupplyPendingOldGAIAPass) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  Cryptographer other_cryptographer(&encryptor_);
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    std::string bootstrap_token;
    cryptographer->GetBootstrapToken(&bootstrap_token);
    other_cryptographer.Bootstrap(bootstrap_token);

    // Now update the nigori to reflect the new keys, and update the
    // cryptographer to have pending keys.
    KeyParams params = {"localhost", "dummy", "old_gaia"};
    other_cryptographer.AddKey(params);
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByTagLookup(kNigoriTag));
    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    node.SetNigoriSpecifics(nigori);
    cryptographer->SetPendingKeys(nigori.encryption_keybag());

    // other_cryptographer now contains all encryption keys, and is encrypting
    // with the newest gaia.
    KeyParams new_params = {"localhost", "dummy", "new_gaia"};
    other_cryptographer.AddKey(new_params);
  }
  // The bootstrap token should have been updated. Save it to ensure it's based
  // on the new GAIA password.
  std::string bootstrap_token;
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN))
      .WillOnce(SaveArg<0>(&bootstrap_token));
  EXPECT_CALL(encryption_observer_, OnPassphraseRequired(_,_));
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(
      "new_gaia",
      false);
  EXPECT_EQ(IMPLICIT_PASSPHRASE,
            sync_manager_.GetEncryptionHandler()->GetPassphraseType());
  EXPECT_FALSE(EncryptEverythingEnabledForTest());
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_initialized());
    EXPECT_FALSE(cryptographer->is_ready());
    // Verify we're encrypting with the new key, even though we have pending
    // keys.
    sync_pb::EncryptedData encrypted;
    other_cryptographer.GetKeys(&encrypted);
    EXPECT_TRUE(cryptographer->CanDecrypt(encrypted));
  }
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(
      "old_gaia",
      false);
  EXPECT_EQ(IMPLICIT_PASSPHRASE,
            sync_manager_.GetEncryptionHandler()->GetPassphraseType());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());

    // Verify we're encrypting with the new key.
    sync_pb::EncryptedData encrypted;
    other_cryptographer.GetKeys(&encrypted);
    EXPECT_TRUE(cryptographer->CanDecrypt(encrypted));

    // Verify the saved bootstrap token is based on the new gaia password.
    Cryptographer temp_cryptographer(&encryptor_);
    temp_cryptographer.Bootstrap(bootstrap_token);
    EXPECT_TRUE(temp_cryptographer.CanDecrypt(encrypted));
  }
}

// Manually set the pending keys in the cryptographer/nigori to reflect the data
// being encrypted with an explicit (unprovided) passphrase, then supply the
// passphrase.
// (case 9 in SyncManager::SyncInternal::SetDecryptionPassphrase)
TEST_F(SyncManagerTest, SupplyPendingExplicitPass) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  Cryptographer other_cryptographer(&encryptor_);
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    std::string bootstrap_token;
    cryptographer->GetBootstrapToken(&bootstrap_token);
    other_cryptographer.Bootstrap(bootstrap_token);

    // Now update the nigori to reflect the new keys, and update the
    // cryptographer to have pending keys.
    KeyParams params = {"localhost", "dummy", "explicit"};
    other_cryptographer.AddKey(params);
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByTagLookup(kNigoriTag));
    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    cryptographer->SetPendingKeys(nigori.encryption_keybag());
    EXPECT_TRUE(cryptographer->has_pending_keys());
    nigori.set_keybag_is_frozen(true);
    node.SetNigoriSpecifics(nigori);
  }
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_,
              OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  EXPECT_CALL(encryption_observer_, OnPassphraseRequired(_, _));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));
  sync_manager_.GetEncryptionHandler()->Init();
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  sync_manager_.GetEncryptionHandler()->SetDecryptionPassphrase("explicit");
  EXPECT_EQ(CUSTOM_PASSPHRASE,
            sync_manager_.GetEncryptionHandler()->GetPassphraseType());
  EXPECT_FALSE(EncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    // Verify we're encrypting with the new key.
    sync_pb::EncryptedData encrypted;
    cryptographer->GetKeys(&encrypted);
    EXPECT_TRUE(other_cryptographer.CanDecrypt(encrypted));
  }
}

// Manually set the pending keys in the cryptographer/nigori to reflect the data
// being encrypted with a new (unprovided) GAIA password, then supply the
// password as a user-provided password.
// This is the android case 7/8.
TEST_F(SyncManagerTest, SupplyPendingGAIAPassUserProvided) {
  EXPECT_FALSE(SetUpEncryption(DONT_WRITE_NIGORI, UNINITIALIZED));
  Cryptographer other_cryptographer(&encryptor_);
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    // Now update the nigori to reflect the new keys, and update the
    // cryptographer to have pending keys.
    KeyParams params = {"localhost", "dummy", "passphrase"};
    other_cryptographer.AddKey(params);
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByTagLookup(kNigoriTag));
    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    node.SetNigoriSpecifics(nigori);
    cryptographer->SetPendingKeys(nigori.encryption_keybag());
    EXPECT_FALSE(cryptographer->is_ready());
  }
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(
      "passphrase",
      false);
  EXPECT_EQ(IMPLICIT_PASSPHRASE,
            sync_manager_.GetEncryptionHandler()->GetPassphraseType());
  EXPECT_FALSE(EncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
  }
}

TEST_F(SyncManagerTest, SetPassphraseWithEmptyPasswordNode) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  int64 node_id = 0;
  std::string tag = "foo";
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode root_node(&trans);
    root_node.InitByRootLookup();

    WriteNode password_node(&trans);
    WriteNode::InitUniqueByCreationResult result =
        password_node.InitUniqueByCreation(PASSWORDS, root_node, tag);
    EXPECT_EQ(WriteNode::INIT_SUCCESS, result);
    node_id = password_node.GetId();
  }
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_,
      OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(
      "new_passphrase",
      true);
  EXPECT_EQ(CUSTOM_PASSPHRASE,
            sync_manager_.GetEncryptionHandler()->GetPassphraseType());
  EXPECT_FALSE(EncryptEverythingEnabledForTest());
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY,
              password_node.InitByClientTagLookup(PASSWORDS,
                                                  tag));
  }
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode password_node(&trans);
    EXPECT_EQ(BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY,
              password_node.InitByIdLookup(node_id));
  }
}

TEST_F(SyncManagerTest, NudgeDelayTest) {
  EXPECT_EQ(sync_manager_.GetNudgeDelayTimeDelta(BOOKMARKS),
      base::TimeDelta::FromMilliseconds(
          SyncManagerImpl::GetDefaultNudgeDelay()));

  EXPECT_EQ(sync_manager_.GetNudgeDelayTimeDelta(AUTOFILL),
      base::TimeDelta::FromSeconds(
          kDefaultShortPollIntervalSeconds));

  EXPECT_EQ(sync_manager_.GetNudgeDelayTimeDelta(PREFERENCES),
      base::TimeDelta::FromMilliseconds(
          SyncManagerImpl::GetPreferencesNudgeDelay()));
}

// Friended by WriteNode, so can't be in an anonymouse namespace.
TEST_F(SyncManagerTest, EncryptBookmarksWithLegacyData) {
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  std::string title;
  SyncAPINameToServerName("Google", &title);
  std::string url = "http://www.google.com";
  std::string raw_title2 = "..";  // An invalid cosmo title.
  std::string title2;
  SyncAPINameToServerName(raw_title2, &title2);
  std::string url2 = "http://www.bla.com";

  // Create a bookmark using the legacy format.
  int64 node_id1 = MakeNode(sync_manager_.GetUserShare(),
      BOOKMARKS,
      "testtag");
  int64 node_id2 = MakeNode(sync_manager_.GetUserShare(),
      BOOKMARKS,
      "testtag2");
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(node_id1));

    sync_pb::EntitySpecifics entity_specifics;
    entity_specifics.mutable_bookmark()->set_url(url);
    node.SetEntitySpecifics(entity_specifics);

    // Set the old style title.
    syncable::MutableEntry* node_entry = node.entry_;
    node_entry->Put(syncable::NON_UNIQUE_NAME, title);

    WriteNode node2(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node2.InitByIdLookup(node_id2));

    sync_pb::EntitySpecifics entity_specifics2;
    entity_specifics2.mutable_bookmark()->set_url(url2);
    node2.SetEntitySpecifics(entity_specifics2);

    // Set the old style title.
    syncable::MutableEntry* node_entry2 = node2.entry_;
    node_entry2->Put(syncable::NON_UNIQUE_NAME, title2);
  }

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(node_id1));
    EXPECT_EQ(BOOKMARKS, node.GetModelType());
    EXPECT_EQ(title, node.GetTitle());
    EXPECT_EQ(title, node.GetBookmarkSpecifics().title());
    EXPECT_EQ(url, node.GetBookmarkSpecifics().url());

    ReadNode node2(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node2.InitByIdLookup(node_id2));
    EXPECT_EQ(BOOKMARKS, node2.GetModelType());
    // We should de-canonicalize the title in GetTitle(), but the title in the
    // specifics should be stored in the server legal form.
    EXPECT_EQ(raw_title2, node2.GetTitle());
    EXPECT_EQ(title2, node2.GetBookmarkSpecifics().title());
    EXPECT_EQ(url2, node2.GetBookmarkSpecifics().url());
  }

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        BOOKMARKS,
        false /* not encrypted */));
  }

  EXPECT_CALL(encryption_observer_,
              OnEncryptedTypesChanged(
                  HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  sync_manager_.GetEncryptionHandler()->EnableEncryptEverything();
  EXPECT_TRUE(EncryptEverythingEnabledForTest());

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    EXPECT_TRUE(GetEncryptedTypesWithTrans(&trans).Equals(
        EncryptableUserTypes()));
    EXPECT_TRUE(syncable::VerifyDataTypeEncryptionForTest(
        trans.GetWrappedTrans(),
        BOOKMARKS,
        true /* is encrypted */));

    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(node_id1));
    EXPECT_EQ(BOOKMARKS, node.GetModelType());
    EXPECT_EQ(title, node.GetTitle());
    EXPECT_EQ(title, node.GetBookmarkSpecifics().title());
    EXPECT_EQ(url, node.GetBookmarkSpecifics().url());

    ReadNode node2(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, node2.InitByIdLookup(node_id2));
    EXPECT_EQ(BOOKMARKS, node2.GetModelType());
    // We should de-canonicalize the title in GetTitle(), but the title in the
    // specifics should be stored in the server legal form.
    EXPECT_EQ(raw_title2, node2.GetTitle());
    EXPECT_EQ(title2, node2.GetBookmarkSpecifics().title());
    EXPECT_EQ(url2, node2.GetBookmarkSpecifics().url());
  }
}

// Create a bookmark and set the title/url, then verify the data was properly
// set. This replicates the unique way bookmarks have of creating sync nodes.
// See BookmarkChangeProcessor::PlaceSyncNode(..).
TEST_F(SyncManagerTest, CreateLocalBookmark) {
  std::string title = "title";
  std::string url = "url";
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode bookmark_root(&trans);
    ASSERT_EQ(BaseNode::INIT_OK,
              bookmark_root.InitByTagLookup(ModelTypeToRootTag(BOOKMARKS)));
    WriteNode node(&trans);
    ASSERT_TRUE(node.InitBookmarkByCreation(bookmark_root, NULL));
    node.SetIsFolder(false);
    node.SetTitle(UTF8ToWide(title));

    sync_pb::BookmarkSpecifics bookmark_specifics(node.GetBookmarkSpecifics());
    bookmark_specifics.set_url(url);
    node.SetBookmarkSpecifics(bookmark_specifics);
  }
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode bookmark_root(&trans);
    ASSERT_EQ(BaseNode::INIT_OK,
              bookmark_root.InitByTagLookup(ModelTypeToRootTag(BOOKMARKS)));
    int64 child_id = bookmark_root.GetFirstChildId();

    ReadNode node(&trans);
    ASSERT_EQ(BaseNode::INIT_OK, node.InitByIdLookup(child_id));
    EXPECT_FALSE(node.GetIsFolder());
    EXPECT_EQ(title, node.GetTitle());
    EXPECT_EQ(url, node.GetBookmarkSpecifics().url());
  }
}

// Verifies WriteNode::UpdateEntryWithEncryption does not make unnecessary
// changes.
TEST_F(SyncManagerTest, UpdateEntryWithEncryption) {
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("url");
  entity_specifics.mutable_bookmark()->set_title("title");
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, client_tag,
                 syncable::GenerateSyncableHash(BOOKMARKS,
                                                client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));
  // Manually change to the same data. Should not set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetEntitySpecifics(entity_specifics);
  }
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Encrypt the datatatype, should set is_unsynced.
  EXPECT_CALL(encryption_observer_,
              OnEncryptedTypesChanged(
                  HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, FULL_ENCRYPTION));

  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, true));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->Get(SPECIFICS);
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->Get(NON_UNIQUE_NAME));
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    EXPECT_TRUE(cryptographer->CanDecryptUsingDefaultKey(
        specifics.encrypted()));
  }
  EXPECT_TRUE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Set a new passphrase. Should set is_unsynced.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_,
      OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(
      "new_passphrase",
      true);
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->Get(SPECIFICS);
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->Get(NON_UNIQUE_NAME));
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->is_ready());
    EXPECT_TRUE(cryptographer->CanDecryptUsingDefaultKey(
        specifics.encrypted()));
  }
  EXPECT_TRUE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Force a re-encrypt everything. Should not set is_unsynced.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, true));

  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();

  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->Get(SPECIFICS);
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->Get(NON_UNIQUE_NAME));
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->CanDecryptUsingDefaultKey(
        specifics.encrypted()));
  }
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to the same data. Should not set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetEntitySpecifics(entity_specifics);
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->Get(SPECIFICS);
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_FALSE(node_entry->Get(IS_UNSYNCED));
    EXPECT_EQ(kEncryptedString, node_entry->Get(NON_UNIQUE_NAME));
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->CanDecryptUsingDefaultKey(
        specifics.encrypted()));
  }
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to different data. Should set is_unsynced.
  {
    entity_specifics.mutable_bookmark()->set_url("url2");
    entity_specifics.mutable_bookmark()->set_title("title2");
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetEntitySpecifics(entity_specifics);
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->Get(SPECIFICS);
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_TRUE(node_entry->Get(IS_UNSYNCED));
    EXPECT_EQ(kEncryptedString, node_entry->Get(NON_UNIQUE_NAME));
    Cryptographer* cryptographer = trans.GetCryptographer();
    EXPECT_TRUE(cryptographer->CanDecryptUsingDefaultKey(
                    specifics.encrypted()));
  }
}

// Passwords have their own handling for encryption. Verify it does not result
// in unnecessary writes via SetEntitySpecifics.
TEST_F(SyncManagerTest, UpdatePasswordSetEntitySpecificsNoChange) {
  std::string client_tag = "title";
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    cryptographer->Encrypt(
        data,
        entity_specifics.mutable_password()->
            mutable_encrypted());
  }
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, client_tag,
                 syncable::GenerateSyncableHash(PASSWORDS,
                                                client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));

  // Manually change to the same data via SetEntitySpecifics. Should not set
  // is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PASSWORDS, client_tag));
    node.SetEntitySpecifics(entity_specifics);
  }
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));
}

// Passwords have their own handling for encryption. Verify it does not result
// in unnecessary writes via SetPasswordSpecifics.
TEST_F(SyncManagerTest, UpdatePasswordSetPasswordSpecifics) {
  std::string client_tag = "title";
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    cryptographer->Encrypt(
        data,
        entity_specifics.mutable_password()->
            mutable_encrypted());
  }
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, client_tag,
                 syncable::GenerateSyncableHash(PASSWORDS,
                                                client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));

  // Manually change to the same data via SetPasswordSpecifics. Should not set
  // is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PASSWORDS, client_tag));
    node.SetPasswordSpecifics(node.GetPasswordSpecifics());
  }
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));

  // Manually change to different data. Should set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PASSWORDS, client_tag));
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret2");
    cryptographer->Encrypt(
        data,
        entity_specifics.mutable_password()->mutable_encrypted());
    node.SetPasswordSpecifics(data);
    const syncable::Entry* node_entry = node.GetEntry();
    EXPECT_TRUE(node_entry->Get(IS_UNSYNCED));
  }
}

// Passwords have their own handling for encryption. Verify setting a new
// passphrase updates the data.
TEST_F(SyncManagerTest, UpdatePasswordNewPassphrase) {
  std::string client_tag = "title";
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    cryptographer->Encrypt(
        data,
        entity_specifics.mutable_password()->mutable_encrypted());
  }
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, client_tag,
                 syncable::GenerateSyncableHash(PASSWORDS,
                                                client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));

  // Set a new passphrase. Should set is_unsynced.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_,
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(encryption_observer_, OnPassphraseAccepted());
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_,
      OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  sync_manager_.GetEncryptionHandler()->SetEncryptionPassphrase(
      "new_passphrase",
      true);
  EXPECT_EQ(CUSTOM_PASSPHRASE,
            sync_manager_.GetEncryptionHandler()->GetPassphraseType());
  EXPECT_TRUE(ResetUnsyncedEntry(PASSWORDS, client_tag));
}

// Passwords have their own handling for encryption. Verify it does not result
// in unnecessary writes via ReencryptEverything.
TEST_F(SyncManagerTest, UpdatePasswordReencryptEverything) {
  std::string client_tag = "title";
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  sync_pb::EntitySpecifics entity_specifics;
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    sync_pb::PasswordSpecificsData data;
    data.set_password_value("secret");
    cryptographer->Encrypt(
        data,
        entity_specifics.mutable_password()->mutable_encrypted());
  }
  MakeServerNode(sync_manager_.GetUserShare(), PASSWORDS, client_tag,
                 syncable::GenerateSyncableHash(PASSWORDS,
                                                client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));

  // Force a re-encrypt everything. Should not set is_unsynced.
  testing::Mock::VerifyAndClearExpectations(&encryption_observer_);
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, false));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  EXPECT_FALSE(ResetUnsyncedEntry(PASSWORDS, client_tag));
}

// Verify SetTitle(..) doesn't unnecessarily set IS_UNSYNCED for bookmarks
// when we write the same data, but does set it when we write new data.
TEST_F(SyncManagerTest, SetBookmarkTitle) {
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("url");
  entity_specifics.mutable_bookmark()->set_title("title");
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, client_tag,
                 syncable::GenerateSyncableHash(BOOKMARKS,
                                                client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to the same title. Should not set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetTitle(UTF8ToWide(client_tag));
  }
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to new title. Should set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetTitle(UTF8ToWide("title2"));
  }
  EXPECT_TRUE(ResetUnsyncedEntry(BOOKMARKS, client_tag));
}

// Verify SetTitle(..) doesn't unnecessarily set IS_UNSYNCED for encrypted
// bookmarks when we write the same data, but does set it when we write new
// data.
TEST_F(SyncManagerTest, SetBookmarkTitleWithEncryption) {
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("url");
  entity_specifics.mutable_bookmark()->set_title("title");
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, client_tag,
                 syncable::GenerateSyncableHash(BOOKMARKS,
                                                client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Encrypt the datatatype, should set is_unsynced.
  EXPECT_CALL(encryption_observer_,
              OnEncryptedTypesChanged(
                  HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, FULL_ENCRYPTION));
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, true));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  EXPECT_TRUE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to the same title. Should not set is_unsynced.
  // NON_UNIQUE_NAME should be kEncryptedString.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetTitle(UTF8ToWide(client_tag));
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->Get(SPECIFICS);
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->Get(NON_UNIQUE_NAME));
  }
  EXPECT_FALSE(ResetUnsyncedEntry(BOOKMARKS, client_tag));

  // Manually change to new title. Should set is_unsynced. NON_UNIQUE_NAME
  // should still be kEncryptedString.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    node.SetTitle(UTF8ToWide("title2"));
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->Get(SPECIFICS);
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->Get(NON_UNIQUE_NAME));
  }
  EXPECT_TRUE(ResetUnsyncedEntry(BOOKMARKS, client_tag));
}

// Verify SetTitle(..) doesn't unnecessarily set IS_UNSYNCED for non-bookmarks
// when we write the same data, but does set it when we write new data.
TEST_F(SyncManagerTest, SetNonBookmarkTitle) {
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_preference()->set_name("name");
  entity_specifics.mutable_preference()->set_value("value");
  MakeServerNode(sync_manager_.GetUserShare(),
                 PREFERENCES,
                 client_tag,
                 syncable::GenerateSyncableHash(PREFERENCES,
                                                client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PREFERENCES, client_tag));

  // Manually change to the same title. Should not set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, client_tag));
    node.SetTitle(UTF8ToWide(client_tag));
  }
  EXPECT_FALSE(ResetUnsyncedEntry(PREFERENCES, client_tag));

  // Manually change to new title. Should set is_unsynced.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, client_tag));
    node.SetTitle(UTF8ToWide("title2"));
  }
  EXPECT_TRUE(ResetUnsyncedEntry(PREFERENCES, client_tag));
}

// Verify SetTitle(..) doesn't unnecessarily set IS_UNSYNCED for encrypted
// non-bookmarks when we write the same data or when we write new data
// data (should remained kEncryptedString).
TEST_F(SyncManagerTest, SetNonBookmarkTitleWithEncryption) {
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_preference()->set_name("name");
  entity_specifics.mutable_preference()->set_value("value");
  MakeServerNode(sync_manager_.GetUserShare(),
                 PREFERENCES,
                 client_tag,
                 syncable::GenerateSyncableHash(PREFERENCES,
                                                client_tag),
                 entity_specifics);
  // New node shouldn't start off unsynced.
  EXPECT_FALSE(ResetUnsyncedEntry(PREFERENCES, client_tag));

  // Encrypt the datatatype, should set is_unsynced.
  EXPECT_CALL(encryption_observer_,
              OnEncryptedTypesChanged(
                  HasModelTypes(EncryptableUserTypes()), true));
  EXPECT_CALL(encryption_observer_, OnEncryptionComplete());
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, FULL_ENCRYPTION));
  EXPECT_CALL(encryption_observer_, OnCryptographerStateChanged(_));
  EXPECT_CALL(encryption_observer_, OnEncryptedTypesChanged(_, true));
  sync_manager_.GetEncryptionHandler()->Init();
  PumpLoop();
  EXPECT_TRUE(ResetUnsyncedEntry(PREFERENCES, client_tag));

  // Manually change to the same title. Should not set is_unsynced.
  // NON_UNIQUE_NAME should be kEncryptedString.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, client_tag));
    node.SetTitle(UTF8ToWide(client_tag));
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->Get(SPECIFICS);
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->Get(NON_UNIQUE_NAME));
  }
  EXPECT_FALSE(ResetUnsyncedEntry(PREFERENCES, client_tag));

  // Manually change to new title. Should not set is_unsynced because the
  // NON_UNIQUE_NAME should still be kEncryptedString.
  {
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(PREFERENCES, client_tag));
    node.SetTitle(UTF8ToWide("title2"));
    const syncable::Entry* node_entry = node.GetEntry();
    const sync_pb::EntitySpecifics& specifics = node_entry->Get(SPECIFICS);
    EXPECT_TRUE(specifics.has_encrypted());
    EXPECT_EQ(kEncryptedString, node_entry->Get(NON_UNIQUE_NAME));
    EXPECT_FALSE(node_entry->Get(IS_UNSYNCED));
  }
}

// Create an encrypted entry when the cryptographer doesn't think the type is
// marked for encryption. Ensure reads/writes don't break and don't unencrypt
// the data.
TEST_F(SyncManagerTest, SetPreviouslyEncryptedSpecifics) {
  std::string client_tag = "tag";
  std::string url = "url";
  std::string url2 = "new_url";
  std::string title = "title";
  sync_pb::EntitySpecifics entity_specifics;
  EXPECT_TRUE(SetUpEncryption(WRITE_TO_NIGORI, DEFAULT_ENCRYPTION));
  {
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    Cryptographer* crypto = trans.GetCryptographer();
    sync_pb::EntitySpecifics bm_specifics;
    bm_specifics.mutable_bookmark()->set_title("title");
    bm_specifics.mutable_bookmark()->set_url("url");
    sync_pb::EncryptedData encrypted;
    crypto->Encrypt(bm_specifics, &encrypted);
    entity_specifics.mutable_encrypted()->CopyFrom(encrypted);
    AddDefaultFieldValue(BOOKMARKS, &entity_specifics);
  }
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, client_tag,
                 syncable::GenerateSyncableHash(BOOKMARKS,
                                                client_tag),
                 entity_specifics);

  {
    // Verify the data.
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    EXPECT_EQ(title, node.GetTitle());
    EXPECT_EQ(url, node.GetBookmarkSpecifics().url());
  }

  {
    // Overwrite the url (which overwrites the specifics).
    WriteTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    WriteNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));

    sync_pb::BookmarkSpecifics bookmark_specifics(node.GetBookmarkSpecifics());
    bookmark_specifics.set_url(url2);
    node.SetBookmarkSpecifics(bookmark_specifics);
  }

  {
    // Verify it's still encrypted and it has the most recent url.
    ReadTransaction trans(FROM_HERE, sync_manager_.GetUserShare());
    ReadNode node(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              node.InitByClientTagLookup(BOOKMARKS, client_tag));
    EXPECT_EQ(title, node.GetTitle());
    EXPECT_EQ(url2, node.GetBookmarkSpecifics().url());
    const syncable::Entry* node_entry = node.GetEntry();
    EXPECT_EQ(kEncryptedString, node_entry->Get(NON_UNIQUE_NAME));
    const sync_pb::EntitySpecifics& specifics = node_entry->Get(SPECIFICS);
    EXPECT_TRUE(specifics.has_encrypted());
  }
}

// Verify transaction version of a model type is incremented when node of
// that type is updated.
TEST_F(SyncManagerTest, IncrementTransactionVersion) {
  ModelSafeRoutingInfo routing_info;
  GetModelSafeRoutingInfo(&routing_info);

  {
    ReadTransaction read_trans(FROM_HERE, sync_manager_.GetUserShare());
    for (ModelSafeRoutingInfo::iterator i = routing_info.begin();
         i != routing_info.end(); ++i) {
      // Transaction version is incremented when SyncManagerTest::SetUp()
      // creates a node of each type.
      EXPECT_EQ(1,
                sync_manager_.GetUserShare()->directory->
                    GetTransactionVersion(i->first));
    }
  }

  // Create bookmark node to increment transaction version of bookmark model.
  std::string client_tag = "title";
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_bookmark()->set_url("url");
  entity_specifics.mutable_bookmark()->set_title("title");
  MakeServerNode(sync_manager_.GetUserShare(), BOOKMARKS, client_tag,
                 syncable::GenerateSyncableHash(BOOKMARKS,
                                                client_tag),
                 entity_specifics);

  {
    ReadTransaction read_trans(FROM_HERE, sync_manager_.GetUserShare());
    for (ModelSafeRoutingInfo::iterator i = routing_info.begin();
         i != routing_info.end(); ++i) {
      EXPECT_EQ(i->first == BOOKMARKS ? 2 : 1,
                sync_manager_.GetUserShare()->directory->
                    GetTransactionVersion(i->first));
    }
  }
}

class MockSyncScheduler : public FakeSyncScheduler {
 public:
  MockSyncScheduler() : FakeSyncScheduler() {}
  virtual ~MockSyncScheduler() {}

  MOCK_METHOD1(Start, void(SyncScheduler::Mode));
  MOCK_METHOD1(ScheduleConfiguration, bool(const ConfigurationParams&));
};

class ComponentsFactory : public TestInternalComponentsFactory {
 public:
  ComponentsFactory(const Switches& switches,
                    SyncScheduler* scheduler_to_use,
                    sessions::SyncSessionContext** session_context)
      : TestInternalComponentsFactory(switches, syncer::STORAGE_IN_MEMORY),
        scheduler_to_use_(scheduler_to_use),
        session_context_(session_context) {}
  virtual ~ComponentsFactory() {}

  virtual scoped_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      sessions::SyncSessionContext* context) OVERRIDE {
    *session_context_ = context;
    return scheduler_to_use_.Pass();
  }

 private:
  scoped_ptr<SyncScheduler> scheduler_to_use_;
  sessions::SyncSessionContext** session_context_;
};

class SyncManagerTestWithMockScheduler : public SyncManagerTest {
 public:
  SyncManagerTestWithMockScheduler() : scheduler_(NULL) {}
  virtual InternalComponentsFactory* GetFactory() OVERRIDE {
    scheduler_ = new MockSyncScheduler();
    return new ComponentsFactory(GetSwitches(), scheduler_, &session_context_);
  }

  MockSyncScheduler* scheduler() { return scheduler_; }
  sessions::SyncSessionContext* session_context() {
      return session_context_;
  }

 private:
  MockSyncScheduler* scheduler_;
  sessions::SyncSessionContext* session_context_;
};

// Test that the configuration params are properly created and sent to
// ScheduleConfigure. No callback should be invoked. Any disabled datatypes
// should be purged.
TEST_F(SyncManagerTestWithMockScheduler, BasicConfiguration) {
  ConfigureReason reason = CONFIGURE_REASON_RECONFIGURATION;
  ModelTypeSet types_to_download(BOOKMARKS, PREFERENCES);
  ModelSafeRoutingInfo new_routing_info;
  GetModelSafeRoutingInfo(&new_routing_info);
  ModelTypeSet enabled_types = GetRoutingInfoTypes(new_routing_info);
  ModelTypeSet disabled_types = Difference(ModelTypeSet::All(), enabled_types);

  ConfigurationParams params;
  EXPECT_CALL(*scheduler(), Start(SyncScheduler::CONFIGURATION_MODE));
  EXPECT_CALL(*scheduler(), ScheduleConfiguration(_)).
      WillOnce(DoAll(SaveArg<0>(&params), Return(true)));

  // Set data for all types.
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelTypeSet::Iterator iter = protocol_types.First(); iter.Good();
       iter.Inc()) {
    SetProgressMarkerForType(iter.Get(), true);
  }

  CallbackCounter ready_task_counter, retry_task_counter;
  sync_manager_.ConfigureSyncer(
      reason,
      types_to_download,
      ModelTypeSet(),
      new_routing_info,
      base::Bind(&CallbackCounter::Callback,
                 base::Unretained(&ready_task_counter)),
      base::Bind(&CallbackCounter::Callback,
                 base::Unretained(&retry_task_counter)));
  EXPECT_EQ(0, ready_task_counter.times_called());
  EXPECT_EQ(0, retry_task_counter.times_called());
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::RECONFIGURATION,
            params.source);
  EXPECT_TRUE(types_to_download.Equals(params.types_to_download));
  EXPECT_EQ(new_routing_info, params.routing_info);

  // Verify all the disabled types were purged.
  EXPECT_TRUE(sync_manager_.InitialSyncEndedTypes().Equals(
      enabled_types));
  EXPECT_TRUE(sync_manager_.GetTypesWithEmptyProgressMarkerToken(
      ModelTypeSet::All()).Equals(disabled_types));
}

// Test that on a reconfiguration (configuration where the session context
// already has routing info), only those recently disabled types are purged.
TEST_F(SyncManagerTestWithMockScheduler, ReConfiguration) {
  ConfigureReason reason = CONFIGURE_REASON_RECONFIGURATION;
  ModelTypeSet types_to_download(BOOKMARKS, PREFERENCES);
  ModelTypeSet disabled_types = ModelTypeSet(THEMES, SESSIONS);
  ModelSafeRoutingInfo old_routing_info;
  ModelSafeRoutingInfo new_routing_info;
  GetModelSafeRoutingInfo(&old_routing_info);
  new_routing_info = old_routing_info;
  new_routing_info.erase(THEMES);
  new_routing_info.erase(SESSIONS);
  ModelTypeSet enabled_types = GetRoutingInfoTypes(new_routing_info);

  ConfigurationParams params;
  EXPECT_CALL(*scheduler(), Start(SyncScheduler::CONFIGURATION_MODE));
  EXPECT_CALL(*scheduler(), ScheduleConfiguration(_)).
      WillOnce(DoAll(SaveArg<0>(&params), Return(true)));

  // Set data for all types except those recently disabled (so we can verify
  // only those recently disabled are purged) .
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelTypeSet::Iterator iter = protocol_types.First(); iter.Good();
       iter.Inc()) {
    if (!disabled_types.Has(iter.Get())) {
      SetProgressMarkerForType(iter.Get(), true);
    } else {
      SetProgressMarkerForType(iter.Get(), false);
    }
  }

  // Set the context to have the old routing info.
  session_context()->set_routing_info(old_routing_info);

  CallbackCounter ready_task_counter, retry_task_counter;
  sync_manager_.ConfigureSyncer(
      reason,
      types_to_download,
      ModelTypeSet(),
      new_routing_info,
      base::Bind(&CallbackCounter::Callback,
                 base::Unretained(&ready_task_counter)),
      base::Bind(&CallbackCounter::Callback,
                 base::Unretained(&retry_task_counter)));
  EXPECT_EQ(0, ready_task_counter.times_called());
  EXPECT_EQ(0, retry_task_counter.times_called());
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::RECONFIGURATION,
            params.source);
  EXPECT_TRUE(types_to_download.Equals(params.types_to_download));
  EXPECT_EQ(new_routing_info, params.routing_info);

  // Verify only the recently disabled types were purged.
  EXPECT_TRUE(sync_manager_.GetTypesWithEmptyProgressMarkerToken(
      ProtocolTypes()).Equals(disabled_types));
}

// Test that the retry callback is invoked on configuration failure.
TEST_F(SyncManagerTestWithMockScheduler, ConfigurationRetry) {
  ConfigureReason reason = CONFIGURE_REASON_RECONFIGURATION;
  ModelTypeSet types_to_download(BOOKMARKS, PREFERENCES);
  ModelSafeRoutingInfo new_routing_info;
  GetModelSafeRoutingInfo(&new_routing_info);

  ConfigurationParams params;
  EXPECT_CALL(*scheduler(), Start(SyncScheduler::CONFIGURATION_MODE));
  EXPECT_CALL(*scheduler(), ScheduleConfiguration(_)).
      WillOnce(DoAll(SaveArg<0>(&params), Return(false)));

  CallbackCounter ready_task_counter, retry_task_counter;
  sync_manager_.ConfigureSyncer(
      reason,
      types_to_download,
      ModelTypeSet(),
      new_routing_info,
      base::Bind(&CallbackCounter::Callback,
                 base::Unretained(&ready_task_counter)),
      base::Bind(&CallbackCounter::Callback,
                 base::Unretained(&retry_task_counter)));
  EXPECT_EQ(0, ready_task_counter.times_called());
  EXPECT_EQ(1, retry_task_counter.times_called());
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::RECONFIGURATION,
            params.source);
  EXPECT_TRUE(types_to_download.Equals(params.types_to_download));
  EXPECT_EQ(new_routing_info, params.routing_info);
}

// Test that PurgePartiallySyncedTypes purges only those types that have not
// fully completed their initial download and apply.
TEST_F(SyncManagerTest, PurgePartiallySyncedTypes) {
  ModelSafeRoutingInfo routing_info;
  GetModelSafeRoutingInfo(&routing_info);
  ModelTypeSet enabled_types = GetRoutingInfoTypes(routing_info);

  UserShare* share = sync_manager_.GetUserShare();

  // The test harness automatically initializes all types in the routing info.
  // Check that autofill is not among them.
  ASSERT_FALSE(enabled_types.Has(AUTOFILL));

  // Further ensure that the test harness did not create its root node.
  {
    syncable::ReadTransaction trans(FROM_HERE, share->directory.get());
    syncable::Entry autofill_root_node(&trans, syncable::GET_BY_SERVER_TAG,
                                       ModelTypeToRootTag(AUTOFILL));
    ASSERT_FALSE(autofill_root_node.good());
  }

  // One more redundant check.
  ASSERT_FALSE(sync_manager_.InitialSyncEndedTypes().Has(AUTOFILL));

  // Give autofill a progress marker.
  sync_pb::DataTypeProgressMarker autofill_marker;
  autofill_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(AUTOFILL));
  autofill_marker.set_token("token");
  share->directory->SetDownloadProgress(AUTOFILL, autofill_marker);

  // Also add a pending autofill root node update from the server.
  TestEntryFactory factory_(share->directory.get());
  int autofill_meta = factory_.CreateUnappliedRootNode(AUTOFILL);

  // Preferences is an enabled type.  Check that the harness initialized it.
  ASSERT_TRUE(enabled_types.Has(PREFERENCES));
  ASSERT_TRUE(sync_manager_.InitialSyncEndedTypes().Has(PREFERENCES));

  // Give preferencse a progress marker.
  sync_pb::DataTypeProgressMarker prefs_marker;
  prefs_marker.set_data_type_id(
      GetSpecificsFieldNumberFromModelType(PREFERENCES));
  prefs_marker.set_token("token");
  share->directory->SetDownloadProgress(PREFERENCES, prefs_marker);

  // Add a fully synced preferences node under the root.
  std::string pref_client_tag = "prefABC";
  std::string pref_hashed_tag = "hashXYZ";
  sync_pb::EntitySpecifics pref_specifics;
  AddDefaultFieldValue(PREFERENCES, &pref_specifics);
  int pref_meta = MakeServerNode(
      share, PREFERENCES, pref_client_tag, pref_hashed_tag, pref_specifics);

  // And now, the purge.
  EXPECT_TRUE(sync_manager_.PurgePartiallySyncedTypes());

  // Ensure that autofill lost its progress marker, but preferences did not.
  ModelTypeSet empty_tokens =
      sync_manager_.GetTypesWithEmptyProgressMarkerToken(ModelTypeSet::All());
  EXPECT_TRUE(empty_tokens.Has(AUTOFILL));
  EXPECT_FALSE(empty_tokens.Has(PREFERENCES));

  // Ensure that autofill lots its node, but preferences did not.
  {
    syncable::ReadTransaction trans(FROM_HERE, share->directory.get());
    syncable::Entry autofill_node(&trans, GET_BY_HANDLE, autofill_meta);
    syncable::Entry pref_node(&trans, GET_BY_HANDLE, pref_meta);
    EXPECT_FALSE(autofill_node.good());
    EXPECT_TRUE(pref_node.good());
  }
}

// Test CleanupDisabledTypes properly purges all disabled types as specified
// by the previous and current enabled params.
TEST_F(SyncManagerTest, PurgeDisabledTypes) {
  ModelSafeRoutingInfo routing_info;
  GetModelSafeRoutingInfo(&routing_info);
  ModelTypeSet enabled_types = GetRoutingInfoTypes(routing_info);
  ModelTypeSet disabled_types = Difference(ModelTypeSet::All(), enabled_types);

  // The harness should have initialized the enabled_types for us.
  EXPECT_TRUE(enabled_types.Equals(sync_manager_.InitialSyncEndedTypes()));

  // Set progress markers for all types.
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelTypeSet::Iterator iter = protocol_types.First(); iter.Good();
       iter.Inc()) {
    SetProgressMarkerForType(iter.Get(), true);
  }

  // Verify all the enabled types remain after cleanup, and all the disabled
  // types were purged.
  sync_manager_.PurgeDisabledTypes(ModelTypeSet::All(), enabled_types,
                                   ModelTypeSet());
  EXPECT_TRUE(enabled_types.Equals(sync_manager_.InitialSyncEndedTypes()));
  EXPECT_TRUE(disabled_types.Equals(
      sync_manager_.GetTypesWithEmptyProgressMarkerToken(ModelTypeSet::All())));

  // Disable some more types.
  disabled_types.Put(BOOKMARKS);
  disabled_types.Put(PREFERENCES);
  ModelTypeSet new_enabled_types =
      Difference(ModelTypeSet::All(), disabled_types);

  // Verify only the non-disabled types remain after cleanup.
  sync_manager_.PurgeDisabledTypes(enabled_types, new_enabled_types,
                                   ModelTypeSet());
  EXPECT_TRUE(new_enabled_types.Equals(sync_manager_.InitialSyncEndedTypes()));
  EXPECT_TRUE(disabled_types.Equals(
      sync_manager_.GetTypesWithEmptyProgressMarkerToken(ModelTypeSet::All())));
}

// A test harness to exercise the code that processes and passes changes from
// the "SYNCER"-WriteTransaction destructor, through the SyncManager, to the
// ChangeProcessor.
class SyncManagerChangeProcessingTest : public SyncManagerTest {
 public:
  virtual void OnChangesApplied(
      ModelType model_type,
      int64 model_version,
      const BaseTransaction* trans,
      const ImmutableChangeRecordList& changes) OVERRIDE {
    last_changes_ = changes;
  }

  virtual void OnChangesComplete(ModelType model_type) OVERRIDE {}

  const ImmutableChangeRecordList& GetRecentChangeList() {
    return last_changes_;
  }

  UserShare* share() {
    return sync_manager_.GetUserShare();
  }

  // Set some flags so our nodes reasonably approximate the real world scenario
  // and can get past CheckTreeInvariants.
  //
  // It's never going to be truly accurate, since we're squashing update
  // receipt, processing and application into a single transaction.
  void SetNodeProperties(syncable::MutableEntry *entry) {
    entry->Put(syncable::ID, id_factory_.NewServerId());
    entry->Put(syncable::BASE_VERSION, 10);
    entry->Put(syncable::SERVER_VERSION, 10);
  }

  // Looks for the given change in the list.  Returns the index at which it was
  // found.  Returns -1 on lookup failure.
  size_t FindChangeInList(int64 id, ChangeRecord::Action action) {
    SCOPED_TRACE(id);
    for (size_t i = 0; i < last_changes_.Get().size(); ++i) {
      if (last_changes_.Get()[i].id == id
          && last_changes_.Get()[i].action == action) {
        return i;
      }
    }
    ADD_FAILURE() << "Failed to find specified change";
    return -1;
  }

  // Returns the current size of the change list.
  //
  // Note that spurious changes do not necessarily indicate a problem.
  // Assertions on change list size can help detect problems, but it may be
  // necessary to reduce their strictness if the implementation changes.
  size_t GetChangeListSize() {
    return last_changes_.Get().size();
  }

 protected:
  ImmutableChangeRecordList last_changes_;
  TestIdFactory id_factory_;
};

// Test creation of a folder and a bookmark.
TEST_F(SyncManagerChangeProcessingTest, AddBookmarks) {
  int64 type_root = GetIdForDataType(BOOKMARKS);
  int64 folder_id = kInvalidId;
  int64 child_id = kInvalidId;

  // Create a folder and a bookmark under it.
  {
    syncable::WriteTransaction trans(
        FROM_HERE, syncable::SYNCER, share()->directory.get());
    syncable::Entry root(&trans, syncable::GET_BY_HANDLE, type_root);
    ASSERT_TRUE(root.good());

    syncable::MutableEntry folder(&trans, syncable::CREATE,
                                  BOOKMARKS, root.Get(syncable::ID), "folder");
    ASSERT_TRUE(folder.good());
    SetNodeProperties(&folder);
    folder.Put(syncable::IS_DIR, true);
    folder_id = folder.Get(syncable::META_HANDLE);

    syncable::MutableEntry child(&trans, syncable::CREATE,
                                 BOOKMARKS, folder.Get(syncable::ID), "child");
    ASSERT_TRUE(child.good());
    SetNodeProperties(&child);
    child_id = child.Get(syncable::META_HANDLE);
  }

  // The closing of the above scope will delete the transaction.  Its processed
  // changes should be waiting for us in a member of the test harness.
  EXPECT_EQ(2UL, GetChangeListSize());

  // We don't need to check these return values here.  The function will add a
  // non-fatal failure if these changes are not found.
  size_t folder_change_pos =
      FindChangeInList(folder_id, ChangeRecord::ACTION_ADD);
  size_t child_change_pos =
      FindChangeInList(child_id, ChangeRecord::ACTION_ADD);

  // Parents are delivered before children.
  EXPECT_LT(folder_change_pos, child_change_pos);
}

// Test moving a bookmark into an empty folder.
TEST_F(SyncManagerChangeProcessingTest, MoveBookmarkIntoEmptyFolder) {
  int64 type_root = GetIdForDataType(BOOKMARKS);
  int64 folder_b_id = kInvalidId;
  int64 child_id = kInvalidId;

  // Create two folders.  Place a child under folder A.
  {
    syncable::WriteTransaction trans(
        FROM_HERE, syncable::SYNCER, share()->directory.get());
    syncable::Entry root(&trans, syncable::GET_BY_HANDLE, type_root);
    ASSERT_TRUE(root.good());

    syncable::MutableEntry folder_a(&trans, syncable::CREATE,
                                  BOOKMARKS, root.Get(syncable::ID), "folderA");
    ASSERT_TRUE(folder_a.good());
    SetNodeProperties(&folder_a);
    folder_a.Put(syncable::IS_DIR, true);

    syncable::MutableEntry folder_b(&trans, syncable::CREATE,
                                  BOOKMARKS, root.Get(syncable::ID), "folderB");
    ASSERT_TRUE(folder_b.good());
    SetNodeProperties(&folder_b);
    folder_b.Put(syncable::IS_DIR, true);
    folder_b_id = folder_b.Get(syncable::META_HANDLE);

    syncable::MutableEntry child(&trans, syncable::CREATE,
                                 BOOKMARKS, folder_a.Get(syncable::ID),
                                 "child");
    ASSERT_TRUE(child.good());
    SetNodeProperties(&child);
    child_id = child.Get(syncable::META_HANDLE);
  }

  // Close that transaction.  The above was to setup the initial scenario.  The
  // real test starts now.

  // Move the child from folder A to folder B.
  {
    syncable::WriteTransaction trans(
        FROM_HERE, syncable::SYNCER, share()->directory.get());

    syncable::Entry folder_b(&trans, syncable::GET_BY_HANDLE, folder_b_id);
    syncable::MutableEntry child(&trans, syncable::GET_BY_HANDLE, child_id);

    child.Put(syncable::PARENT_ID, folder_b.Get(syncable::ID));
  }

  EXPECT_EQ(1UL, GetChangeListSize());

  // Verify that this was detected as a real change.  An early version of the
  // UniquePosition code had a bug where moves from one folder to another were
  // ignored unless the moved node's UniquePosition value was also changed in
  // some way.
  FindChangeInList(child_id, ChangeRecord::ACTION_UPDATE);
}

// Test moving a bookmark into a non-empty folder.
TEST_F(SyncManagerChangeProcessingTest, MoveIntoPopulatedFolder) {
  int64 type_root = GetIdForDataType(BOOKMARKS);
  int64 child_a_id = kInvalidId;
  int64 child_b_id = kInvalidId;

  // Create two folders.  Place one child each under folder A and folder B.
  {
    syncable::WriteTransaction trans(
        FROM_HERE, syncable::SYNCER, share()->directory.get());
    syncable::Entry root(&trans, syncable::GET_BY_HANDLE, type_root);
    ASSERT_TRUE(root.good());

    syncable::MutableEntry folder_a(&trans, syncable::CREATE,
                                  BOOKMARKS, root.Get(syncable::ID), "folderA");
    ASSERT_TRUE(folder_a.good());
    SetNodeProperties(&folder_a);
    folder_a.Put(syncable::IS_DIR, true);

    syncable::MutableEntry folder_b(&trans, syncable::CREATE,
                                  BOOKMARKS, root.Get(syncable::ID), "folderB");
    ASSERT_TRUE(folder_b.good());
    SetNodeProperties(&folder_b);
    folder_b.Put(syncable::IS_DIR, true);

    syncable::MutableEntry child_a(&trans, syncable::CREATE,
                                   BOOKMARKS, folder_a.Get(syncable::ID),
                                   "childA");
    ASSERT_TRUE(child_a.good());
    SetNodeProperties(&child_a);
    child_a_id = child_a.Get(syncable::META_HANDLE);

    syncable::MutableEntry child_b(&trans, syncable::CREATE,
                                   BOOKMARKS, folder_b.Get(syncable::ID),
                                   "childB");
    SetNodeProperties(&child_b);
    child_b_id = child_b.Get(syncable::META_HANDLE);

  }

  // Close that transaction.  The above was to setup the initial scenario.  The
  // real test starts now.

  {
    syncable::WriteTransaction trans(
        FROM_HERE, syncable::SYNCER, share()->directory.get());

    syncable::MutableEntry child_a(&trans, syncable::GET_BY_HANDLE, child_a_id);
    syncable::MutableEntry child_b(&trans, syncable::GET_BY_HANDLE, child_b_id);

    // Move child A from folder A to folder B and update its position.
    child_a.Put(syncable::PARENT_ID, child_b.Get(syncable::PARENT_ID));
    child_a.PutPredecessor(child_b.Get(syncable::ID));
  }

  EXPECT_EQ(2UL, GetChangeListSize());

  // Verify that both child A and child B are in the change list, even though
  // only child A was moved.  The rules state that all siblings of
  // position-modified items must be included in the list of changes.
  int64 child_a_pos = FindChangeInList(child_a_id, ChangeRecord::ACTION_UPDATE);
  int64 child_b_pos = FindChangeInList(child_b_id, ChangeRecord::ACTION_UPDATE);

  // Siblings should appear in left-to-right order.
  EXPECT_LT(child_b_pos, child_a_pos);
}

// Tests the ordering of deletion changes.
TEST_F(SyncManagerChangeProcessingTest, DeletionsAndChanges) {
  int64 type_root = GetIdForDataType(BOOKMARKS);
  int64 folder_a_id = kInvalidId;
  int64 folder_b_id = kInvalidId;
  int64 child_id = kInvalidId;

  // Create two folders.  Place a child under folder A.
  {
    syncable::WriteTransaction trans(
        FROM_HERE, syncable::SYNCER, share()->directory.get());
    syncable::Entry root(&trans, syncable::GET_BY_HANDLE, type_root);
    ASSERT_TRUE(root.good());

    syncable::MutableEntry folder_a(&trans, syncable::CREATE,
                                  BOOKMARKS, root.Get(syncable::ID), "folderA");
    ASSERT_TRUE(folder_a.good());
    SetNodeProperties(&folder_a);
    folder_a.Put(syncable::IS_DIR, true);
    folder_a_id = folder_a.Get(syncable::META_HANDLE);

    syncable::MutableEntry folder_b(&trans, syncable::CREATE,
                                  BOOKMARKS, root.Get(syncable::ID), "folderB");
    ASSERT_TRUE(folder_b.good());
    SetNodeProperties(&folder_b);
    folder_b.Put(syncable::IS_DIR, true);
    folder_b_id = folder_b.Get(syncable::META_HANDLE);

    syncable::MutableEntry child(&trans, syncable::CREATE,
                                 BOOKMARKS, folder_a.Get(syncable::ID),
                                 "child");
    ASSERT_TRUE(child.good());
    SetNodeProperties(&child);
    child_id = child.Get(syncable::META_HANDLE);
  }

  // Close that transaction.  The above was to setup the initial scenario.  The
  // real test starts now.

  {
    syncable::WriteTransaction trans(
        FROM_HERE, syncable::SYNCER, share()->directory.get());

    syncable::MutableEntry folder_a(
        &trans, syncable::GET_BY_HANDLE, folder_a_id);
    syncable::MutableEntry folder_b(
        &trans, syncable::GET_BY_HANDLE, folder_b_id);
    syncable::MutableEntry child(&trans, syncable::GET_BY_HANDLE, child_id);

    // Delete folder B and its child.
    child.Put(syncable::IS_DEL, true);
    folder_b.Put(syncable::IS_DEL, true);

    // Make an unrelated change to folder A.
    folder_a.Put(syncable::NON_UNIQUE_NAME, "NewNameA");
  }

  EXPECT_EQ(3UL, GetChangeListSize());

  size_t folder_a_pos =
      FindChangeInList(folder_a_id, ChangeRecord::ACTION_UPDATE);
  size_t folder_b_pos =
      FindChangeInList(folder_b_id, ChangeRecord::ACTION_DELETE);
  size_t child_pos = FindChangeInList(child_id, ChangeRecord::ACTION_DELETE);

  // Deletes should appear before updates.
  EXPECT_LT(child_pos, folder_a_pos);
  EXPECT_LT(folder_b_pos, folder_a_pos);
}

}  // namespace
