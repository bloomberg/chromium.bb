// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_client.h"

#include "base/guid.h"
#include "build/build_config.h"
#include "google_apis/google_api_keys.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace {

constexpr char kChromotingAppIdentifier[] = "CRD";

// TODO(yuweih): We should target different service environments.
constexpr char kFtlServerEndpoint[] = "instantmessaging-pa.googleapis.com";

}  // namespace

namespace remoting {

FtlClient::FtlClient() {
  auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
  auto channel = grpc::CreateChannel(kFtlServerEndpoint, channel_creds);
  peer_to_peer_stub_ = PeerToPeer::NewStub(channel);
}

FtlClient::~FtlClient() {}

grpc::Status FtlClient::GetIceServer(ftl::GetICEServerResponse* response) {
  grpc::ClientContext context;
  FillClientContextMetadata(&context);

  ftl::GetICEServerRequest request;
  request.set_allocated_header(BuildRequestHeader().release());

  return peer_to_peer_stub_->GetICEServer(&context, request, response);
}

// static
void FtlClient::FillClientContextMetadata(grpc::ClientContext* context) {
  context->AddMetadata("x-goog-api-key", google_apis::GetRemotingFtlAPIKey());
}

// static
std::unique_ptr<ftl::RequestHeader> FtlClient::BuildRequestHeader() {
  auto header = std::make_unique<ftl::RequestHeader>();
  header->set_request_id(base::GenerateGUID());
  header->set_app(kChromotingAppIdentifier);
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
