// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/uma_util.h"

#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"

namespace data_reduction_proxy {

ProxyScheme ConvertNetProxySchemeToProxyScheme(
    net::ProxyServer::Scheme scheme) {
  switch (scheme) {
    case net::ProxyServer::SCHEME_HTTP:
      return PROXY_SCHEME_HTTP;
    case net::ProxyServer::SCHEME_HTTPS:
      return PROXY_SCHEME_HTTPS;
    case net::ProxyServer::SCHEME_QUIC:
      return PROXY_SCHEME_QUIC;
    case net::ProxyServer::SCHEME_DIRECT:
      return PROXY_SCHEME_DIRECT;
    default:
      NOTREACHED() << scheme;
      return PROXY_SCHEME_UNKNOWN;
  }
}

void LogSuccessfulProxyUMAs(const DataReductionProxyTypeInfo& proxy_info,
                            const net::ProxyServer& proxy_server,
                            bool is_main_frame) {
  // Report the success counts.
  UMA_HISTOGRAM_COUNTS_100(
      "DataReductionProxy.SuccessfulRequestCompletionCounts",
      proxy_info.proxy_index);

  // It is possible that the scheme of request->proxy_server() is different
  // from the scheme of proxy_info.proxy_servers.front(). The former may be set
  // to QUIC by the network stack, while the latter may be set to HTTPS.
  UMA_HISTOGRAM_ENUMERATION(
      "DataReductionProxy.ProxySchemeUsed",
      ConvertNetProxySchemeToProxyScheme(proxy_server.scheme()),
      PROXY_SCHEME_MAX);
  if (is_main_frame) {
    UMA_HISTOGRAM_COUNTS_100(
        "DataReductionProxy.SuccessfulRequestCompletionCounts.MainFrame",
        proxy_info.proxy_index);
  }
}

}  // namespace data_reduction_proxy
