// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/syncer_proto_util.h"

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/sync_enums.pb.h"
#include "sync/sessions/sync_session_context.h"
#include "sync/syncable/blob.h"
#include "sync/syncable/directory.h"
#include "sync/test/engine/mock_connection_manager.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

using sync_pb::ClientToServerMessage;
using sync_pb::CommitResponse_EntryResponse;
using sync_pb::SyncEntity;

namespace syncer {

using sessions::SyncSessionContext;
using syncable::Blob;

class MockDelegate : public sessions::SyncSession::Delegate {
 public:
   MockDelegate() {}
   ~MockDelegate() {}

  MOCK_METHOD1(OnReceivedShortPollIntervalUpdate, void(const base::TimeDelta&));
  MOCK_METHOD1(OnReceivedLongPollIntervalUpdate ,void(const base::TimeDelta&));
  MOCK_METHOD1(OnReceivedSessionsCommitDelay, void(const base::TimeDelta&));
  MOCK_METHOD1(OnReceivedClientInvalidationHintBufferSize, void(int));
  MOCK_METHOD1(OnSyncProtocolError, void(const SyncProtocolError&));
};

// Builds a ClientToServerResponse with some data type ids, including
// invalid ones.  GetTypesToMigrate() should return only the valid
// model types.
TEST(SyncerProtoUtil, GetTypesToMigrate) {
  sync_pb::ClientToServerResponse response;
  response.add_migrated_data_type_id(
      GetSpecificsFieldNumberFromModelType(BOOKMARKS));
  response.add_migrated_data_type_id(
      GetSpecificsFieldNumberFromModelType(HISTORY_DELETE_DIRECTIVES));
  response.add_migrated_data_type_id(-1);
  EXPECT_TRUE(
      GetTypesToMigrate(response).Equals(
          ModelTypeSet(BOOKMARKS, HISTORY_DELETE_DIRECTIVES)));
}

// Builds a ClientToServerResponse_Error with some error data type
// ids, including invalid ones.  ConvertErrorPBToLocalType() should
// return a SyncProtocolError with only the valid model types.
TEST(SyncerProtoUtil, ConvertErrorPBToLocalType) {
  sync_pb::ClientToServerResponse_Error error_pb;
  error_pb.set_error_type(sync_pb::SyncEnums::THROTTLED);
  error_pb.add_error_data_type_ids(
      GetSpecificsFieldNumberFromModelType(BOOKMARKS));
  error_pb.add_error_data_type_ids(
      GetSpecificsFieldNumberFromModelType(HISTORY_DELETE_DIRECTIVES));
  error_pb.add_error_data_type_ids(-1);
  SyncProtocolError error = ConvertErrorPBToLocalType(error_pb);
  EXPECT_TRUE(
      error.error_data_types.Equals(
          ModelTypeSet(BOOKMARKS, HISTORY_DELETE_DIRECTIVES)));
}

TEST(SyncerProtoUtil, TestBlobToProtocolBufferBytesUtilityFunctions) {
  unsigned char test_data1[] = {1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 4, 2, 9};
  unsigned char test_data2[] = {1, 99, 3, 4, 5, 6, 7, 8, 0, 1, 4, 2, 9};
  unsigned char test_data3[] = {99, 2, 3, 4, 5, 6, 7, 8};

  syncable::Blob test_blob1, test_blob2, test_blob3;
  for (size_t i = 0; i < arraysize(test_data1); ++i)
    test_blob1.push_back(test_data1[i]);
  for (size_t i = 0; i < arraysize(test_data2); ++i)
    test_blob2.push_back(test_data2[i]);
  for (size_t i = 0; i < arraysize(test_data3); ++i)
    test_blob3.push_back(test_data3[i]);

  std::string test_message1(reinterpret_cast<char*>(test_data1),
      arraysize(test_data1));
  std::string test_message2(reinterpret_cast<char*>(test_data2),
      arraysize(test_data2));
  std::string test_message3(reinterpret_cast<char*>(test_data3),
      arraysize(test_data3));

  EXPECT_TRUE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message1,
                                                    test_blob1));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message1,
                                                     test_blob2));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message1,
                                                     test_blob3));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message2,
                                                     test_blob1));
  EXPECT_TRUE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message2,
                                                    test_blob2));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message2,
                                                     test_blob3));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message3,
                                                     test_blob1));
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message3,
                                                     test_blob2));
  EXPECT_TRUE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message3,
                                                    test_blob3));

  Blob blob1_copy;
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message1,
                                                     blob1_copy));
  SyncerProtoUtil::CopyProtoBytesIntoBlob(test_message1, &blob1_copy);
  EXPECT_TRUE(SyncerProtoUtil::ProtoBytesEqualsBlob(test_message1,
                                                    blob1_copy));

  std::string message2_copy;
  EXPECT_FALSE(SyncerProtoUtil::ProtoBytesEqualsBlob(message2_copy,
                                                     test_blob2));
  SyncerProtoUtil::CopyBlobIntoProtoBytes(test_blob2, &message2_copy);
  EXPECT_TRUE(SyncerProtoUtil::ProtoBytesEqualsBlob(message2_copy,
                                                    test_blob2));
}

