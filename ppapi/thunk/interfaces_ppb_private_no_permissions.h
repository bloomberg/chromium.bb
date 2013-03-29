// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

// These interfaces don't require private permissions. However, they only work
// for whitelisted origins.
PROXIED_API(PPB_TCPServerSocket_Private)
PROXIED_API(PPB_TCPSocket_Private)
UNPROXIED_API(PPB_NetworkList_Private)
PROXIED_API(PPB_NetworkMonitor_Private)

PROXIED_IFACE(NoAPIName, PPB_HOSTRESOLVER_PRIVATE_INTERFACE_0_1,
              PPB_HostResolver_Private_0_1)
PROXIED_IFACE(PPB_TCPServerSocket_Private,
              PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE_0_1,
              PPB_TCPServerSocket_Private_0_1)
PROXIED_IFACE(PPB_TCPSocket_Private, PPB_TCPSOCKET_PRIVATE_INTERFACE_0_3,
              PPB_TCPSocket_Private_0_3)
PROXIED_IFACE(PPB_TCPSocket_Private, PPB_TCPSOCKET_PRIVATE_INTERFACE_0_4,
              PPB_TCPSocket_Private_0_4)
PROXIED_IFACE(PPB_TCPSocket_Private, PPB_TCPSOCKET_PRIVATE_INTERFACE_0_5,
              PPB_TCPSocket_Private_0_5)
PROXIED_IFACE(NoAPIName, PPB_UDPSOCKET_PRIVATE_INTERFACE_0_2,
              PPB_UDPSocket_Private_0_2)
PROXIED_IFACE(NoAPIName, PPB_UDPSOCKET_PRIVATE_INTERFACE_0_3,
              PPB_UDPSocket_Private_0_3)
PROXIED_IFACE(NoAPIName, PPB_UDPSOCKET_PRIVATE_INTERFACE_0_4,
              PPB_UDPSocket_Private_0_4)
PROXIED_IFACE(NoAPIName, PPB_UDPSOCKET_PRIVATE_INTERFACE_0_5,
              PPB_UDPSocket_Private_0_5)

PROXIED_IFACE(NoAPIName, PPB_NETADDRESS_PRIVATE_INTERFACE_0_1,
              PPB_NetAddress_Private_0_1)
PROXIED_IFACE(NoAPIName, PPB_NETADDRESS_PRIVATE_INTERFACE_1_0,
              PPB_NetAddress_Private_1_0)
PROXIED_IFACE(NoAPIName, PPB_NETADDRESS_PRIVATE_INTERFACE_1_1,
              PPB_NetAddress_Private_1_1)
PROXIED_IFACE(NoAPIName, PPB_NETWORKLIST_PRIVATE_INTERFACE_0_2,
              PPB_NetworkList_Private_0_2)
PROXIED_IFACE(PPB_NetworkMonitor_Private,
              PPB_NETWORKMONITOR_PRIVATE_INTERFACE_0_2,
              PPB_NetworkMonitor_Private_0_2)

#include "ppapi/thunk/interfaces_postamble.h"
