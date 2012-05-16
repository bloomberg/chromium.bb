// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_util.h"

#include "base/callback.h"
#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"

namespace remoting {

static void CallbackAdapter(void* user_data, int32_t result) {
  scoped_ptr<base::Callback<void(int)> > callback(
      reinterpret_cast<base::Callback<void(int)>*>(user_data));
  callback->Run(result);
}

pp::CompletionCallback PpCompletionCallback(
    base::Callback<void(int)> callback) {
  return pp::CompletionCallback(&CallbackAdapter,
                                new base::Callback<void(int)>(callback));
}

bool SocketAddressToPpAddressWithPort(const talk_base::SocketAddress& address,
                                      PP_NetAddress_Private* pp_address,
                                      uint16_t port) {
  bool result = false;
  switch (address.ipaddr().family()) {
    case AF_INET: {
      in_addr addr = address.ipaddr().ipv4_address();
      result = pp::NetAddressPrivate::CreateFromIPv4Address(
          reinterpret_cast<uint8_t*>(&addr), port, pp_address);
      break;
    }
    case AF_INET6: {
      in6_addr addr = address.ipaddr().ipv6_address();
      result = pp::NetAddressPrivate::CreateFromIPv6Address(
          addr.s6_addr, 0, port, pp_address);
      break;
    }
    default: {
      LOG(WARNING) << "Unknown address family: " << address.ipaddr().family();
    }
  }
  if (!result) {
    LOG(WARNING) << "Failed to convert address: " << address.ToString();
  }
  return result;
}

bool SocketAddressToPpAddress(const talk_base::SocketAddress& address,
                              PP_NetAddress_Private* pp_address) {
  return SocketAddressToPpAddressWithPort(address, pp_address, address.port());
}

bool PpAddressToSocketAddress(const PP_NetAddress_Private& pp_address,
                              talk_base::SocketAddress* address) {
  uint8_t addr_storage[16];
  bool result = pp::NetAddressPrivate::GetAddress(
      pp_address, &addr_storage, sizeof(addr_storage));

  if (result) {
    switch (pp::NetAddressPrivate::GetFamily(pp_address)) {
      case PP_NETADDRESSFAMILY_IPV4:
        address->SetIP(talk_base::IPAddress(
            *reinterpret_cast<in_addr*>(addr_storage)));
        break;
      case PP_NETADDRESSFAMILY_IPV6:
        address->SetIP(talk_base::IPAddress(
            *reinterpret_cast<in6_addr*>(addr_storage)));
        break;
      default:
        result = false;
    }
  }

  if (!result) {
    LOG(WARNING) << "Failed to convert address: "
                 << pp::NetAddressPrivate::Describe(pp_address, true);
  } else {
    address->SetPort(pp::NetAddressPrivate::GetPort(pp_address));
  }
  return result;
}

}  // namespace remoting