// Tests NameFromSyncEntity and NameFromCommitEntryResponse when only the name
// field is provided.
TEST(SyncerProtoUtil, NameExtractionOneName) {
  SyncEntity one_name_entity;
  CommitResponse_EntryResponse one_name_response;

  const std::string one_name_string("Eggheadednesses");
  one_name_entity.set_name(one_name_string);
  one_name_response.set_name(one_name_string);

  const std::string name_a =
      SyncerProtoUtil::NameFromSyncEntity(one_name_entity);
  EXPECT_EQ(one_name_string, name_a);
}

TEST(SyncerProtoUtil, NameExtractionOneUniqueName) {
  SyncEntity one_name_entity;
  CommitResponse_EntryResponse one_name_response;

  const std::string one_name_string("Eggheadednesses");

  one_name_entity.set_non_unique_name(one_name_string);
  one_name_response.set_non_unique_name(one_name_string);

  const std::string name_a =
      SyncerProtoUtil::NameFromSyncEntity(one_name_entity);
  EXPECT_EQ(one_name_string, name_a);
}

// Tests NameFromSyncEntity and NameFromCommitEntryResponse when both the name
// field and the non_unique_name fields are provided.
// Should prioritize non_unique_name.
TEST(SyncerProtoUtil, NameExtractionTwoNames) {
  SyncEntity two_name_entity;
  CommitResponse_EntryResponse two_name_response;

  const std::string neuro("Neuroanatomists");
  const std::string oxyphen("Oxyphenbutazone");

  two_name_entity.set_name(oxyphen);
  two_name_entity.set_non_unique_name(neuro);

  two_name_response.set_name(oxyphen);
  two_name_response.set_non_unique_name(neuro);

  const std::string name_a =
      SyncerProtoUtil::NameFromSyncEntity(two_name_entity);
  EXPECT_EQ(neuro, name_a);
}

class SyncerProtoUtilTest : public testing::Test {
 public:
  virtual void SetUp() {
    dir_maker_.SetUp();
  }

  virtual void TearDown() {
    dir_maker_.TearDown();
  }

  syncable::Directory* directory() {
    return dir_maker_.directory();
  }

 protected:
  base::MessageLoop message_loop_;
  TestDirectorySetterUpper dir_maker_;
};

