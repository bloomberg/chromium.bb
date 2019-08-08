// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_network_service_client.h"

#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"

namespace network {

TestNetworkServiceClient::TestNetworkServiceClient() : binding_(nullptr) {}

TestNetworkServiceClient::TestNetworkServiceClient(
    mojom::NetworkServiceClientRequest request)
    : binding_(this, std::move(request)) {}

TestNetworkServiceClient::~TestNetworkServiceClient() {}

void TestNetworkServiceClient::OnAuthRequired(
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const GURL& url,
    const GURL& site_for_cookies,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    int32_t resource_type,
    const base::Optional<ResourceResponseHead>& head,
    mojom::AuthChallengeResponderPtr auth_challenge_responder) {
  NOTREACHED();
}

void TestNetworkServiceClient::OnCertificateRequested(
    const base::Optional<base::UnguessableToken>& window_id,
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    mojom::NetworkServiceClient::OnCertificateRequestedCallback callback) {
  NOTREACHED();
}

void TestNetworkServiceClient::OnSSLCertificateError(
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    int32_t resource_type,
    const GURL& url,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  NOTREACHED();
}

void TestNetworkServiceClient::OnCookiesRead(int process_id,
                                             int routing_id,
                                             const GURL& url,
                                             const GURL& first_party_url,
                                             const net::CookieList& cookie_list,
                                             bool blocked_by_policy) {
  NOTREACHED();
}

void TestNetworkServiceClient::OnCookieChange(
    int process_id,
    int routing_id,
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy) {
  NOTREACHED();
}

#if defined(OS_CHROMEOS)
void TestNetworkServiceClient::OnTrustAnchorUsed(
    const std::string& username_hash) {
  NOTREACHED();
}
#endif

void TestNetworkServiceClient::OnFileUploadRequested(
    uint32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    OnFileUploadRequestedCallback callback) {
  if (upload_files_invalid_) {
    std::move(callback).Run(net::ERR_ACCESS_DENIED, std::vector<base::File>());
    return;
  }
  base::ScopedAllowBlockingForTesting allow_blocking;
  uint32_t file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                        (async ? base::File::FLAG_ASYNC : 0);
  std::vector<base::File> files;
  for (base::FilePath path : file_paths) {
    files.emplace_back(path, file_flags);
    if (!files.back().IsValid()) {
      std::move(callback).Run(
          net::FileErrorToNetError(files.back().error_details()),
          std::vector<base::File>());
      return;
    }
  }

  if (ignore_last_upload_file_) {
    // Make the TestNetworkServiceClient respond one less file as requested.
    files.pop_back();
  }

  std::move(callback).Run(net::OK, std::move(files));
}

void TestNetworkServiceClient::OnLoadingStateUpdate(
    std::vector<mojom::LoadInfoPtr> infos,
    OnLoadingStateUpdateCallback callback) {}

void TestNetworkServiceClient::OnClearSiteData(
    int process_id,
    int routing_id,
    const GURL& url,
    const std::string& header_value,
    int load_flags,
    OnClearSiteDataCallback callback) {
  NOTREACHED();
}

void TestNetworkServiceClient::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {}

#if defined(OS_ANDROID)
void TestNetworkServiceClient::OnGenerateHttpNegotiateAuthToken(
    const std::string& server_auth_token,
    bool can_delegate,
    const std::string& auth_negotiate_android_account_type,
    const std::string& spn,
    OnGenerateHttpNegotiateAuthTokenCallback callback) {
  NOTREACHED();
}
#endif

}  // namespace network
