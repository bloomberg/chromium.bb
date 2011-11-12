// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/private/net_address_private_impl.h"

#include <string.h>

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "build/build_config.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/sys_byteorder.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/thunk.h"

// The net address interface doesn't have a normal C -> C++ thunk since it
// doesn't actually have any proxy wrapping or associated objects; it's just a
// call into base. So we implement the entire interface here, using the thunk
// namespace so it magically gets hooked up in the proper places.

namespace ppapi {

namespace {

#if defined(OS_WIN)
// The type of |sockaddr::sa_family|.
typedef ADDRESS_FAMILY sa_family_t;
#endif

inline sa_family_t GetFamily(const PP_NetAddress_Private& addr) {
  return reinterpret_cast<const sockaddr*>(addr.data)->sa_family;
}

PP_Bool AreHostsEqual(const PP_NetAddress_Private* addr1,
                      const PP_NetAddress_Private* addr2) {
  if (!NetAddressPrivateImpl::ValidateNetAddress(*addr1) ||
      !NetAddressPrivateImpl::ValidateNetAddress(*addr2))
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

PP_Bool AreEqual(const PP_NetAddress_Private* addr1,
                 const PP_NetAddress_Private* addr2) {
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
                const struct PP_NetAddress_Private* addr,
                PP_Bool include_port) {
  if (!NetAddressPrivateImpl::ValidateNetAddress(*addr))
    return PP_MakeUndefined();

#if defined(OS_WIN)
  // On Windows, |NetAddressToString()| doesn't work in the sandbox.
  // TODO(viettrungluu): Consider switching to this everywhere once it's fully
  // implemented.
  switch (GetFamily(*addr)) {
    case AF_INET: {
      const sockaddr_in* a = reinterpret_cast<const sockaddr_in*>(addr->data);
      unsigned ip = ntohl(a->sin_addr.s_addr);
      unsigned port = ntohs(a->sin_port);
      std::string description = base::StringPrintf(
          "%u.%u.%u.%u",
          (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
      if (include_port)
        description.append(base::StringPrintf(":%u", port));
      return StringVar::StringToPPVar(module, description);
    }
    case AF_INET6:
      // TODO(viettrungluu): crbug.com/103969
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED();
      break;
  }
  return PP_MakeUndefined();
#else
  const sockaddr* a = reinterpret_cast<const sockaddr*>(addr->data);
  socklen_t l = addr->size;
  std::string description =
      include_port ? net::NetAddressToStringWithPort(a, l) :
                     net::NetAddressToString(a, l);
  return StringVar::StringToPPVar(module, description);
#endif
}

PP_Bool ReplacePort(const struct PP_NetAddress_Private* src_addr,
                    uint16_t port,
                    struct PP_NetAddress_Private* dest_addr) {
  if (!NetAddressPrivateImpl::ValidateNetAddress(*src_addr))
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

void GetAnyAddress(PP_Bool is_ipv6, PP_NetAddress_Private* addr) {
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

const PPB_NetAddress_Private net_address_private_interface = {
  &AreEqual,
  &AreHostsEqual,
  &Describe,
  &ReplacePort,
  &GetAnyAddress
};

}  // namespace

namespace thunk {

PPAPI_THUNK_EXPORT const PPB_NetAddress_Private*
GetPPB_NetAddress_Private_Thunk() {
  return &net_address_private_interface;
}

}  // namespace thunk

// static
bool NetAddressPrivateImpl::ValidateNetAddress(
    const PP_NetAddress_Private& addr) {
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
