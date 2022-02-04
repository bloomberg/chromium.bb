// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_request_info.h"

#include "content/public/browser/weak_document_ptr.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"

namespace content {

NavigationRequestInfo::NavigationRequestInfo(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::BeginNavigationParamsPtr begin_params,
    network::mojom::WebSandboxFlags sandbox_flags,
    const net::IsolationInfo& isolation_info,
    bool is_main_frame,
    bool are_ancestors_secure,
    int frame_tree_node_id,
    bool report_raw_headers,
    bool upgrade_if_insecure,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        blob_url_loader_factory,
    const base::UnguessableToken& devtools_navigation_token,
    const base::UnguessableToken& devtools_frame_token,
    bool obey_origin_policy,
    net::HttpRequestHeaders cors_exempt_headers,
    network::mojom::ClientSecurityStatePtr client_security_state,
    const absl::optional<std::vector<net::SourceStream::SourceType>>&
        devtools_accepted_stream_types,
    bool is_pdf,
    WeakDocumentPtr initiator_document)
    : common_params(std::move(common_params)),
      begin_params(std::move(begin_params)),
      sandbox_flags(sandbox_flags),
      isolation_info(isolation_info),
      is_main_frame(is_main_frame),
      are_ancestors_secure(are_ancestors_secure),
      frame_tree_node_id(frame_tree_node_id),
      report_raw_headers(report_raw_headers),
      upgrade_if_insecure(upgrade_if_insecure),
      blob_url_loader_factory(std::move(blob_url_loader_factory)),
      devtools_navigation_token(devtools_navigation_token),
      devtools_frame_token(devtools_frame_token),
      obey_origin_policy(obey_origin_policy),
      cors_exempt_headers(std::move(cors_exempt_headers)),
      client_security_state(std::move(client_security_state)),
      devtools_accepted_stream_types(devtools_accepted_stream_types),
      is_pdf(is_pdf),
      initiator_document(std::move(initiator_document)) {}

NavigationRequestInfo::~NavigationRequestInfo() {}

}  // namespace content
