// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/non_blocking_type_commit_contribution.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/hash/sha1.h"
#include "base/test/mock_callback.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/syncable/directory_cryptographer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using sync_pb::CommitResponse;
using sync_pb::EntitySpecifics;
using sync_pb::SharingMessageCommitError;
using sync_pb::SyncEntity;

const ClientTagHash kTag = ClientTagHash::FromHashed("tag");
const char kValue[] = "value";
const char kURL[] = "url";
const char kTitle[] = "title";

EntitySpecifics GeneratePreferenceSpecifics(const ClientTagHash& tag,
                                            const std::string& value) {
  EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag.value());
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

EntitySpecifics GenerateBookmarkSpecifics(const std::string& url,
                                          const std::string& title) {
  EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_url(url);
  specifics.mutable_bookmark()->set_legacy_canonicalized_title(title);
  return specifics;
}

TEST(NonBlockingTypeCommitContributionTest, PopulateCommitProtoDefault) {
  const int64_t kBaseVersion = 7;
  base::Time creation_time =
      base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  base::Time modification_time =
      creation_time + base::TimeDelta::FromSeconds(1);

  auto data = std::make_unique<syncer::EntityData>();

  data->client_tag_hash = kTag;
  data->specifics = GeneratePreferenceSpecifics(kTag, kValue);

  // These fields are not really used for much, but we set them anyway
  // to make this item look more realistic.
  data->creation_time = creation_time;
  data->modification_time = modification_time;
  data->name = "Name:";

  CommitRequestData request_data;
  request_data.sequence_number = 2;
  request_data.base_version = kBaseVersion;
  base::Base64Encode(base::SHA1HashString(data->specifics.SerializeAsString()),
                     &request_data.specifics_hash);
  request_data.entity = std::move(data);

  SyncEntity entity;
  NonBlockingTypeCommitContribution::PopulateCommitProto(PREFERENCES,
                                                         request_data, &entity);

  // Exhaustively verify the populated SyncEntity.
  EXPECT_TRUE(entity.id_string().empty());
  EXPECT_EQ(7, entity.version());
  EXPECT_EQ(modification_time.ToJsTime(), entity.mtime());
  EXPECT_EQ(creation_time.ToJsTime(), entity.ctime());
  EXPECT_FALSE(entity.name().empty());
  EXPECT_FALSE(entity.client_defined_unique_tag().empty());
  EXPECT_EQ(kTag.value(), entity.specifics().preference().name());
  EXPECT_FALSE(entity.deleted());
  EXPECT_EQ(kValue, entity.specifics().preference().value());
  EXPECT_TRUE(entity.parent_id_string().empty());
  EXPECT_FALSE(entity.unique_position().has_custom_compressed_v1());
  EXPECT_EQ(0, entity.position_in_parent());
}

TEST(NonBlockingTypeCommitContributionTest, PopulateCommitProtoBookmark) {
  const int64_t kBaseVersion = 7;
  base::Time creation_time =
      base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  base::Time modification_time =
      creation_time + base::TimeDelta::FromSeconds(1);

  auto data = std::make_unique<syncer::EntityData>();

  data->id = "bookmark";
  data->specifics = GenerateBookmarkSpecifics(kURL, kTitle);

  // These fields are not really used for much, but we set them anyway
  // to make this item look more realistic.
  data->creation_time = creation_time;
  data->modification_time = modification_time;
  data->name = "Name:";
  data->parent_id = "ParentOf:";
  data->is_folder = true;
  syncer::UniquePosition uniquePosition = syncer::UniquePosition::FromInt64(
      10, syncer::UniquePosition::RandomSuffix());
  data->unique_position = uniquePosition.ToProto();

  CommitRequestData request_data;
  request_data.sequence_number = 2;
  request_data.base_version = kBaseVersion;
  base::Base64Encode(base::SHA1HashString(data->specifics.SerializeAsString()),
                     &request_data.specifics_hash);
  request_data.entity = std::move(data);

  SyncEntity entity;
  NonBlockingTypeCommitContribution::PopulateCommitProto(BOOKMARKS,
                                                         request_data, &entity);

  // Exhaustively verify the populated SyncEntity.
  EXPECT_FALSE(entity.id_string().empty());
  EXPECT_EQ(7, entity.version());
  EXPECT_EQ(modification_time.ToJsTime(), entity.mtime());
  EXPECT_EQ(creation_time.ToJsTime(), entity.ctime());
  EXPECT_FALSE(entity.name().empty());
  EXPECT_TRUE(entity.client_defined_unique_tag().empty());
  EXPECT_EQ(kURL, entity.specifics().bookmark().url());
  EXPECT_FALSE(entity.deleted());
  EXPECT_EQ(kTitle, entity.specifics().bookmark().legacy_canonicalized_title());
  EXPECT_TRUE(entity.folder());
  EXPECT_FALSE(entity.parent_id_string().empty());
  EXPECT_TRUE(entity.unique_position().has_custom_compressed_v1());
  EXPECT_NE(0, entity.position_in_parent());
}

