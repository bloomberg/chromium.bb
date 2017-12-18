// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_param_ipc_traits.h"

#include "ipc/ipc_message_utils.h"
#include "net/http/http_util.h"

namespace IPC {

namespace {

void WriteCert(base::Pickle* m, net::X509Certificate* cert) {
  WriteParam(m, !!cert);
  if (cert)
    cert->Persist(m);
}

bool ReadCert(const base::Pickle* m,
              base::PickleIterator* iter,
              scoped_refptr<net::X509Certificate>* cert) {
  DCHECK(!*cert);
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  *cert = net::X509Certificate::CreateFromPickle(iter);
  return !!cert->get();
}

}  // namespace

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

void ParamTraits<net::SSLInfo>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.is_valid());
  if (!p.is_valid())
    return;
  WriteCert(m, p.cert.get());
  WriteCert(m, p.unverified_cert.get());
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
  WriteParam(m, p.ocsp_result.response_status);
  WriteParam(m, p.ocsp_result.revocation_status);
}

bool ParamTraits<net::SSLInfo>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     param_type* r) {
  bool is_valid = false;
  if (!ReadParam(m, iter, &is_valid))
    return false;
  if (!is_valid)
    return true;
  return ReadCert(m, iter, &r->cert) &&
         ReadCert(m, iter, &r->unverified_cert) &&
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
         ReadParam(m, iter, &r->ocsp_result.response_status) &&
         ReadParam(m, iter, &r->ocsp_result.revocation_status);
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
