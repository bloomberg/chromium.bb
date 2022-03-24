// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"

#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/protocol/vault.pb.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/sync/trusted_vault/trusted_vault_crypto.h"
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::Eq;
using testing::IsNull;
using testing::Ne;
using testing::NotNull;
using testing::SizeIs;

const char kAccessToken[] = "access_token";
const char kEncodedPrivateKey[] =
    "49e052293c29b5a50b0013eec9d030ac2ad70a42fe093be084264647cb04e16f";

MATCHER_P2(TrustedVaultKeyAndVersionEq, expected_key, expected_version, "") {
  const TrustedVaultKeyAndVersion& key_and_version = arg;
  return key_and_version.key == expected_key &&
         key_and_version.version == expected_version;
}

std::unique_ptr<SecureBoxKeyPair> MakeTestKeyPair() {
  std::vector<uint8_t> private_key_bytes;
  bool success = base::HexStringToBytes(kEncodedPrivateKey, &private_key_bytes);
  DCHECK(success);
  return SecureBoxKeyPair::CreateByPrivateKeyImport(private_key_bytes);
}

sync_pb::SecurityDomain MakeSecurityDomainWithDegradedRecoverability(
    bool recoverability_degraded) {
  sync_pb::SecurityDomain security_domain;
  security_domain.set_name(kSyncSecurityDomainName);
  security_domain.mutable_security_domain_details()
      ->mutable_sync_details()
      ->set_degraded_recoverability(recoverability_degraded);
  return security_domain;
}

sync_pb::JoinSecurityDomainsResponse MakeJoinSecurityDomainsResponse(
    int current_epoch) {
  sync_pb::JoinSecurityDomainsResponse response;
  sync_pb::SecurityDomain* security_domain = response.mutable_security_domain();
  security_domain->set_name(kSyncSecurityDomainName);
  security_domain->set_current_epoch(current_epoch);
  return response;
}

class FakeTrustedVaultAccessTokenFetcher
    : public TrustedVaultAccessTokenFetcher {
 public:
  explicit FakeTrustedVaultAccessTokenFetcher(
      const absl::optional<std::string>& access_token)
      : access_token_(access_token) {}
  ~FakeTrustedVaultAccessTokenFetcher() override = default;

  void FetchAccessToken(const CoreAccountId& account_id,
                        TokenCallback callback) override {
    absl::optional<signin::AccessTokenInfo> access_token_info;
    if (access_token_) {
      access_token_info = signin::AccessTokenInfo(
          *access_token_,
          /*expiration_time_param=*/base::Time::Now() + base::Hours(1),
          /*id_token=*/std::string());
    }
    std::move(callback).Run(access_token_info);
  }

 private:
  const absl::optional<std::string> access_token_;
};

// TODO(crbug.com/1113598): revisit this tests suite and determine what actually
// should be tested on the Connection level and what should be done on lower
// levels (DownloadKeysResponseHandler and RegisterAuthenticationFactorRequest).
class TrustedVaultConnectionImplTest : public testing::Test {
 public:
  TrustedVaultConnectionImplTest()
      : connection_(
            kTestURL,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)
                ->Clone(),
            std::make_unique<FakeTrustedVaultAccessTokenFetcher>(
                kAccessToken)) {}

  ~TrustedVaultConnectionImplTest() override = default;

  TrustedVaultConnectionImpl* connection() { return &connection_; }

  // Allows overloading of FakeTrustedVaultAccessTokenFetcher behavior, doesn't
  // overwrite connection().
  std::unique_ptr<TrustedVaultConnectionImpl> CreateConnectionWithAccessToken(
      absl::optional<std::string> access_token) {
    return std::make_unique<TrustedVaultConnectionImpl>(
        kTestURL,
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_)
            ->Clone(),
        std::make_unique<FakeTrustedVaultAccessTokenFetcher>(access_token));
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingHTTPRequest() {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.GetPendingRequest(/*index=*/0);
  }

  bool RespondToJoinSecurityDomainsRequest(
      net::HttpStatusCode response_http_code,
      const std::string& response_content) {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullJoinSecurityDomainsURLForTesting(kTestURL).spec(),
        response_content, response_http_code);
  }

  bool RespondToGetSecurityDomainMemberRequest(
      net::HttpStatusCode response_http_code) {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullGetSecurityDomainMemberURLForTesting(
            kTestURL, MakeTestKeyPair()->public_key().ExportToBytes())
            .spec(),
        /*content=*/std::string(), response_http_code);
  }

  bool RespondToGetSecurityDomainRequest(net::HttpStatusCode response_http_code,
                                         const std::string& response_body) {
    // Allow request to reach |test_url_loader_factory_|.
    base::RunLoop().RunUntilIdle();
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFullGetSecurityDomainURLForTesting(kTestURL).spec(), response_body,
        response_http_code);
  }

  const std::vector<std::vector<uint8_t>> kTrustedVaultKeys = {{1, 2},
                                                               {1, 2, 3, 4}};
  const GURL kTestURL = GURL("https://test.com/test");

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  TrustedVaultConnectionImpl connection_;
};