// Verifies how PASSWORDS protos are committed on the wire, making sure the data
// is properly encrypted except for password metadata.
TEST(NonBlockingTypeCommitContributionTest,
     PopulateCommitProtoPasswordWithoutCustomPassphrase) {
  const std::string kMetadataUrl = "http://foo.com";
  const std::string kSignonRealm = "signon_realm";
  const int64_t kBaseVersion = 7;

  auto data = std::make_unique<syncer::EntityData>();
  data->client_tag_hash = kTag;
  sync_pb::PasswordSpecificsData* password_data =
      data->specifics.mutable_password()->mutable_client_only_encrypted_data();
  password_data->set_signon_realm(kSignonRealm);

  data->specifics.mutable_password()->mutable_unencrypted_metadata()->set_url(
      kMetadataUrl);

  auto request_data = std::make_unique<CommitRequestData>();
  request_data->sequence_number = 2;
  request_data->base_version = kBaseVersion;
  base::Base64Encode(base::SHA1HashString(data->specifics.SerializeAsString()),
                     &request_data->specifics_hash);
  request_data->entity = std::move(data);

  base::ObserverList<TypeDebugInfoObserver>::Unchecked observers;
  DataTypeDebugInfoEmitter debug_info_emitter(PASSWORDS, &observers);

  DirectoryCryptographer cryptographer;
  cryptographer.AddKey({KeyDerivationParams::CreateForPbkdf2(), "dummy"});

  CommitRequestDataList requests_data;
  requests_data.push_back(std::move(request_data));
  NonBlockingTypeCommitContribution contribution(
      PASSWORDS, sync_pb::DataTypeContext(), std::move(requests_data),
      /*on_commit_response_callback=*/base::NullCallback(),
      /*on_full_commit_failure_callback=*/base::NullCallback(), &cryptographer,
      PassphraseType::kImplicitPassphrase, &debug_info_emitter,
      /*only_commit_specifics=*/false);

  sync_pb::ClientToServerMessage msg;
  contribution.AddToCommitMessage(&msg);
  contribution.CleanUp();

  ASSERT_EQ(1, msg.commit().entries().size());
  SyncEntity entity = msg.commit().entries(0);

  // Exhaustively verify the populated SyncEntity.
  EXPECT_TRUE(entity.id_string().empty());
  EXPECT_EQ(7, entity.version());
  EXPECT_EQ("encrypted", entity.name());
  EXPECT_EQ(kTag.value(), entity.client_defined_unique_tag());
  EXPECT_FALSE(entity.deleted());
  EXPECT_FALSE(entity.specifics().has_encrypted());
  EXPECT_TRUE(entity.specifics().has_password());
  EXPECT_EQ(kSignonRealm,
            entity.specifics().password().unencrypted_metadata().url());
  EXPECT_TRUE(
      entity.specifics().password().unencrypted_metadata().has_blacklisted());
  EXPECT_FALSE(
      entity.specifics().password().unencrypted_metadata().blacklisted());
  EXPECT_FALSE(entity.specifics().password().encrypted().blob().empty());
  EXPECT_TRUE(entity.parent_id_string().empty());
  EXPECT_FALSE(entity.unique_position().has_custom_compressed_v1());
  EXPECT_EQ(0, entity.position_in_parent());
}

// Same as above but uses CUSTOM_PASSPHRASE. In this case, field
// |unencrypted_metadata| should be cleared.
TEST(NonBlockingTypeCommitContributionTest,
     PopulateCommitProtoPasswordWithCustomPassphrase) {
  const std::string kMetadataUrl = "http://foo.com";
  const std::string kSignonRealm = "signon_realm";
  const int64_t kBaseVersion = 7;

  auto data = std::make_unique<syncer::EntityData>();
  data->client_tag_hash = kTag;
  sync_pb::PasswordSpecificsData* password_data =
      data->specifics.mutable_password()->mutable_client_only_encrypted_data();
  password_data->set_signon_realm(kSignonRealm);

  data->specifics.mutable_password()->mutable_unencrypted_metadata()->set_url(
      kMetadataUrl);

  auto request_data = std::make_unique<CommitRequestData>();
  request_data->sequence_number = 2;
  request_data->base_version = kBaseVersion;
  base::Base64Encode(base::SHA1HashString(data->specifics.SerializeAsString()),
                     &request_data->specifics_hash);
  request_data->entity = std::move(data);

  base::ObserverList<TypeDebugInfoObserver>::Unchecked observers;
  DataTypeDebugInfoEmitter debug_info_emitter(PASSWORDS, &observers);

  DirectoryCryptographer cryptographer;
  cryptographer.AddKey({KeyDerivationParams::CreateForPbkdf2(), "dummy"});

  CommitRequestDataList requests_data;
  requests_data.push_back(std::move(request_data));
  NonBlockingTypeCommitContribution contribution(
      PASSWORDS, sync_pb::DataTypeContext(), std::move(requests_data),
      /*on_commit_response_callback=*/base::NullCallback(),
      /*on_full_commit_failure_callback=*/base::NullCallback(), &cryptographer,
      PassphraseType::kCustomPassphrase, &debug_info_emitter,
      /*only_commit_specifics=*/false);

  sync_pb::ClientToServerMessage msg;
  contribution.AddToCommitMessage(&msg);
  contribution.CleanUp();

  ASSERT_EQ(1, msg.commit().entries().size());
  SyncEntity entity = msg.commit().entries(0);

  // Exhaustively verify the populated SyncEntity.
  EXPECT_TRUE(entity.id_string().empty());
  EXPECT_EQ(7, entity.version());
  EXPECT_EQ("encrypted", entity.name());
  EXPECT_EQ(kTag.value(), entity.client_defined_unique_tag());
  EXPECT_FALSE(entity.deleted());
  EXPECT_FALSE(entity.specifics().has_encrypted());
  EXPECT_TRUE(entity.specifics().has_password());
  EXPECT_FALSE(entity.specifics().password().encrypted().blob().empty());
  EXPECT_FALSE(entity.specifics().password().has_unencrypted_metadata());
  EXPECT_TRUE(entity.parent_id_string().empty());
  EXPECT_FALSE(entity.unique_position().has_custom_compressed_v1());
  EXPECT_EQ(0, entity.position_in_parent());
}

