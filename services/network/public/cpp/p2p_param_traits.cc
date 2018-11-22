// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/p2p_param_traits.h"

#include "ipc/ipc_message_utils.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

namespace IPC {

void ParamTraits<net::IPEndPoint>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.address());
  WriteParam(m, p.port());
}

bool ParamTraits<net::IPEndPoint>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        param_type* p) {
  net::IPAddress address;
  uint16_t port;
  if (!ReadParam(m, iter, &address) || !ReadParam(m, iter, &port))
    return false;
  if (!address.empty() && !address.IsValid())
    return false;

  *p = net::IPEndPoint(address, port);
  return true;
}

void ParamTraits<net::IPEndPoint>::Log(const param_type& p, std::string* l) {
  LogParam("IPEndPoint:" + p.ToString(), l);
}

void ParamTraits<net::IPAddress>::Write(base::Pickle* m, const param_type& p) {
  base::StackVector<uint8_t, 16> bytes;
  for (uint8_t byte : p.bytes())
    bytes->push_back(byte);
  WriteParam(m, bytes);
}

bool ParamTraits<net::IPAddress>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  base::StackVector<uint8_t, 16> bytes;
  if (!ReadParam(m, iter, &bytes))
    return false;
  if (bytes->size() && bytes->size() != net::IPAddress::kIPv4AddressSize &&
      bytes->size() != net::IPAddress::kIPv6AddressSize) {
    return false;
  }
  *p = net::IPAddress(bytes->data(), bytes->size());
  return true;
}

void ParamTraits<net::IPAddress>::Log(const param_type& p, std::string* l) {
  LogParam("IPAddress:" + (p.empty() ? "(empty)" : p.ToString()), l);
}

}  // namespace IPC

// Generation of IPC definitions.

// Generate constructors.
#undef SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
#include "ipc/struct_constructor_macros.h"
#include "p2p_param_traits.h"

// Generate param traits write methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "p2p_param_traits.h"
}  // namespace IPC

// Generate param traits read methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "p2p_param_traits.h"
}  // namespace IPC

// Generate param traits log methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "p2p_param_traits.h"
}  // namespace IPC