TEST_F(SyncerProtoUtilTest, VerifyResponseBirthday) {
  // Both sides empty
  EXPECT_TRUE(directory()->store_birthday().empty());
  sync_pb::ClientToServerResponse response;
  EXPECT_FALSE(SyncerProtoUtil::VerifyResponseBirthday(response, directory()));

  // Remote set, local empty
  response.set_store_birthday("flan");
  EXPECT_TRUE(SyncerProtoUtil::VerifyResponseBirthday(response, directory()));
  EXPECT_EQ(directory()->store_birthday(), "flan");

  // Remote empty, local set.
  response.clear_store_birthday();
  EXPECT_TRUE(SyncerProtoUtil::VerifyResponseBirthday(response, directory()));
  EXPECT_EQ(directory()->store_birthday(), "flan");

  // Doesn't match
  response.set_store_birthday("meat");
  EXPECT_FALSE(SyncerProtoUtil::VerifyResponseBirthday(response, directory()));

  response.set_error_code(sync_pb::SyncEnums::CLEAR_PENDING);
  EXPECT_FALSE(SyncerProtoUtil::VerifyResponseBirthday(response, directory()));
}

TEST_F(SyncerProtoUtilTest, VerifyDisabledByAdmin) {
  // No error code
  sync_pb::ClientToServerResponse response;
  EXPECT_FALSE(SyncerProtoUtil::IsSyncDisabledByAdmin(response));

  // Has error code, but not disabled
  response.set_error_code(sync_pb::SyncEnums::NOT_MY_BIRTHDAY);
  EXPECT_FALSE(SyncerProtoUtil::IsSyncDisabledByAdmin(response));

  // Has error code, and is disabled by admin
  response.set_error_code(sync_pb::SyncEnums::DISABLED_BY_ADMIN);
  EXPECT_TRUE(SyncerProtoUtil::IsSyncDisabledByAdmin(response));
}

TEST_F(SyncerProtoUtilTest, AddRequestBirthday) {
  EXPECT_TRUE(directory()->store_birthday().empty());
  ClientToServerMessage msg;
  SyncerProtoUtil::AddRequestBirthday(directory(), &msg);
  EXPECT_FALSE(msg.has_store_birthday());

  directory()->set_store_birthday("meat");
  SyncerProtoUtil::AddRequestBirthday(directory(), &msg);
  EXPECT_EQ(msg.store_birthday(), "meat");
}

class DummyConnectionManager : public ServerConnectionManager {
 public:
  DummyConnectionManager(CancelationSignal* signal)
      : ServerConnectionManager("unused", 0, false, signal),
        send_error_(false),
        access_denied_(false) {}

  virtual ~DummyConnectionManager() {}
  virtual bool PostBufferWithCachedAuth(
      PostBufferParams* params,
      ScopedServerStatusWatcher* watcher) OVERRIDE {
    if (send_error_) {
      return false;
    }

    sync_pb::ClientToServerResponse response;
    if (access_denied_) {
      response.set_error_code(sync_pb::SyncEnums::ACCESS_DENIED);
    }
    response.SerializeToString(&params->buffer_out);

    return true;
  }

  void set_send_error(bool send) {
    send_error_ = send;
  }

  void set_access_denied(bool denied) {
    access_denied_ = denied;
  }

 private:
  bool send_error_;
  bool access_denied_;
};

TEST_F(SyncerProtoUtilTest, PostAndProcessHeaders) {
  CancelationSignal signal;
  DummyConnectionManager dcm(&signal);
  ClientToServerMessage msg;
  SyncerProtoUtil::SetProtocolVersion(&msg);
  msg.set_share("required");
  msg.set_message_contents(ClientToServerMessage::GET_UPDATES);
  sync_pb::ClientToServerResponse response;

  dcm.set_send_error(true);
  EXPECT_FALSE(SyncerProtoUtil::PostAndProcessHeaders(&dcm, NULL,
      msg, &response));

  dcm.set_send_error(false);
  EXPECT_TRUE(SyncerProtoUtil::PostAndProcessHeaders(&dcm, NULL,
      msg, &response));

  dcm.set_access_denied(true);
  EXPECT_FALSE(SyncerProtoUtil::PostAndProcessHeaders(&dcm, NULL,
      msg, &response));
}

}  // namespace syncer
