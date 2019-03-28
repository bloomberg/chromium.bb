// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_grpc_context.h"

#include <utility>

#include "base/guid.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "google_apis/google_api_keys.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"
#include "third_party/grpc/src/include/grpcpp/client_context.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {

namespace {

constexpr char kChromotingAppIdentifier[] = "CRD";

// TODO(yuweih): We should target different service environments.
constexpr char kFtlServerEndpoint[] = "instantmessaging-pa.googleapis.com";

static base::NoDestructor<GrpcChannelSharedPtr> g_channel_for_testing;

}  // namespace

// static
std::string FtlGrpcContext::GetChromotingAppIdentifier() {
  return kChromotingAppIdentifier;
}

// static
ftl::Id FtlGrpcContext::CreateIdFromString(const std::string& ftl_id) {
  ftl::Id id;
  id.set_id(ftl_id);
  id.set_app(GetChromotingAppIdentifier());
  // TODO(yuweih): Migrate to IdType.Type.CHROMOTING_ID.
  id.set_type(ftl::IdType_Type_EMAIL);
  return id;
}

// static
GrpcChannelSharedPtr FtlGrpcContext::CreateChannel() {
  if (*g_channel_for_testing) {
    return *g_channel_for_testing;
  }
  return CreateSslChannelForEndpoint(kFtlServerEndpoint);
}

// static
std::unique_ptr<grpc::ClientContext> FtlGrpcContext::CreateClientContext() {
  auto context = std::make_unique<grpc::ClientContext>();
  context->AddMetadata("x-goog-api-key", google_apis::GetRemotingFtlAPIKey());
  return context;
}

// static
ftl::RequestHeader FtlGrpcContext::CreateRequestHeader(
    const std::string& ftl_auth_token) {
  ftl::RequestHeader header;
  header.set_request_id(base::GenerateGUID());
  header.set_app(kChromotingAppIdentifier);
  if (!ftl_auth_token.empty()) {
    header.set_auth_token_payload(ftl_auth_token);
  }
  ftl::ClientInfo* client_info = header.mutable_client_info();
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

// static
void FtlGrpcContext::SetChannelForTesting(GrpcChannelSharedPtr channel) {
  *g_channel_for_testing = channel;
}

}  // namespace remoting