TEST(NonBlockingTypeCommitContributionTest,
     ShouldPropagateFailedItemsOnCommitResponse) {
  auto data = std::make_unique<syncer::EntityData>();
  data->client_tag_hash = ClientTagHash::FromHashed("hash");
  auto request_data = std::make_unique<CommitRequestData>();
  request_data->entity = std::move(data);
  CommitRequestDataList requests_data;
  requests_data.push_back(std::move(request_data));

  base::ObserverList<TypeDebugInfoObserver>::Unchecked observers;
  DataTypeDebugInfoEmitter debug_info_emitter(PASSWORDS, &observers);

  DirectoryCryptographer cryptographer;

  FailedCommitResponseDataList actual_error_response_list;

  auto on_commit_response_callback = base::BindOnce(
      [](FailedCommitResponseDataList* actual_error_response_list,
         const CommitResponseDataList& committed_response_list,
         const FailedCommitResponseDataList& error_response_list) {
        // We put expectations outside of the callback, so that they fail if
        // callback is not ran.
        *actual_error_response_list = error_response_list;
      },
      &actual_error_response_list);

  NonBlockingTypeCommitContribution contribution(
      PASSWORDS, sync_pb::DataTypeContext(), std::move(requests_data),
      std::move(on_commit_response_callback),
      /*on_full_commit_failure_callback=*/base::NullCallback(), &cryptographer,
      PassphraseType::kCustomPassphrase, &debug_info_emitter,
      /*only_commit_specifics=*/false);

  sync_pb::ClientToServerMessage msg;
  contribution.AddToCommitMessage(&msg);

  sync_pb::ClientToServerResponse response;
  sync_pb::CommitResponse* commit_response = response.mutable_commit();

  {
    sync_pb::CommitResponse_EntryResponse* entry =
        commit_response->add_entryresponse();
    entry->set_response_type(CommitResponse::TRANSIENT_ERROR);
    SharingMessageCommitError* sharing_message_error =
        entry->mutable_datatype_specific_error()
            ->mutable_sharing_message_error();
    sharing_message_error->set_error_code(
        SharingMessageCommitError::INVALID_ARGUMENT);
  }

  StatusController status;
  contribution.ProcessCommitResponse(response, &status);
  contribution.CleanUp();

  ASSERT_EQ(1u, actual_error_response_list.size());
  FailedCommitResponseData failed_item = actual_error_response_list[0];
  EXPECT_EQ(ClientTagHash::FromHashed("hash"), failed_item.client_tag_hash);
  EXPECT_EQ(CommitResponse::TRANSIENT_ERROR, failed_item.response_type);
  EXPECT_EQ(
      SharingMessageCommitError::INVALID_ARGUMENT,
      failed_item.datatype_specific_error.sharing_message_error().error_code());
}

TEST(NonBlockingTypeCommitContributionTest, ShouldPropagateFullCommitFailure) {
  DirectoryCryptographer cryptographer;
  base::ObserverList<TypeDebugInfoObserver>::Unchecked observers;
  DataTypeDebugInfoEmitter debug_info_emitter(PASSWORDS, &observers);

  base::MockOnceCallback<void(SyncCommitError commit_error)>
      on_commit_failure_callback;
  EXPECT_CALL(on_commit_failure_callback, Run(SyncCommitError::kNetworkError));

  NonBlockingTypeCommitContribution contribution(
      PASSWORDS, sync_pb::DataTypeContext(), CommitRequestDataList(),
      /*on_commit_response_callback=*/base::NullCallback(),
      on_commit_failure_callback.Get(), &cryptographer,
      PassphraseType::kCustomPassphrase, &debug_info_emitter,
      /*only_commit_specifics=*/false);

  contribution.ProcessCommitFailure(SyncCommitError::kNetworkError);
  contribution.CleanUp();
}

}  // namespace

}  // namespace syncer