TEST_F(TrustedVaultConnectionImplTest,
       ShouldSendJoinSecurityDomainsRequestWithoutKeys) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterDeviceWithoutKeys(
          /*account_info=*/CoreAccountInfo(), key_pair->public_key(),
          TrustedVaultConnection::RegisterDeviceWithoutKeysCallback());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url,
              Eq(GetFullJoinSecurityDomainsURLForTesting(kTestURL)));

  sync_pb::JoinSecurityDomainsRequest deserialized_body;
  EXPECT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.security_domain().name(),
              Eq(kSyncSecurityDomainName));

  std::string public_key_string;
  AssignBytesToProtoString(key_pair->public_key().ExportToBytes(),
                           &public_key_string);

  std::string encoded_public_key;
  base::Base64UrlEncode(public_key_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);

  const sync_pb::SecurityDomainMember& member =
      deserialized_body.security_domain_member();
  EXPECT_THAT(member.name(),
              Eq(kSecurityDomainMemberNamePrefix + encoded_public_key));
  EXPECT_THAT(member.public_key(), Eq(public_key_string));
  EXPECT_THAT(member.member_type(),
              Eq(sync_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE));

  // Constant key with |epoch| set to kUnknownConstantKeyVersion must be sent.
  ASSERT_THAT(deserialized_body.shared_member_key(), SizeIs(1));
  const sync_pb::SharedMemberKey& shared_key =
      deserialized_body.shared_member_key(0);
  EXPECT_THAT(shared_key.epoch(), Eq(0));

  EXPECT_THAT(DecryptTrustedVaultWrappedKey(
                  key_pair->private_key(),
                  /*wrapped_key=*/ProtoStringToBytes(shared_key.wrapped_key())),
              Eq(GetConstantTrustedVaultKey()));
  EXPECT_TRUE(VerifyMemberProof(key_pair->public_key(),
                                GetConstantTrustedVaultKey(),
                                ProtoStringToBytes(shared_key.member_proof())));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldSendJoinSecurityDomainsRequestWithKeys) {
  const std::vector<std::vector<uint8_t>> kTrustedVaultKeys = {{1, 2},
                                                               {1, 2, 3, 4}};
  const int kLastKeyVersion = 1234;
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeys,
          kLastKeyVersion, key_pair->public_key(),
          AuthenticationFactorType::kPhysicalDevice,
          /*authentication_factor_type_hint=*/absl::nullopt,
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url,
              Eq(GetFullJoinSecurityDomainsURLForTesting(kTestURL)));

  sync_pb::JoinSecurityDomainsRequest deserialized_body;
  EXPECT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.security_domain().name(),
              Eq(kSyncSecurityDomainName));

  std::string public_key_string;
  AssignBytesToProtoString(key_pair->public_key().ExportToBytes(),
                           &public_key_string);

  std::string encoded_public_key;
  base::Base64UrlEncode(public_key_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);

  const sync_pb::SecurityDomainMember& member =
      deserialized_body.security_domain_member();
  EXPECT_THAT(member.name(),
              Eq(kSecurityDomainMemberNamePrefix + encoded_public_key));
  EXPECT_THAT(member.public_key(), Eq(public_key_string));
  EXPECT_THAT(member.member_type(),
              Eq(sync_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE));

  ASSERT_THAT(deserialized_body.shared_member_key(),
              SizeIs(kTrustedVaultKeys.size()));
  const sync_pb::SharedMemberKey& shared_key_1 =
      deserialized_body.shared_member_key(0);
  EXPECT_THAT(shared_key_1.epoch(), Eq(kLastKeyVersion - 1));

  EXPECT_THAT(DecryptTrustedVaultWrappedKey(key_pair->private_key(),
                                            /*wrapped_key=*/ProtoStringToBytes(
                                                shared_key_1.wrapped_key())),
              Eq(kTrustedVaultKeys[0]));
  EXPECT_TRUE(
      VerifyMemberProof(key_pair->public_key(), kTrustedVaultKeys[0],
                        ProtoStringToBytes(shared_key_1.member_proof())));

  const sync_pb::SharedMemberKey& shared_key_2 =
      deserialized_body.shared_member_key(1);
  EXPECT_THAT(shared_key_2.epoch(), Eq(kLastKeyVersion));

  EXPECT_THAT(DecryptTrustedVaultWrappedKey(key_pair->private_key(),
                                            /*wrapped_key=*/ProtoStringToBytes(
                                                shared_key_2.wrapped_key())),
              Eq(kTrustedVaultKeys[1]));
  EXPECT_TRUE(
      VerifyMemberProof(key_pair->public_key(), kTrustedVaultKeys[1],
                        ProtoStringToBytes(shared_key_2.member_proof())));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldSendJoinSecurityDomainsRequestTypeHint) {
  const int kTypeHint = 19;
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeys,
          /*last_trusted_vault_key_version=*/1234, key_pair->public_key(),
          AuthenticationFactorType::kUnspecified,
          /*authentication_factor_type_hint=*/kTypeHint,
          TrustedVaultConnection::RegisterAuthenticationFactorCallback());
  EXPECT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_request, NotNull());
  const network::ResourceRequest& resource_request = pending_request->request;
  EXPECT_THAT(resource_request.method, Eq("POST"));
  EXPECT_THAT(resource_request.url,
              Eq(GetFullJoinSecurityDomainsURLForTesting(kTestURL)));

  sync_pb::JoinSecurityDomainsRequest deserialized_body;
  ASSERT_TRUE(deserialized_body.ParseFromString(
      network::GetUploadData(resource_request)));
  EXPECT_THAT(deserialized_body.member_type_hint(), Eq(kTypeHint));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleSuccessfulJoinSecurityDomainsRequest) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeys,
          /*last_trusted_vault_key_version=*/1, key_pair->public_key(),
          AuthenticationFactorType::kPhysicalDevice,
          /*authentication_factor_type_hint=*/absl::nullopt, callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(TrustedVaultRegistrationStatus::kSuccess)));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(
      net::HTTP_OK, MakeJoinSecurityDomainsResponse(/*current_epoch=*/1)
                        .SerializeAsString()));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldPopulateConstantKeyAndVersionWhenJoinSecurityDomain) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<TrustedVaultConnection::RegisterDeviceWithoutKeysCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterDeviceWithoutKeys(
          /*account_info=*/CoreAccountInfo(), key_pair->public_key(),
          callback.Get());
  ASSERT_THAT(request, NotNull());

  const int kServerConstantKeyVersion = 100;
  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultRegistrationStatus::kSuccess),
                  TrustedVaultKeyAndVersionEq(GetConstantTrustedVaultKey(),
                                              kServerConstantKeyVersion)));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(
      net::HTTP_OK, MakeJoinSecurityDomainsResponse(
                        /*current_epoch=*/kServerConstantKeyVersion)
                        .SerializeAsString()));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleJoinSecurityDomainsResponseWithConflictError) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<TrustedVaultConnection::RegisterDeviceWithoutKeysCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterDeviceWithoutKeys(
          /*account_info=*/CoreAccountInfo(), key_pair->public_key(),
          callback.Get());
  ASSERT_THAT(request, NotNull());

  const int kServerConstantKeyVersion = 100;
  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultRegistrationStatus::kAlreadyRegistered),
                  TrustedVaultKeyAndVersionEq(GetConstantTrustedVaultKey(),
                                              kServerConstantKeyVersion)));

  sync_pb::JoinSecurityDomainsErrorDetail error_detail;
  *error_detail.mutable_already_exists_response() =
      MakeJoinSecurityDomainsResponse(
          /*current_epoch=*/kServerConstantKeyVersion);

  sync_pb::RPCStatus response;
  sync_pb::Proto3Any* status_detail = response.add_details();
  status_detail->set_type_url(kJoinSecurityDomainsErrorDetailTypeURL);
  status_detail->set_value(error_detail.SerializeAsString());

  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(
      net::HTTP_CONFLICT, response.SerializeAsString()));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleJoinSecurityDomainsRequestWithEmptyResponse) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeys,
          /*last_trusted_vault_key_version=*/0, key_pair->public_key(),
          AuthenticationFactorType::kPhysicalDevice,
          /*authentication_factor_type_hint=*/absl::nullopt, callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(TrustedVaultRegistrationStatus::kOtherError)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_OK,
                                          /*response_content=*/std::string()));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleJoinSecurityDomainsRequestWithCorruptedResponse) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeys,
          /*last_trusted_vault_key_version=*/0, key_pair->public_key(),
          AuthenticationFactorType::kPhysicalDevice,
          /*authentication_factor_type_hint=*/absl::nullopt, callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(TrustedVaultRegistrationStatus::kOtherError)));
  EXPECT_TRUE(RespondToJoinSecurityDomainsRequest(
      net::HTTP_OK,
      /*response_content=*/"corrupted_proto"));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleFailedJoinSecurityDomainsRequest) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeys,
          /*last_trusted_vault_key_version=*/1, key_pair->public_key(),
          AuthenticationFactorType::kPhysicalDevice,
          /*authentication_factor_type_hint=*/absl::nullopt, callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(Eq(TrustedVaultRegistrationStatus::kOtherError)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_INTERNAL_SERVER_ERROR,
                                          /*response_content=*/std::string()));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleFailedJoinSecurityDomainsRequestWithNotFoundStatus) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeys,
          /*last_trusted_vault_key_version=*/1, key_pair->public_key(),
          AuthenticationFactorType::kPhysicalDevice,
          /*authentication_factor_type_hint=*/absl::nullopt, callback.Get());
  ASSERT_THAT(request, NotNull());

  // In particular, HTTP_NOT_FOUND indicates that security domain was removed.
  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultRegistrationStatus::kLocalDataObsolete)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_NOT_FOUND,
                                          /*response_content=*/std::string()));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleFailedJoinSecurityDomainsRequestWithBadRequestStatus) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeys,
          /*last_trusted_vault_key_version=*/1, key_pair->public_key(),
          AuthenticationFactorType::kPhysicalDevice,
          /*authentication_factor_type_hint=*/absl::nullopt, callback.Get());
  ASSERT_THAT(request, NotNull());

  // In particular, HTTP_BAD_REQUEST indicates that
  // |last_trusted_vault_key_and_version| is not actually the last on the server
  // side.
  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultRegistrationStatus::kLocalDataObsolete)));
  EXPECT_TRUE(
      RespondToJoinSecurityDomainsRequest(net::HTTP_BAD_REQUEST,
                                          /*response_content=*/std::string()));
}

