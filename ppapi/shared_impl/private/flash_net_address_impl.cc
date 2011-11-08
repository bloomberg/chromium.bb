// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/private/flash_net_address_impl.h"

#include <string.h>

#include <string>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/sys_byteorder.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_flash_net_address.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/thunk.h"

// The Flash net address interface doesn't have a normal C -> C++ thunk since it
// doesn't actually have any proxy wrapping or associated objects; it's just a
// call into base. So we implement the entire interface here, using the thunk
// namespace so it magically gets hooked up in the proper places.

namespace ppapi {

namespace {

#if defined(OS_WIN)
// The type of |sockaddr::sa_family|.
typedef ADDRESS_FAMILY sa_family_t;
#endif

inline sa_family_t GetFamily(const PP_Flash_NetAddress& addr) {
  return reinterpret_cast<const sockaddr*>(addr.data)->sa_family;
}

PP_Bool AreHostsEqual(const PP_Flash_NetAddress* addr1,
                      const PP_Flash_NetAddress* addr2) {
  if (!FlashNetAddressImpl::ValidateNetAddress(*addr1) ||
      !FlashNetAddressImpl::ValidateNetAddress(*addr2))
    return PP_FALSE;

  if (GetFamily(*addr1) != GetFamily(*addr2))
    return PP_FALSE;

  if (GetFamily(*addr1) == AF_INET) {
    const sockaddr_in* a1 = reinterpret_cast<const sockaddr_in*>(addr1->data);
    const sockaddr_in* a2 = reinterpret_cast<const sockaddr_in*>(addr2->data);
    return PP_FromBool(a1->sin_addr.s_addr == a2->sin_addr.s_addr);
  }

  if (GetFamily(*addr1) == AF_INET6) {
    const sockaddr_in6* a1 = reinterpret_cast<const sockaddr_in6*>(addr1->data);
    const sockaddr_in6* a2 = reinterpret_cast<const sockaddr_in6*>(addr2->data);
    return PP_FromBool(a1->sin6_flowinfo == a2->sin6_flowinfo &&
                       memcmp(&a1->sin6_addr, &a2->sin6_addr,
                              sizeof(a1->sin6_addr)) == 0 &&
                       a1->sin6_scope_id == a2->sin6_scope_id);
  }

  return PP_FALSE;
}

PP_Bool AreEqual(const PP_Flash_NetAddress* addr1,
                 const PP_Flash_NetAddress* addr2) {
  // |AreHostsEqual()| will also validate the addresses and return false if
  // either is invalid.
  if (!AreHostsEqual(addr1, addr2))
    return PP_FALSE;

  // Note: Here, we know that |addr1| and |addr2| have the same family.
  if (GetFamily(*addr1) == AF_INET) {
    const sockaddr_in* a1 = reinterpret_cast<const sockaddr_in*>(addr1->data);
    const sockaddr_in* a2 = reinterpret_cast<const sockaddr_in*>(addr2->data);
    return PP_FromBool(a1->sin_port == a2->sin_port);
  }

  if (GetFamily(*addr1) == AF_INET6) {
    const sockaddr_in6* a1 = reinterpret_cast<const sockaddr_in6*>(addr1->data);
    const sockaddr_in6* a2 = reinterpret_cast<const sockaddr_in6*>(addr2->data);
    return PP_FromBool(a1->sin6_port == a2->sin6_port);
  }

  return PP_FALSE;
}

PP_Var Describe(PP_Module module,
                const struct PP_Flash_NetAddress* addr,
                PP_Bool include_port) {
  if (!FlashNetAddressImpl::ValidateNetAddress(*addr))
    return PP_MakeUndefined();

  const sockaddr* a = reinterpret_cast<const sockaddr*>(addr->data);
  socklen_t l = addr->size;
  std::string description =
      include_port ? net::NetAddressToStringWithPort(a, l) :
                     net::NetAddressToString(a, l);
  return StringVar::StringToPPVar(module, description);
}

PP_Bool ReplacePort(const struct PP_Flash_NetAddress* src_addr,
                    uint16_t port,
                    struct PP_Flash_NetAddress* dest_addr) {
  if (!FlashNetAddressImpl::ValidateNetAddress(*src_addr))
    return PP_FALSE;

  if (GetFamily(*src_addr) == AF_INET) {
    memmove(dest_addr, src_addr, sizeof(*src_addr));
    reinterpret_cast<sockaddr_in*>(dest_addr->data)->sin_port = htons(port);
    return PP_TRUE;
  }

  if (GetFamily(*src_addr) == AF_INET6) {
    memmove(dest_addr, src_addr, sizeof(*src_addr));
    reinterpret_cast<sockaddr_in6*>(dest_addr->data)->sin6_port = htons(port);
    return PP_TRUE;
  }

  return PP_FALSE;
}

void GetAnyAddress(PP_Bool is_ipv6, struct PP_Flash_NetAddress* addr) {
  memset(addr->data, 0, arraysize(addr->data) * sizeof(addr->data[0]));
  if (is_ipv6) {
    sockaddr_in6* a = reinterpret_cast<sockaddr_in6*>(addr->data);
    addr->size = sizeof(*a);
    a->sin6_family = AF_INET6;
    a->sin6_addr = in6addr_any;
  } else {
    sockaddr_in* a = reinterpret_cast<sockaddr_in*>(addr->data);
    addr->size = sizeof(*a);
    a->sin_family = AF_INET;
    a->sin_addr.s_addr = INADDR_ANY;
  }
}

const PPB_Flash_NetAddress flash_net_address_interface = {
  &AreEqual,
  &AreHostsEqual,
  &Describe,
  &ReplacePort,
  &GetAnyAddress
};

}  // namespace

namespace thunk {

PPAPI_THUNK_EXPORT const PPB_Flash_NetAddress* GetPPB_Flash_NetAddress_Thunk() {
  return &flash_net_address_interface;
}

}  // namespace thunk

// static
bool FlashNetAddressImpl::ValidateNetAddress(const PP_Flash_NetAddress& addr) {
  if (addr.size < sizeof(reinterpret_cast<sockaddr*>(0)->sa_family))
    return false;

  // TODO(viettrungluu): more careful validation?
  // Just do a size check for AF_INET.
  if (GetFamily(addr) == AF_INET && addr.size >= sizeof(sockaddr_in))
    return true;

  // Ditto for AF_INET6.
  if (GetFamily(addr) == AF_INET6 && addr.size >= sizeof(sockaddr_in6))
    return true;

  // Reject everything else.
  return false;
}

}  // namespace ppapi
