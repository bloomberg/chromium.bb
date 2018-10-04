// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/network_utils.h"

#include "net/base/data_url.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_data_job.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"

namespace {

net::registry_controlled_domains::PrivateRegistryFilter
getNetPrivateRegistryFilter(blink::NetworkUtils::PrivateRegistryFilter filter) {
  switch (filter) {
    case blink::NetworkUtils::kIncludePrivateRegistries:
      return net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
    case blink::NetworkUtils::kExcludePrivateRegistries:
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

bool IsReservedIPAddress(const String& host) {
  net::IPAddress address;
  StringUTF8Adaptor utf8(host);
  if (!net::ParseURLHostnameToAddress(utf8.AsStringPiece(), &address))
    return false;
  return !address.IsPubliclyRoutable();
}

bool IsLocalHostname(const String& host, bool* is_local6) {
  StringUTF8Adaptor utf8(host);
  return net::IsLocalHostname(utf8.AsStringPiece(), is_local6);
}

String GetDomainAndRegistry(const String& host, PrivateRegistryFilter filter) {
  StringUTF8Adaptor host_utf8(host);
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      host_utf8.AsStringPiece(), getNetPrivateRegistryFilter(filter));
  return String(domain.data(), domain.length());
}

scoped_refptr<SharedBuffer> ParseDataURLAndPopulateResponse(
    const KURL& url,
    ResourceResponse& response) {
  // The following code contains duplication of GetInfoFromDataURL() and
  // WebURLLoaderImpl::PopulateURLResponse() in
  // content/child/web_url_loader_impl.cc. Merge them once content/child is
  // moved to platform/.
  std::string utf8_mime_type;
  std::string utf8_charset;
  std::string data_string;
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));

  int result = net::URLRequestDataJob::BuildResponse(
      GURL(url), &utf8_mime_type, &utf8_charset, &data_string, headers.get());
  if (result != net::OK)
    return nullptr;

  if (!blink::IsSupportedMimeType(utf8_mime_type))
    return nullptr;

  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(data_string.data(), data_string.size());
  response.SetHTTPStatusCode(200);
  response.SetHTTPStatusText("OK");
  response.SetURL(url);
  response.SetMimeType(WebString::FromUTF8(utf8_mime_type));
  response.SetExpectedContentLength(data->size());
  response.SetTextEncodingName(WebString::FromUTF8(utf8_charset));

  size_t iter = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    response.AddHTTPHeaderField(WebString::FromLatin1(name),
                                WebString::FromLatin1(value));
  }
  return data;
}

bool IsDataURLMimeTypeSupported(const KURL& url) {
  std::string utf8_mime_type;
  std::string utf8_charset;
  if (net::DataURL::Parse(GURL(url), &utf8_mime_type, &utf8_charset, nullptr)) {
    return blink::IsSupportedMimeType(utf8_mime_type);
  }
  return false;
}

bool IsRedirectResponseCode(int response_code) {
  return net::HttpResponseHeaders::IsRedirectResponseCode(response_code);
}

bool IsCertificateTransparencyRequiredError(int error_code) {
  return error_code == net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
}

bool IsLegacySymantecCertError(int error_code) {
  return error_code == net::ERR_CERT_SYMANTEC_LEGACY;
}

String GenerateAcceptLanguageHeader(const String& lang) {
  CString cstring(lang.Utf8());
  std::string string(cstring.data(), cstring.length());
  return WebString::FromUTF8(
      net::HttpUtil::GenerateAcceptLanguageHeader(string));
}

}  // NetworkUtils

}  // namespace blink