TEST_F(
    TrustedVaultConnectionImplTest,
    ShouldHandleAccessTokenFetchingFailureWhenRegisteringAuthenticationFactor) {
  std::unique_ptr<TrustedVaultConnectionImpl> connection =
      CreateConnectionWithAccessToken(
          /*access_token=*/absl::nullopt);

  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  // |callback| is called immediately after RegisterAuthenticationFactor(),
  // because there is no access token.
  EXPECT_CALL(
      callback,
      Run(Eq(TrustedVaultRegistrationStatus::kAccessTokenFetchingFailure)));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeys,
          /*last_trusted_vault_key_version=*/1, key_pair->public_key(),
          AuthenticationFactorType::kPhysicalDevice,
          /*authentication_factor_type_hint=*/absl::nullopt, callback.Get());
  ASSERT_THAT(request, NotNull());

  // No requests should be sent to the network.
  EXPECT_THAT(GetPendingHTTPRequest(), IsNull());
}

TEST_F(TrustedVaultConnectionImplTest, ShouldCancelJoinSecurityDomainsRequest) {
  std::unique_ptr<SecureBoxKeyPair> key_pair = MakeTestKeyPair();
  ASSERT_THAT(key_pair, NotNull());

  base::MockCallback<
      TrustedVaultConnection::RegisterAuthenticationFactorCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->RegisterAuthenticationFactor(
          /*account_info=*/CoreAccountInfo(), kTrustedVaultKeys,
          /*last_trusted_vault_key_version=*/1, key_pair->public_key(),
          AuthenticationFactorType::kPhysicalDevice,
          /*authentication_factor_type_hint=*/absl::nullopt, callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run).Times(0);
  request.reset();
  // Returned value isn't checked here, because the request can be cancelled
  // before reaching TestURLLoaderFactory.
  RespondToJoinSecurityDomainsRequest(net::HTTP_OK,
                                      /*response_content=*/std::string());
}

