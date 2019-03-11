// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_grpc_context.h"

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

}  // namespace

// static
std::string FtlGrpcContext::GetChromotingAppIdentifier() {
  return kChromotingAppIdentifier;
}

FtlGrpcContext::FtlGrpcContext(OAuthTokenGetter* token_getter)
    : weak_factory_(this) {
  DCHECK(token_getter);
  token_getter_ = token_getter;
  auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
  channel_ = grpc::CreateChannel(kFtlServerEndpoint, channel_creds);
}

FtlGrpcContext::~FtlGrpcContext() = default;

void FtlGrpcContext::SetAuthToken(const std::string& auth_token) {
  DCHECK(!auth_token.empty());
  auth_token_ = auth_token;
}

void FtlGrpcContext::SetChannelForTesting(
    std::shared_ptr<grpc::ChannelInterface> channel) {
  channel_ = channel;
}

void FtlGrpcContext::GetOAuthTokenAndExecuteRpc(
    ExecuteRpcWithContextCallback execute_rpc_with_context) {
  token_getter_->CallWithToken(base::BindOnce(
      &FtlGrpcContext::ExecuteRpcWithFetchedOAuthToken,
      weak_factory_.GetWeakPtr(), std::move(execute_rpc_with_context)));
}

void FtlGrpcContext::ExecuteRpcWithFetchedOAuthToken(
    ExecuteRpcWithContextCallback execute_rpc_with_context,
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
  std::move(execute_rpc_with_context).Run(std::move(context));
}

// static
std::unique_ptr<grpc::ClientContext> FtlGrpcContext::CreateClientContext() {
  auto context = std::make_unique<grpc::ClientContext>();
  context->AddMetadata("x-goog-api-key", google_apis::GetRemotingFtlAPIKey());
  return context;
}

std::unique_ptr<ftl::RequestHeader> FtlGrpcContext::BuildRequestHeader() {
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
