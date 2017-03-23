// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/NetworkUtils.h"

#include "components/mime_util/mime_util.h"
#include "net/base/data_url.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_data_job.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/URLConversion.h"
#include "public/platform/WebString.h"
#include "url/gurl.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"

namespace {

net::registry_controlled_domains::PrivateRegistryFilter
getNetPrivateRegistryFilter(blink::NetworkUtils::PrivateRegistryFilter filter) {
  switch (filter) {
    case blink::NetworkUtils::IncludePrivateRegistries:
      return net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
    case blink::NetworkUtils::ExcludePrivateRegistries:
      return net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
  }
  // There are only two NetworkUtils::PrivateRegistryFilter enum entries, so
  // we should never reach this point. However, we must have a default return
  // value to avoid a compiler error.
  NOTREACHED();
  return net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
}

}  // namespace

namespace blink {

namespace NetworkUtils {

bool isReservedIPAddress(const String& host) {
  net::IPAddress address;
  StringUTF8Adaptor utf8(host);
  if (!net::ParseURLHostnameToAddress(utf8.asStringPiece(), &address))
    return false;
  return address.IsReserved();
}

bool isLocalHostname(const String& host, bool* isLocal6) {
  StringUTF8Adaptor utf8(host);
  return net::IsLocalHostname(utf8.asStringPiece(), isLocal6);
}

String getDomainAndRegistry(const String& host, PrivateRegistryFilter filter) {
  StringUTF8Adaptor hostUtf8(host);
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      hostUtf8.asStringPiece(), getNetPrivateRegistryFilter(filter));
  return String(domain.data(), domain.length());
}

PassRefPtr<SharedBuffer> parseDataURLAndPopulateResponse(
    const KURL& url,
    ResourceResponse& response) {
  // The following code contains duplication of GetInfoFromDataURL() and
  // WebURLLoaderImpl::PopulateURLResponse() in
  // content/child/web_url_loader_impl.cc. Merge them once content/child is
  // moved to platform/.
  std::string utf8MimeType;
  std::string utf8Charset;
  std::string dataString;
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));

  int result = net::URLRequestDataJob::BuildResponse(
      WebStringToGURL(url.getString()), &utf8MimeType, &utf8Charset,
      &dataString, headers.get());
  if (result != net::OK)
    return nullptr;

  if (!mime_util::IsSupportedMimeType(utf8MimeType))
    return nullptr;

  RefPtr<SharedBuffer> data =
      SharedBuffer::create(dataString.data(), dataString.size());
  response.setHTTPStatusCode(200);
  response.setHTTPStatusText("OK");
  response.setURL(url);
  response.setMimeType(WebString::fromUTF8(utf8MimeType));
  response.setExpectedContentLength(data->size());
  response.setTextEncodingName(WebString::fromUTF8(utf8Charset));

  size_t iter = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    response.addHTTPHeaderField(WebString::fromLatin1(name),
                                WebString::fromLatin1(value));
  }
  return data;
}

bool isRedirectResponseCode(int responseCode) {
  return net::HttpResponseHeaders::IsRedirectResponseCode(responseCode);
}

}  // NetworkUtils

}  // namespace blink
