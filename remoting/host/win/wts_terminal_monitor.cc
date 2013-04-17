// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/wts_terminal_monitor.h"

#include <windows.h>
#include <winternl.h>
#include <wtsapi32.h>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "base/utf_string_conversions.h"
#include "net/base/ip_endpoint.h"

namespace {

// Used to query the endpoint of an attached RDP client.
const WINSTATIONINFOCLASS kWinStationRemoteAddress =
    static_cast<WINSTATIONINFOCLASS>(29);

// WinStationRemoteAddress information class returns the following structure.
// Note that its layout is different from sockaddr_in/sockaddr_in6. For
// instance both |ipv4| and |ipv6| structures are 4 byte aligned so there is
// additional 2 byte padding after |sin_family|.
struct RemoteAddress {
  unsigned short sin_family;
  union {
    struct {
      USHORT sin_port;
      ULONG in_addr;
      UCHAR sin_zero[8];
    } ipv4;
    struct {
      USHORT sin6_port;
      ULONG sin6_flowinfo;
      USHORT sin6_addr[8];
      ULONG sin6_scope_id;
    } ipv6;
  };
};

// Loads winsta.dll dynamically and resolves the address of
// the winsta!WinStationQueryInformationW() function.
class WinstaLoader {
 public:
  WinstaLoader();
  ~WinstaLoader();

  // Returns the address and port of the RDP client attached to |session_id|.
  bool GetRemoteAddress(uint32 session_id, RemoteAddress* address);

 private:
  // Handle of dynamically loaded winsta.dll.
  base::ScopedNativeLibrary winsta_;

  // Points to winsta!WinStationQueryInformationW().
  PWINSTATIONQUERYINFORMATIONW win_station_query_information_;

  DISALLOW_COPY_AND_ASSIGN(WinstaLoader);
};

static base::LazyInstance<WinstaLoader> g_winsta = LAZY_INSTANCE_INITIALIZER;

WinstaLoader::WinstaLoader() :
  winsta_(base::FilePath(base::GetNativeLibraryName(UTF8ToUTF16("winsta")))) {

  // Resolve the function pointer.
  win_station_query_information_ =
      static_cast<PWINSTATIONQUERYINFORMATIONW>(
          winsta_.GetFunctionPointer("WinStationQueryInformationW"));
}

WinstaLoader::~WinstaLoader() {
}

bool WinstaLoader::GetRemoteAddress(uint32 session_id, RemoteAddress* address) {
  ULONG length;
  return win_station_query_information_(WTS_CURRENT_SERVER_HANDLE,
                                        session_id,
                                        kWinStationRemoteAddress,
                                        address,
                                        sizeof(*address),
                                        &length) != FALSE;
}

}  // namespace

namespace remoting {

// Session id that does not represent any session.
const uint32 kInvalidSessionId = 0xffffffffu;

WtsTerminalMonitor::~WtsTerminalMonitor() {
}

// static
bool WtsTerminalMonitor::GetEndpointForSessionId(uint32 session_id,
                                                 net::IPEndPoint* endpoint) {
  // Fast path for the case when |session_id| is currently attached to
  // the physical console.
  if (session_id == WTSGetActiveConsoleSessionId()) {
    *endpoint = net::IPEndPoint();
    return true;
  }

  RemoteAddress address;
  // WinStationQueryInformationW() fails if no RDP client is attached to
  // |session_id|.
  if (!g_winsta.Get().GetRemoteAddress(session_id, &address))
    return false;

  // Convert the RemoteAddress structure into sockaddr_in/sockaddr_in6.
  switch (address.sin_family) {
    case AF_INET: {
      sockaddr_in ipv4 = { 0 };
      ipv4.sin_family = AF_INET;
      ipv4.sin_port = address.ipv4.sin_port;
      ipv4.sin_addr.S_un.S_addr = address.ipv4.in_addr;
      return endpoint->FromSockAddr(
          reinterpret_cast<struct sockaddr*>(&ipv4), sizeof(ipv4));
    }

    case AF_INET6: {
      sockaddr_in6 ipv6 = { 0 };
      ipv6.sin6_family = AF_INET6;
      ipv6.sin6_port = address.ipv6.sin6_port;
      ipv6.sin6_flowinfo = address.ipv6.sin6_flowinfo;
      memcpy(&ipv6.sin6_addr, address.ipv6.sin6_addr, sizeof(ipv6.sin6_addr));
      ipv6.sin6_scope_id = address.ipv6.sin6_scope_id;
      return endpoint->FromSockAddr(
          reinterpret_cast<struct sockaddr*>(&ipv6), sizeof(ipv6));
    }

    default:
      return false;
  }
}

// static
uint32 WtsTerminalMonitor::GetSessionIdForEndpoint(
    const net::IPEndPoint& client_endpoint) {
  // Use the fast path if the caller wants to get id of the session attached to
  // the physical console.
  if (client_endpoint == net::IPEndPoint())
    return WTSGetActiveConsoleSessionId();

  // Enumerate all sessions and try to match the client endpoint.
  WTS_SESSION_INFO* session_info;
  DWORD session_info_count;
  if (!WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &session_info,
                            &session_info_count)) {
    LOG_GETLASTERROR(ERROR) << "Failed to enumerate all sessions";
    return kInvalidSessionId;
  }
  for (DWORD i = 0; i < session_info_count; ++i) {
    net::IPEndPoint endpoint;
    if (GetEndpointForSessionId(session_info[i].SessionId, &endpoint) &&
        endpoint == client_endpoint) {
      uint32 session_id = session_info[i].SessionId;
      WTSFreeMemory(session_info);
      return session_id;
    }
  }

  // |client_endpoint| is not associated with any session.
  WTSFreeMemory(session_info);
  return kInvalidSessionId;
}

WtsTerminalMonitor::WtsTerminalMonitor() {
}

}  // namespace remoting
