// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_

#include <string>

#include "base/pickle.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "net/base/host_port_pair.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/http/http_request_headers.h"
#include "net/nqe/effective_connection_type.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/interfaces/fetch_api.mojom-shared.h"
#include "url/ipc/url_param_traits.h"

#ifndef INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_
#define INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_

// services/network/public/cpp is currently packaged as a static library,
// so there's no need for export defines; it's linked directly into whatever
// other components need it.
// This redefinition is present for the IPC macros below, including in the
// included header files.
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

namespace network {
struct HttpRawRequestResponseInfo;
}

namespace IPC {

template <>
struct ParamTraits<net::CertVerifyResult> {
  typedef net::CertVerifyResult param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<net::HashValue> {
  typedef net::HashValue param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<net::HostPortPair> {
  typedef net::HostPortPair param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<net::HttpRequestHeaders> {
  typedef net::HttpRequestHeaders param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<net::OCSPVerifyResult> {
  typedef net::OCSPVerifyResult param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<net::SSLInfo> {
  typedef net::SSLInfo param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>> {
  typedef scoped_refptr<net::ct::SignedCertificateTimestamp> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<scoped_refptr<network::HttpRawRequestResponseInfo>> {
  typedef scoped_refptr<network::HttpRawRequestResponseInfo> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<scoped_refptr<net::HttpResponseHeaders>> {
  typedef scoped_refptr<net::HttpResponseHeaders> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<scoped_refptr<net::X509Certificate>> {
  typedef scoped_refptr<net::X509Certificate> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<net::LoadTimingInfo> {
  typedef net::LoadTimingInfo param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_

IPC_ENUM_TRAITS_MAX_VALUE(
    net::ct::CTPolicyCompliance,
    net::ct::CTPolicyCompliance::CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE)
IPC_ENUM_TRAITS_MAX_VALUE(net::OCSPVerifyResult::ResponseStatus,
                          net::OCSPVerifyResult::PARSE_RESPONSE_DATA_ERROR)
IPC_ENUM_TRAITS_MAX_VALUE(net::OCSPRevocationStatus,
                          net::OCSPRevocationStatus::UNKNOWN)

IPC_ENUM_TRAITS_MAX_VALUE(net::ct::SCTVerifyStatus, net::ct::SCT_STATUS_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(net::SSLInfo::HandshakeType,
                          net::SSLInfo::HANDSHAKE_FULL)
IPC_ENUM_TRAITS_MAX_VALUE(net::TokenBindingParam, net::TB_PARAM_ECDSAP256)

IPC_STRUCT_TRAITS_BEGIN(net::HttpRequestHeaders::HeaderKeyValuePair)
  IPC_STRUCT_TRAITS_MEMBER(key)
  IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(net::SignedCertificateTimestampAndStatus)
  IPC_STRUCT_TRAITS_MEMBER(sct)
  IPC_STRUCT_TRAITS_MEMBER(status)
IPC_STRUCT_TRAITS_END()

// Parameters for a ResourceMsg_RequestComplete
IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::CORSError,
                          network::mojom::CORSError::kLast)

IPC_STRUCT_TRAITS_BEGIN(network::CORSErrorStatus)
  IPC_STRUCT_TRAITS_MEMBER(cors_error)
  IPC_STRUCT_TRAITS_MEMBER(related_response_headers)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::URLLoaderCompletionStatus)
  IPC_STRUCT_TRAITS_MEMBER(error_code)
  IPC_STRUCT_TRAITS_MEMBER(exists_in_cache)
  IPC_STRUCT_TRAITS_MEMBER(completion_time)
  IPC_STRUCT_TRAITS_MEMBER(encoded_data_length)
  IPC_STRUCT_TRAITS_MEMBER(encoded_body_length)
  IPC_STRUCT_TRAITS_MEMBER(decoded_body_length)
  IPC_STRUCT_TRAITS_MEMBER(cors_error_status)
  IPC_STRUCT_TRAITS_MEMBER(ssl_info)
  IPC_STRUCT_TRAITS_MEMBER(blocked_cross_site_document)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(net::URLRequest::ReferrerPolicy,
                          net::URLRequest::MAX_REFERRER_POLICY - 1)

IPC_STRUCT_TRAITS_BEGIN(net::RedirectInfo)
  IPC_STRUCT_TRAITS_MEMBER(status_code)
  IPC_STRUCT_TRAITS_MEMBER(new_method)
  IPC_STRUCT_TRAITS_MEMBER(new_url)
  IPC_STRUCT_TRAITS_MEMBER(new_site_for_cookies)
  IPC_STRUCT_TRAITS_MEMBER(new_referrer)
  IPC_STRUCT_TRAITS_MEMBER(new_referrer_policy)
  IPC_STRUCT_TRAITS_MEMBER(referred_token_binding_host)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::ResourceResponseInfo)
  IPC_STRUCT_TRAITS_MEMBER(request_time)
  IPC_STRUCT_TRAITS_MEMBER(response_time)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(mime_type)
  IPC_STRUCT_TRAITS_MEMBER(charset)
  IPC_STRUCT_TRAITS_MEMBER(is_legacy_symantec_cert)
  IPC_STRUCT_TRAITS_MEMBER(cert_validity_start)
  IPC_STRUCT_TRAITS_MEMBER(content_length)
  IPC_STRUCT_TRAITS_MEMBER(encoded_data_length)
  IPC_STRUCT_TRAITS_MEMBER(encoded_body_length)
  IPC_STRUCT_TRAITS_MEMBER(appcache_id)
  IPC_STRUCT_TRAITS_MEMBER(appcache_manifest_url)
  IPC_STRUCT_TRAITS_MEMBER(load_timing)
  IPC_STRUCT_TRAITS_MEMBER(raw_request_response_info)
  IPC_STRUCT_TRAITS_MEMBER(download_file_path)
  IPC_STRUCT_TRAITS_MEMBER(was_fetched_via_spdy)
  IPC_STRUCT_TRAITS_MEMBER(was_alpn_negotiated)
  IPC_STRUCT_TRAITS_MEMBER(was_alternate_protocol_available)
  IPC_STRUCT_TRAITS_MEMBER(connection_info)
  IPC_STRUCT_TRAITS_MEMBER(alpn_negotiated_protocol)
  IPC_STRUCT_TRAITS_MEMBER(socket_address)
  IPC_STRUCT_TRAITS_MEMBER(was_fetched_via_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(was_fallback_required_by_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(url_list_via_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(response_type_via_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(service_worker_start_time)
  IPC_STRUCT_TRAITS_MEMBER(service_worker_ready_time)
  IPC_STRUCT_TRAITS_MEMBER(is_in_cache_storage)
  IPC_STRUCT_TRAITS_MEMBER(cache_storage_cache_name)
  IPC_STRUCT_TRAITS_MEMBER(did_service_worker_navigation_preload)
  IPC_STRUCT_TRAITS_MEMBER(previews_state)
  IPC_STRUCT_TRAITS_MEMBER(effective_connection_type)
  IPC_STRUCT_TRAITS_MEMBER(certificate)
  IPC_STRUCT_TRAITS_MEMBER(cert_status)
  IPC_STRUCT_TRAITS_MEMBER(ssl_connection_status)
  IPC_STRUCT_TRAITS_MEMBER(ssl_key_exchange_group)
  IPC_STRUCT_TRAITS_MEMBER(signed_certificate_timestamps)
  IPC_STRUCT_TRAITS_MEMBER(cors_exposed_header_names)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(net::HttpResponseInfo::ConnectionInfo,
                          net::HttpResponseInfo::NUM_OF_CONNECTION_INFOS - 1)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::FetchResponseType,
                          network::mojom::FetchResponseType::kLast)

IPC_ENUM_TRAITS_MAX_VALUE(net::EffectiveConnectionType,
                          net::EFFECTIVE_CONNECTION_TYPE_LAST - 1)

IPC_STRUCT_TRAITS_BEGIN(network::ResourceResponseHead)
  IPC_STRUCT_TRAITS_PARENT(network::ResourceResponseInfo)
  IPC_STRUCT_TRAITS_MEMBER(request_start)
  IPC_STRUCT_TRAITS_MEMBER(response_start)
IPC_STRUCT_TRAITS_END()

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_
