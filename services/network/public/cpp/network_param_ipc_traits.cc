// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_param_ipc_traits.h"

#include "ipc/ipc_message_utils.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/http_raw_request_response_info.h"

namespace IPC {

void ParamTraits<net::CertVerifyResult>::Write(base::Pickle* m,
                                               const param_type& p) {
  WriteParam(m, p.verified_cert);
  WriteParam(m, p.cert_status);
  WriteParam(m, p.has_md2);
  WriteParam(m, p.has_md4);
  WriteParam(m, p.has_md5);
  WriteParam(m, p.has_sha1);
  WriteParam(m, p.has_sha1_leaf);
  WriteParam(m, p.public_key_hashes);
  WriteParam(m, p.is_issued_by_known_root);
  WriteParam(m, p.is_issued_by_additional_trust_anchor);
  WriteParam(m, p.common_name_fallback_used);
  WriteParam(m, p.ocsp_result);
}

bool ParamTraits<net::CertVerifyResult>::Read(const base::Pickle* m,
                                              base::PickleIterator* iter,
                                              param_type* r) {
  return ReadParam(m, iter, &r->verified_cert) &&
         ReadParam(m, iter, &r->cert_status) &&
         ReadParam(m, iter, &r->has_md2) && ReadParam(m, iter, &r->has_md4) &&
         ReadParam(m, iter, &r->has_md5) && ReadParam(m, iter, &r->has_sha1) &&
         ReadParam(m, iter, &r->has_sha1_leaf) &&
         ReadParam(m, iter, &r->public_key_hashes) &&
         ReadParam(m, iter, &r->is_issued_by_known_root) &&
         ReadParam(m, iter, &r->is_issued_by_additional_trust_anchor) &&
         ReadParam(m, iter, &r->common_name_fallback_used) &&
         ReadParam(m, iter, &r->ocsp_result);
}

void ParamTraits<net::CertVerifyResult>::Log(const param_type& p,
                                             std::string* l) {
  l->append("<CertVerifyResult>");
}

void ParamTraits<net::HashValue>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.ToString());
}

bool ParamTraits<net::HashValue>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* r) {
  std::string str;
  return ReadParam(m, iter, &str) && r->FromString(str);
}

void ParamTraits<net::HashValue>::Log(const param_type& p, std::string* l) {
  l->append("<HashValue>");
}

void ParamTraits<net::HostPortPair>::Write(base::Pickle* m,
                                           const param_type& p) {
  WriteParam(m, p.host());
  WriteParam(m, p.port());
}

bool ParamTraits<net::HostPortPair>::Read(const base::Pickle* m,
                                          base::PickleIterator* iter,
                                          param_type* r) {
  std::string host;
  uint16_t port;
  if (!ReadParam(m, iter, &host) || !ReadParam(m, iter, &port))
    return false;

  r->set_host(host);
  r->set_port(port);
  return true;
}

void ParamTraits<net::HostPortPair>::Log(const param_type& p, std::string* l) {
  l->append(p.ToString());
}

void ParamTraits<net::HttpRequestHeaders>::Write(base::Pickle* m,
                                                 const param_type& p) {
  WriteParam(m, static_cast<int>(p.GetHeaderVector().size()));
  for (size_t i = 0; i < p.GetHeaderVector().size(); ++i)
    WriteParam(m, p.GetHeaderVector()[i]);
}

bool ParamTraits<net::HttpRequestHeaders>::Read(const base::Pickle* m,
                                                base::PickleIterator* iter,
                                                param_type* r) {
  // Sanity check.
  int size;
  if (!iter->ReadLength(&size))
    return false;
  for (int i = 0; i < size; ++i) {
    net::HttpRequestHeaders::HeaderKeyValuePair pair;
    if (!ReadParam(m, iter, &pair) ||
        !net::HttpUtil::IsValidHeaderName(pair.key) ||
        !net::HttpUtil::IsValidHeaderValue(pair.value))
      return false;
    r->SetHeader(pair.key, pair.value);
  }
  return true;
}

void ParamTraits<net::HttpRequestHeaders>::Log(const param_type& p,
                                               std::string* l) {
  l->append(p.ToString());
}

void ParamTraits<scoped_refptr<net::HttpResponseHeaders>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.get() != nullptr);
  if (p.get()) {
    // Do not disclose Set-Cookie headers over IPC.
    p->Persist(m, net::HttpResponseHeaders::PERSIST_SANS_COOKIES);
  }
}

bool ParamTraits<scoped_refptr<net::HttpResponseHeaders>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (has_object)
    *r = new net::HttpResponseHeaders(iter);
  return true;
}

void ParamTraits<scoped_refptr<net::HttpResponseHeaders>>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<HttpResponseHeaders>");
}

void ParamTraits<net::OCSPVerifyResult>::Write(base::Pickle* m,
                                               const param_type& p) {
  WriteParam(m, p.response_status);
  WriteParam(m, p.revocation_status);
}

bool ParamTraits<net::OCSPVerifyResult>::Read(const base::Pickle* m,
                                              base::PickleIterator* iter,
                                              param_type* r) {
  return ReadParam(m, iter, &r->response_status) &&
         ReadParam(m, iter, &r->revocation_status);
}

void ParamTraits<net::OCSPVerifyResult>::Log(const param_type& p,
                                             std::string* l) {
  l->append("<OCSPVerifyResult>");
}