TEST_F(TrustedVaultConnectionImplTest, ShouldSendGetSecurityDomainsRequest) {
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadNewKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(/*key=*/std::vector<uint8_t>(),
                                    /*version=*/1),
          /*device_key_pair=*/MakeTestKeyPair(), base::DoNothing());
  EXPECT_THAT(request, NotNull());

  network::TestURLLoaderFactory::PendingRequest* pending_http_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_http_request, NotNull());

  const network::ResourceRequest& resource_request =
      pending_http_request->request;
  EXPECT_THAT(resource_request.method, Eq("GET"));
  EXPECT_THAT(resource_request.url,
              Eq(GetFullGetSecurityDomainMemberURLForTesting(
                  kTestURL, MakeTestKeyPair()->public_key().ExportToBytes())));
}

// TODO(crbug.com/1113598): add coverage for at least one successful case
// (need to share some helper functions with
// download_keys_response_handler_unittest.cc).
TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleFailedGetSecurityDomainMemberRequest) {
  base::MockCallback<TrustedVaultConnection::DownloadNewKeysCallback> callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadNewKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(/*key=*/std::vector<uint8_t>(),
                                    /*version=*/1),
          /*device_key_pair=*/MakeTestKeyPair(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback,
              Run(Eq(TrustedVaultDownloadKeysStatus::kOtherError), _, _));
  EXPECT_TRUE(
      RespondToGetSecurityDomainMemberRequest(net::HTTP_INTERNAL_SERVER_ERROR));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleAccessTokenFetchingFailureWhenDownloadingKeys) {
  std::unique_ptr<TrustedVaultConnectionImpl> connection =
      CreateConnectionWithAccessToken(
          /*access_token=*/absl::nullopt);

  base::MockCallback<TrustedVaultConnection::DownloadNewKeysCallback> callback;

  // |callback| is called immediately after DownloadNewKeys(), because there is
  // no access token.
  EXPECT_CALL(
      callback,
      Run(Eq(TrustedVaultDownloadKeysStatus::kAccessTokenFetchingFailure), _,
          _));
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection->DownloadNewKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(
              /*key=*/std::vector<uint8_t>(),
              /*version=*/1),
          /*device_key_pair=*/MakeTestKeyPair(), callback.Get());
  ASSERT_THAT(request, NotNull());

  // No requests should be sent to the network.
  EXPECT_THAT(GetPendingHTTPRequest(), IsNull());
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldCancelGetSecurityDomainMemberRequest) {
  base::MockCallback<TrustedVaultConnection::DownloadNewKeysCallback> callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadNewKeys(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultKeyAndVersion(
              /*key=*/std::vector<uint8_t>(),
              /*version=*/1),
          /*device_key_pair=*/MakeTestKeyPair(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run).Times(0);
  request.reset();
  // Returned value isn't checked here, because the request can be cancelled
  // before reaching TestURLLoaderFactory.
  RespondToGetSecurityDomainMemberRequest(net::HTTP_OK);
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldSendGetSecurityDomainRequestWhenRetrievingRecoverability) {
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadIsRecoverabilityDegraded(
          /*account_info=*/CoreAccountInfo(),
          TrustedVaultConnection::IsRecoverabilityDegradedCallback());
  ASSERT_THAT(request, NotNull());

  const network::TestURLLoaderFactory::PendingRequest* pending_http_request =
      GetPendingHTTPRequest();
  ASSERT_THAT(pending_http_request, NotNull());

  const network::ResourceRequest& resource_request =
      pending_http_request->request;
  EXPECT_THAT(resource_request.method, Eq("GET"));
  EXPECT_THAT(resource_request.url,
              Eq(GetFullGetSecurityDomainURLForTesting(kTestURL)));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleValidResponseWhenRetrievingRecoverability) {
  base::MockCallback<TrustedVaultConnection::IsRecoverabilityDegradedCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadIsRecoverabilityDegraded(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kNotDegraded));
  EXPECT_TRUE(RespondToGetSecurityDomainRequest(
      net::HTTP_OK,
      /*response_body=*/MakeSecurityDomainWithDegradedRecoverability(
          /*recoverability_degraded=*/false)
          .SerializeAsString()));
  testing::Mock::VerifyAndClearExpectations(&callback);

  request = connection()->DownloadIsRecoverabilityDegraded(
      /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kDegraded));
  EXPECT_TRUE(RespondToGetSecurityDomainRequest(
      net::HTTP_OK,
      /*response_body=*/MakeSecurityDomainWithDegradedRecoverability(
          /*recoverability_degraded=*/true)
          .SerializeAsString()));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleFailedRequestWhenRetrievingRecoverability) {
  base::MockCallback<TrustedVaultConnection::IsRecoverabilityDegradedCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadIsRecoverabilityDegraded(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kError));
  EXPECT_TRUE(RespondToGetSecurityDomainRequest(
      net::HTTP_INTERNAL_SERVER_ERROR,
      /*response_body=*/MakeSecurityDomainWithDegradedRecoverability(
          /*recoverability_degraded=*/false)
          .SerializeAsString()));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldHandleCorruptedResponseWhenRetrievingRecoverability) {
  base::MockCallback<TrustedVaultConnection::IsRecoverabilityDegradedCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadIsRecoverabilityDegraded(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kError));
  // Respond with invalid proto.
  EXPECT_TRUE(
      RespondToGetSecurityDomainRequest(net::HTTP_OK,
                                        /*response_body=*/"invalid proto"));

  request = connection()->DownloadIsRecoverabilityDegraded(
      /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run(TrustedVaultRecoverabilityStatus::kError));
  // Respond with empty proto.
  EXPECT_TRUE(RespondToGetSecurityDomainRequest(
      net::HTTP_OK,
      /*response_body=*/sync_pb::SecurityDomain().SerializeAsString()));
}

TEST_F(TrustedVaultConnectionImplTest,
       ShouldCancelRequestWhenRetrievingRecoverability) {
  base::MockCallback<TrustedVaultConnection::IsRecoverabilityDegradedCallback>
      callback;

  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection()->DownloadIsRecoverabilityDegraded(
          /*account_info=*/CoreAccountInfo(), callback.Get());
  ASSERT_THAT(request, NotNull());

  EXPECT_CALL(callback, Run).Times(0);
  request.reset();
  // Returned value isn't checked here, because the request can be cancelled
  // before reaching TestURLLoaderFactory.
  RespondToGetSecurityDomainRequest(
      net::HTTP_OK,
      /*response_body=*/MakeSecurityDomainWithDegradedRecoverability(
          /*recoverability_degraded=*/false)
          .SerializeAsString());
}

}  // namespace

}  // namespace syncer
