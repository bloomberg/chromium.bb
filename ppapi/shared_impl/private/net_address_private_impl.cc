// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/private/net_address_private_impl.h"

#if defined(OS_WIN)
#include <windows.h>
#include <ws2tcpip.h>
#elif defined(OS_POSIX)
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include <string.h>

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/thunk.h"

#if defined(OS_MACOSX)
// This is a bit evil, but it's standard operating procedure for |s6_addr|....
#define s6_addr16 __u6_addr.__u6_addr16
#endif

#if defined(OS_WIN)
// The type of |sockaddr::sa_family|.
typedef ADDRESS_FAMILY sa_family_t;

#define s6_addr16 u.Word
#define ntohs(x) _byteswap_ushort(x)
#define htons(x) _byteswap_ushort(x)
#endif  // defined(OS_WIN)

// The net address interface doesn't have a normal C -> C++ thunk since it
// doesn't actually have any proxy wrapping or associated objects; it's just a
// call into base. So we implement the entire interface here, using the thunk
// namespace so it magically gets hooked up in the proper places.

namespace ppapi {

namespace {

static const size_t kIPv4AddressSize = 4;
static const size_t kIPv6AddressSize = 16;

bool IPEndPointToSockaddr(const std::vector<unsigned char>& address,
                          int port,
                          sockaddr* sa) {
  DCHECK(sa);
  switch (address.size()) {
    case kIPv4AddressSize: {
      struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(sa);
      memset(addr, 0, sizeof(struct sockaddr_in));
      addr->sin_family = AF_INET;
      addr->sin_port = base::HostToNet16(port);
      memcpy(&addr->sin_addr, &address[0], static_cast<int>(address.size()));
      break;
    }
    case kIPv6AddressSize: {
      struct sockaddr_in6* addr6 =
          reinterpret_cast<struct sockaddr_in6*>(sa);
      memset(addr6, 0, sizeof(struct sockaddr_in6));
      addr6->sin6_family = AF_INET6;
      addr6->sin6_port = base::HostToNet16(port);
      memcpy(&addr6->sin6_addr, &address[0], static_cast<int>(address.size()));
      break;
    }
    default:
      return false;
  }
  return true;
}

bool SockaddrToIPEndPoint(const sockaddr& sa,
                          std::vector<unsigned char>* address,
                          int* port) {
  DCHECK(address);
  DCHECK(port);
  switch (sa.sa_family) {
    case AF_INET: {
      const struct sockaddr_in* addr =
          reinterpret_cast<const struct sockaddr_in*>(&sa);
      *port = base::NetToHost16(addr->sin_port);
      const char* bytes = reinterpret_cast<const char*>(&addr->sin_addr);
      address->assign(&bytes[0], &bytes[kIPv4AddressSize]);
      break;
    }
    case AF_INET6: {
      const struct sockaddr_in6* addr =
          reinterpret_cast<const struct sockaddr_in6*>(&sa);
      *port = base::NetToHost16(addr->sin6_port);
      const char* bytes = reinterpret_cast<const char*>(&addr->sin6_addr);
      address->assign(&bytes[0], &bytes[kIPv6AddressSize]);
      break;
    }
    default:
      return false;
  }
  return true;
}

// This assert fails on OpenBSD for an unknown reason at the moment.
#if !defined(OS_OPENBSD)
// Make sure the storage in |PP_NetAddress_Private| is big enough. (Do it here
// since the data is opaque elsewhere.)
COMPILE_ASSERT(sizeof(reinterpret_cast<PP_NetAddress_Private*>(0)->data) >=
               sizeof(sockaddr_storage), PP_NetAddress_Private_data_too_small);
#endif

sa_family_t GetFamilyInternal(const PP_NetAddress_Private* addr) {
  return reinterpret_cast<const sockaddr*>(addr->data)->sa_family;
}

PP_NetAddressFamily_Private GetFamily(const PP_NetAddress_Private* addr) {
  switch (GetFamilyInternal(addr)) {
    case AF_INET:
      return PP_NETADDRESSFAMILY_IPV4;
    case AF_INET6:
      return PP_NETADDRESSFAMILY_IPV6;
    default:
      return PP_NETADDRESSFAMILY_UNSPECIFIED;
  }
}

uint16_t GetPort(const PP_NetAddress_Private* addr) {
  switch (GetFamilyInternal(addr)) {
    case AF_INET: {
      const sockaddr_in* a = reinterpret_cast<const sockaddr_in*>(addr->data);
      return base::NetToHost16(a->sin_port);
    }
    case AF_INET6: {
      const sockaddr_in6* a = reinterpret_cast<const sockaddr_in6*>(addr->data);
      return base::NetToHost16(a->sin6_port);
    }
    default:
      return 0;
  }
}

PP_Bool GetAddress(const PP_NetAddress_Private* addr,
                   void* address,
                   uint16_t address_size) {
  switch (GetFamilyInternal(addr)) {
    case AF_INET: {
      const sockaddr_in* a = reinterpret_cast<const sockaddr_in*>(addr->data);
      if (address_size >= sizeof(a->sin_addr.s_addr)) {
        memcpy(address, &(a->sin_addr.s_addr), sizeof(a->sin_addr.s_addr));
        return PP_TRUE;
      }
      break;
    }
    case AF_INET6: {
      const sockaddr_in6* a = reinterpret_cast<const sockaddr_in6*>(addr->data);
      if (address_size >= sizeof(a->sin6_addr.s6_addr)) {
        memcpy(address, &(a->sin6_addr.s6_addr), sizeof(a->sin6_addr.s6_addr));
        return PP_TRUE;
      }
      break;
    }
    default:
      break;
  }

  return PP_FALSE;
}

uint32_t GetScopeID(const PP_NetAddress_Private* addr) {
  switch (GetFamilyInternal(addr)) {
    case AF_INET6: {
      const sockaddr_in6* a = reinterpret_cast<const sockaddr_in6*>(addr->data);
      return a->sin6_scope_id;
    }
    default:
      return 0;
  }
}

PP_Bool AreHostsEqual(const PP_NetAddress_Private* addr1,
                      const PP_NetAddress_Private* addr2) {
  if (!NetAddressPrivateImpl::ValidateNetAddress(*addr1) ||
      !NetAddressPrivateImpl::ValidateNetAddress(*addr2))
    return PP_FALSE;

  sa_family_t addr1_family = GetFamilyInternal(addr1);
  if (addr1_family != GetFamilyInternal(addr2))
    return PP_FALSE;

  switch (addr1_family) {
    case AF_INET: {
      const sockaddr_in* a1 = reinterpret_cast<const sockaddr_in*>(addr1->data);
      const sockaddr_in* a2 = reinterpret_cast<const sockaddr_in*>(addr2->data);
      return PP_FromBool(a1->sin_addr.s_addr == a2->sin_addr.s_addr);
    }
    case AF_INET6: {
      const sockaddr_in6* a1 =
          reinterpret_cast<const sockaddr_in6*>(addr1->data);
      const sockaddr_in6* a2 =
          reinterpret_cast<const sockaddr_in6*>(addr2->data);
      return PP_FromBool(a1->sin6_flowinfo == a2->sin6_flowinfo &&
                         memcmp(&a1->sin6_addr, &a2->sin6_addr,
                                sizeof(a1->sin6_addr)) == 0 &&
                         a1->sin6_scope_id == a2->sin6_scope_id);
    }
    default:
      return PP_FALSE;
  }
}

PP_Bool AreEqual(const PP_NetAddress_Private* addr1,
                 const PP_NetAddress_Private* addr2) {
  // |AreHostsEqual()| will also validate the addresses and return false if
  // either is invalid.
  if (!AreHostsEqual(addr1, addr2))
    return PP_FALSE;

  // Note: Here, we know that |addr1| and |addr2| have the same family.
  switch (GetFamilyInternal(addr1)) {
    case AF_INET: {
      const sockaddr_in* a1 = reinterpret_cast<const sockaddr_in*>(addr1->data);
      const sockaddr_in* a2 = reinterpret_cast<const sockaddr_in*>(addr2->data);
      return PP_FromBool(a1->sin_port == a2->sin_port);
    }
    case AF_INET6: {
      const sockaddr_in6* a1 =
          reinterpret_cast<const sockaddr_in6*>(addr1->data);
      const sockaddr_in6* a2 =
          reinterpret_cast<const sockaddr_in6*>(addr2->data);
      return PP_FromBool(a1->sin6_port == a2->sin6_port);
    }
    default:
      return PP_FALSE;
  }
}

std::string ConvertIPv4AddressToString(const sockaddr_in* a,
                                       bool include_port) {
  unsigned ip = base::NetToHost32(a->sin_addr.s_addr);
  unsigned port = base::NetToHost16(a->sin_port);
  std::string description = base::StringPrintf(
      "%u.%u.%u.%u",
      (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
  if (include_port)
    base::StringAppendF(&description, ":%u", port);
  return description;
}

// Format an IPv6 address for human consumption, basically according to RFC
// 5952.
//  - If the scope is nonzero, it is appended to the address as "%<scope>" (this
//    is not in RFC 5952, but consistent with |getnameinfo()| on Linux and
//    Windows).
//  - If |include_port| is true, the address (possibly including the scope) is
//    enclosed in square brackets and ":<port>" is appended, i.e., the overall
//    format is "[<address>]:<port>".
//  - If the address is an IPv4 address embedded IPv6 (per RFC 4291), then the
//    mixed format is used, e.g., "::ffff:192.168.1.2". This is optional per RFC
//    5952, but consistent with |getnameinfo()|.
std::string ConvertIPv6AddressToString(const sockaddr_in6* a,
                                       bool include_port) {
  unsigned port = base::NetToHost16(a->sin6_port);
  unsigned scope = a->sin6_scope_id;
  std::string description(include_port ? "[" : "");

  // IPv4 address embedded in IPv6.
  if (a->sin6_addr.s6_addr16[0] == 0 && a->sin6_addr.s6_addr16[1] == 0 &&
      a->sin6_addr.s6_addr16[2] == 0 && a->sin6_addr.s6_addr16[3] == 0 &&
      a->sin6_addr.s6_addr16[4] == 0 &&
      (a->sin6_addr.s6_addr16[5] == 0 || a->sin6_addr.s6_addr16[5] == 0xffff)) {
    base::StringAppendF(
        &description,
        a->sin6_addr.s6_addr16[5] == 0 ? "::%u.%u.%u.%u" : "::ffff:%u.%u.%u.%u",
        static_cast<unsigned>(a->sin6_addr.s6_addr[12]),
        static_cast<unsigned>(a->sin6_addr.s6_addr[13]),
        static_cast<unsigned>(a->sin6_addr.s6_addr[14]),
        static_cast<unsigned>(a->sin6_addr.s6_addr[15]));

  // "Real" IPv6 addresses.
  } else {
    // Find the first longest run of 0s (of length > 1), to collapse to "::".
    int longest_start = 0;
    int longest_length = 0;
    int curr_start = 0;
    int curr_length = 0;
    for (int i = 0; i < 8; i++) {
      if (base::NetToHost16(a->sin6_addr.s6_addr16[i]) != 0) {
        curr_length = 0;
      } else {
        if (!curr_length)
          curr_start = i;
        curr_length++;
        if (curr_length > longest_length) {
          longest_start = curr_start;
          longest_length = curr_length;
        }
      }
    }

    bool need_sep = false;  // Whether the next item needs a ':' to separate.
    for (int i = 0; i < 8;) {
      if (longest_length > 1 && i == longest_start) {
        description.append("::");
        need_sep = false;
        i += longest_length;
      } else {
        unsigned v = base::NetToHost16(a->sin6_addr.s6_addr16[i]);
        base::StringAppendF(&description, need_sep ? ":%x" : "%x", v);
        need_sep = true;
        i++;
      }
    }
  }

  // Nonzero scopes, e.g., 123, are indicated by appending, e.g., "%123".
  if (scope != 0)
    base::StringAppendF(&description, "%%%u", scope);

  if (include_port)
    base::StringAppendF(&description, "]:%u", port);

  return description;
}

PP_Var Describe(PP_Module /*module*/,
                const struct PP_NetAddress_Private* addr,
                PP_Bool include_port) {
  if (!NetAddressPrivateImpl::ValidateNetAddress(*addr))
    return PP_MakeUndefined();

  // On Windows, |NetAddressToString()| doesn't work in the sandbox. On Mac,
  // the output isn't consistent with RFC 5952, at least on Mac OS 10.6:
  // |getnameinfo()| collapses length-one runs of zeros (and also doesn't
  // display the scope).
  switch (GetFamilyInternal(addr)) {
    case AF_INET: {
      const sockaddr_in* a = reinterpret_cast<const sockaddr_in*>(addr->data);
      return StringVar::StringToPPVar(
          ConvertIPv4AddressToString(a, !!include_port));
    }
    case AF_INET6: {
      const sockaddr_in6* a = reinterpret_cast<const sockaddr_in6*>(addr->data);
      return StringVar::StringToPPVar(
          ConvertIPv6AddressToString(a, !!include_port));
    }
    default:
      NOTREACHED();
      break;
  }
  return PP_MakeUndefined();
}

PP_Bool ReplacePort(const struct PP_NetAddress_Private* src_addr,
                    uint16_t port,
                    struct PP_NetAddress_Private* dest_addr) {
  if (!NetAddressPrivateImpl::ValidateNetAddress(*src_addr))
    return PP_FALSE;

  switch (GetFamilyInternal(src_addr)) {
    case AF_INET: {
      memmove(dest_addr, src_addr, sizeof(*src_addr));
      reinterpret_cast<sockaddr_in*>(dest_addr->data)->sin_port =
          base::HostToNet16(port);
      return PP_TRUE;
    }
    case AF_INET6: {
      memmove(dest_addr, src_addr, sizeof(*src_addr));
      reinterpret_cast<sockaddr_in6*>(dest_addr->data)->sin6_port =
          base::HostToNet16(port);
      return PP_TRUE;
    }
    default:
      return PP_FALSE;
  }
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

void CreateFromIPv4Address(const uint8_t ip[4],
                           uint16_t port,
                           struct PP_NetAddress_Private* addr_out) {
  memset(addr_out->data, 0,
         arraysize(addr_out->data) * sizeof(addr_out->data[0]));
  sockaddr_in* a = reinterpret_cast<sockaddr_in*>(addr_out->data);
  addr_out->size = sizeof(*a);
  a->sin_family = AF_INET;
  memcpy(&(a->sin_addr), ip, sizeof(a->sin_addr));
  a->sin_port = htons(port);
}

void CreateFromIPv6Address(const uint8_t ip[16],
                           uint32_t scope_id,
                           uint16_t port,
                           struct PP_NetAddress_Private* addr_out) {
  memset(addr_out->data, 0,
         arraysize(addr_out->data) * sizeof(addr_out->data[0]));
  sockaddr_in6* a = reinterpret_cast<sockaddr_in6*>(addr_out->data);
  addr_out->size = sizeof(*a);
  a->sin6_family = AF_INET6;
  memcpy(&(a->sin6_addr), ip, sizeof(a->sin6_addr));
  a->sin6_port = htons(port);
  a->sin6_scope_id = scope_id;
}

const PPB_NetAddress_Private_0_1 net_address_private_interface_0_1 = {
  &AreEqual,
  &AreHostsEqual,
  &Describe,
  &ReplacePort,
  &GetAnyAddress
};

const PPB_NetAddress_Private_1_0 net_address_private_interface_1_0 = {
  &AreEqual,
  &AreHostsEqual,
  &Describe,
  &ReplacePort,
  &GetAnyAddress,
  &GetFamily,
  &GetPort,
  &GetAddress
};

const PPB_NetAddress_Private_1_1 net_address_private_interface_1_1 = {
  &AreEqual,
  &AreHostsEqual,
  &Describe,
  &ReplacePort,
  &GetAnyAddress,
  &GetFamily,
  &GetPort,
  &GetAddress,
  &GetScopeID,
  &CreateFromIPv4Address,
  &CreateFromIPv6Address
};

}  // namespace

namespace thunk {

PPAPI_THUNK_EXPORT const PPB_NetAddress_Private_0_1*
GetPPB_NetAddress_Private_0_1_Thunk() {
  return &net_address_private_interface_0_1;
}

PPAPI_THUNK_EXPORT const PPB_NetAddress_Private_1_0*
GetPPB_NetAddress_Private_1_0_Thunk() {
  return &net_address_private_interface_1_0;
}

PPAPI_THUNK_EXPORT const PPB_NetAddress_Private_1_1*
GetPPB_NetAddress_Private_1_1_Thunk() {
  return &net_address_private_interface_1_1;
}

}  // namespace thunk

// static
const PP_NetAddress_Private NetAddressPrivateImpl::kInvalidNetAddress = { 0 };

// static
bool NetAddressPrivateImpl::ValidateNetAddress(
    const PP_NetAddress_Private& addr) {
  if (addr.size < sizeof(reinterpret_cast<sockaddr*>(0)->sa_family))
    return false;

  // TODO(viettrungluu): more careful validation?
  switch (GetFamilyInternal(&addr)) {
    case AF_INET:
      // Just do a size check for AF_INET.
      if (addr.size >= sizeof(sockaddr_in))
        return true;
      break;
    case AF_INET6:
      // Ditto for AF_INET6.
      if (addr.size >= sizeof(sockaddr_in6))
        return true;
      break;
    default:
      break;
  }

  // Reject everything else.
  return false;
}

// static
bool NetAddressPrivateImpl::SockaddrToNetAddress(
    const sockaddr* sa,
    uint32_t sa_length,
    PP_NetAddress_Private* net_addr) {
  if (!sa || sa_length == 0 || !net_addr)
    return false;

  CHECK_LE(sa_length, sizeof(net_addr->data));
  net_addr->size = sa_length;
  memcpy(net_addr->data, sa, net_addr->size);
  return true;
}

// static
bool NetAddressPrivateImpl::IPEndPointToNetAddress(
    const std::vector<unsigned char>& address,
    int port,
    PP_NetAddress_Private* net_addr) {
  sockaddr_storage storage;
  sockaddr* sa = reinterpret_cast<sockaddr*>(&storage);
  if (!IPEndPointToSockaddr(address, port, sa))
    return false;
  return SockaddrToNetAddress(sa, sizeof(storage), net_addr);
}

// static
bool NetAddressPrivateImpl::NetAddressToIPEndPoint(
    const PP_NetAddress_Private& net_addr,
    std::vector<unsigned char>* address,
    int* port) {
  if (!address || !port || !ValidateNetAddress(net_addr))
    return false;

  return SockaddrToIPEndPoint(
      reinterpret_cast<const sockaddr&>(*net_addr.data), address, port);
}

}  // namespace ppapi