void ParamTraits<net::SSLInfo>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.is_valid());
  if (!p.is_valid())
    return;
  WriteParam(m, p.cert);
  WriteParam(m, p.unverified_cert);
  WriteParam(m, p.cert_status);
  WriteParam(m, p.security_bits);
  WriteParam(m, p.key_exchange_group);
  WriteParam(m, p.connection_status);
  WriteParam(m, p.is_issued_by_known_root);
  WriteParam(m, p.pkp_bypassed);
  WriteParam(m, p.client_cert_sent);
  WriteParam(m, p.channel_id_sent);
  WriteParam(m, p.token_binding_negotiated);
  WriteParam(m, p.token_binding_key_param);
  WriteParam(m, p.handshake_type);
  WriteParam(m, p.public_key_hashes);
  WriteParam(m, p.pinning_failure_log);
  WriteParam(m, p.signed_certificate_timestamps);
  WriteParam(m, p.ct_policy_compliance);
  WriteParam(m, p.ocsp_result);
  WriteParam(m, p.is_fatal_cert_error);
}

bool ParamTraits<net::SSLInfo>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     param_type* r) {
  bool is_valid = false;
  if (!ReadParam(m, iter, &is_valid))
    return false;
  if (!is_valid)
    return true;
  return ReadParam(m, iter, &r->cert) &&
         ReadParam(m, iter, &r->unverified_cert) &&
         ReadParam(m, iter, &r->cert_status) &&
         ReadParam(m, iter, &r->security_bits) &&
         ReadParam(m, iter, &r->key_exchange_group) &&
         ReadParam(m, iter, &r->connection_status) &&
         ReadParam(m, iter, &r->is_issued_by_known_root) &&
         ReadParam(m, iter, &r->pkp_bypassed) &&
         ReadParam(m, iter, &r->client_cert_sent) &&
         ReadParam(m, iter, &r->channel_id_sent) &&
         ReadParam(m, iter, &r->token_binding_negotiated) &&
         ReadParam(m, iter, &r->token_binding_key_param) &&
         ReadParam(m, iter, &r->handshake_type) &&
         ReadParam(m, iter, &r->public_key_hashes) &&
         ReadParam(m, iter, &r->pinning_failure_log) &&
         ReadParam(m, iter, &r->signed_certificate_timestamps) &&
         ReadParam(m, iter, &r->ct_policy_compliance) &&
         ReadParam(m, iter, &r->ocsp_result) &&
         ReadParam(m, iter, &r->is_fatal_cert_error);
}

void ParamTraits<net::SSLInfo>::Log(const param_type& p, std::string* l) {
  l->append("<SSLInfo>");
}

void ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.get() != nullptr);
  if (p.get())
    p->Persist(m);
}

bool ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (has_object)
    *r = net::ct::SignedCertificateTimestamp::CreateFromPickle(iter);
  return true;
}

void ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<SignedCertificateTimestamp>");
}

void ParamTraits<scoped_refptr<network::HttpRawRequestResponseInfo>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.get() != nullptr);
  if (!p.get())
    return;

  WriteParam(m, p->http_status_code);
  WriteParam(m, p->http_status_text);
  WriteParam(m, p->request_headers);
  WriteParam(m, p->response_headers);
  WriteParam(m, p->request_headers_text);
  WriteParam(m, p->response_headers_text);
}

bool ParamTraits<scoped_refptr<network::HttpRawRequestResponseInfo>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  *r = new network::HttpRawRequestResponseInfo();
  return ReadParam(m, iter, &(*r)->http_status_code) &&
         ReadParam(m, iter, &(*r)->http_status_text) &&
         ReadParam(m, iter, &(*r)->request_headers) &&
         ReadParam(m, iter, &(*r)->response_headers) &&
         ReadParam(m, iter, &(*r)->request_headers_text) &&
         ReadParam(m, iter, &(*r)->response_headers_text);
}

void ParamTraits<scoped_refptr<network::HttpRawRequestResponseInfo>>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  if (p.get()) {
    LogParam(p->request_headers, l);
    l->append(", ");
    LogParam(p->response_headers, l);
  }
  l->append(")");
}

void ParamTraits<scoped_refptr<net::X509Certificate>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, !!p);
  if (p)
    p->Persist(m);
}

bool ParamTraits<scoped_refptr<net::X509Certificate>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  DCHECK(!*r);
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  *r = net::X509Certificate::CreateFromPickle(iter);
  return !!r->get();
}

void ParamTraits<scoped_refptr<net::X509Certificate>>::Log(const param_type& p,
                                                           std::string* l) {
  l->append("<X509Certificate>");
}

}  // namespace IPC

// Generation of IPC definitions.

// Generate constructors.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_
#include "ipc/struct_constructor_macros.h"
#include "network_param_ipc_traits.h"

// Generate destructors.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_
#include "ipc/struct_destructor_macros.h"
#include "network_param_ipc_traits.h"

// Generate param traits write methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "network_param_ipc_traits.h"
}  // namespace IPC

// Generate param traits read methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "network_param_ipc_traits.h"
}  // namespace IPC

// Generate param traits log methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_IPC_TRAITS_H_
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "network_param_ipc_traits.h"
}  // namespace IPC
