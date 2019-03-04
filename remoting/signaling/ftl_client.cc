// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_client.h"

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "build/build_config.h"
#include "google_apis/google_api_keys.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {

namespace {

constexpr char kChromotingAppIdentifier[] = "CRD";

// TODO(yuweih): We should target different service environments.
constexpr char kFtlServerEndpoint[] = "instantmessaging-pa.googleapis.com";

constexpr ftl::FtlCapability::Feature kFtlCapabilities[] = {
    ftl::FtlCapability_Feature_RECEIVE_CALLS_FROM_GAIA,
    ftl::FtlCapability_Feature_GAIA_REACHABLE};

}  // namespace

FtlClient::FtlClient(OAuthTokenGetter* token_getter) : weak_factory_(this) {
  DCHECK(token_getter);
  token_getter_ = token_getter;
  auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
  auto channel = grpc::CreateChannel(kFtlServerEndpoint, channel_creds);
  peer_to_peer_stub_ = PeerToPeer::NewStub(channel);
  registration_stub_ = Registration::NewStub(channel);
  messaging_stub_ = Messaging::NewStub(channel);
}

FtlClient::~FtlClient() = default;

void FtlClient::SetAuthToken(const std::string& auth_token) {
  DCHECK(!auth_token.empty());
  auth_token_ = auth_token;
}

void FtlClient::GetIceServer(RpcCallback<ftl::GetICEServerResponse> callback) {
  ftl::GetICEServerRequest request;
  request.set_allocated_header(BuildRequestHeader().release());

  GetOAuthTokenAndExecuteRpc(
      base::BindOnce(&PeerToPeer::Stub::AsyncGetICEServer,
                     base::Unretained(peer_to_peer_stub_.get())),
      request, std::move(callback));
}

void FtlClient::SignInGaia(const std::string& device_id,
                           ftl::SignInGaiaMode::Value sign_in_gaia_mode,
                           RpcCallback<ftl::SignInGaiaResponse> callback) {
  ftl::SignInGaiaRequest request;
  request.set_allocated_header(BuildRequestHeader().release());
  request.set_app(kChromotingAppIdentifier);
  request.set_mode(sign_in_gaia_mode);

  request.mutable_register_data()->mutable_device_id()->set_id(device_id);

  // TODO(yuweih): Consider using different device ID type.
  request.mutable_register_data()->mutable_device_id()->set_type(
      ftl::DeviceIdType_Type_WEB_UUID);

  size_t ftl_capability_count =
      sizeof(kFtlCapabilities) / sizeof(ftl::FtlCapability::Feature);
  for (size_t i = 0; i < ftl_capability_count; i++) {
    request.mutable_register_data()->add_caps(kFtlCapabilities[i]);
  }

  GetOAuthTokenAndExecuteRpc(
      base::BindOnce(&Registration::Stub::AsyncSignInGaia,
                     base::Unretained(registration_stub_.get())),
      request, std::move(callback));
}

void FtlClient::PullMessages(RpcCallback<ftl::PullMessagesResponse> callback) {
  ftl::PullMessagesRequest request;
  request.set_allocated_header(BuildRequestHeader().release());

  GetOAuthTokenAndExecuteRpc(
      base::BindOnce(&Messaging::Stub::AsyncPullMessages,
                     base::Unretained(messaging_stub_.get())),
      request, std::move(callback));
}

void FtlClient::AckMessages(const std::vector<ftl::ReceiverMessage>& messages,
                            RpcCallback<ftl::AckMessagesResponse> callback) {
  ftl::AckMessagesRequest request;
  request.set_allocated_header(BuildRequestHeader().release());

  for (auto& message : messages) {
    ftl::ReceiverMessage* new_message = request.add_messages();
    *new_message = message;
  }

  GetOAuthTokenAndExecuteRpc(
      base::BindOnce(&Messaging::Stub::AsyncAckMessages,
                     base::Unretained(messaging_stub_.get())),
      request, std::move(callback));
}

template <typename RequestType, typename ResponseType>
void FtlClient::GetOAuthTokenAndExecuteRpc(
    GrpcAsyncDispatcher::AsyncRpcFunction<RequestType, ResponseType> rpc,
    const RequestType& request,
    RpcCallback<ResponseType> callback) {
  token_getter_->CallWithToken(base::BindOnce(
      &FtlClient::ExecuteRpcWithFetchedOAuthToken<RequestType, ResponseType>,
      weak_factory_.GetWeakPtr(), std::move(rpc), request,
      std::move(callback)));
}

template <typename RequestType, typename ResponseType>
void FtlClient::ExecuteRpcWithFetchedOAuthToken(
    GrpcAsyncDispatcher::AsyncRpcFunction<RequestType, ResponseType> rpc,
    const RequestType& request,
    RpcCallback<ResponseType> callback,
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token) {
  std::unique_ptr<grpc::ClientContext> context = CreateClientContext();
  if (status != OAuthTokenGetter::Status::SUCCESS) {
    LOG(ERROR) << "Failed to fetch access token. Status: " << status;
  }
  if (status == OAuthTokenGetter::Status::SUCCESS && !access_token.empty()) {
    context->set_credentials(grpc::AccessTokenCredentials(access_token));
  } else {
    LOG(WARNING) << "Attempting to execute RPC without access token.";
  }
  dispatcher_.ExecuteAsyncRpc(std::move(rpc), std::move(context), request,
                              std::move(callback));
}

// static
std::unique_ptr<grpc::ClientContext> FtlClient::CreateClientContext() {
  auto context = std::make_unique<grpc::ClientContext>();
  context->AddMetadata("x-goog-api-key", google_apis::GetRemotingFtlAPIKey());
  return context;
}

std::unique_ptr<ftl::RequestHeader> FtlClient::BuildRequestHeader() {
  auto header = std::make_unique<ftl::RequestHeader>();
  header->set_request_id(base::GenerateGUID());
  header->set_app(kChromotingAppIdentifier);
  if (!auth_token_.empty()) {
    header->set_auth_token_payload(auth_token_);
  }
  ftl::ClientInfo* client_info = header->mutable_client_info();
  client_info->set_api_version(ftl::ApiVersion_Value_V4);
  client_info->set_version_major(VERSION_MAJOR);
  // Chrome's version has four number components, and the VERSION_MINOR is
  // always 0, like X.0.X.X. The FTL server requires three-component version
  // number so we just skip the VERSION_MINOR here.
  client_info->set_version_minor(VERSION_BUILD);
  client_info->set_version_point(VERSION_PATCH);
  ftl::Platform_Type platform_type;
#if defined(OS_ANDROID)
  platform_type = ftl::Platform_Type_FTL_ANDROID;
#elif defined(OS_IOS)
  platform_type = ftl::Platform_Type_FTL_IOS;
#else
  platform_type = ftl::Platform_Type_FTL_DESKTOP;
#endif
  client_info->set_platform_type(platform_type);
  return header;
}

}  // namespace remoting
