// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

PROXIED_API(PPB_Broker)
PROXIED_API(PPB_TCPSocket_Private)
PROXIED_API(PPB_UDPSocket_Private)

PROXIED_IFACE(PPB_Broker, PPB_BROKER_TRUSTED_INTERFACE_0_2, PPB_BrokerTrusted)
PROXIED_IFACE(PPB_Instance, PPB_FLASHFULLSCREEN_INTERFACE, PPB_FlashFullscreen)
PROXIED_IFACE(NoAPIName, PPB_NETADDRESS_PRIVATE_INTERFACE,
              PPB_NetAddress_Private)
PROXIED_IFACE(PPB_TCPSocket_Private, PPB_TCPSOCKET_PRIVATE_INTERFACE,
              PPB_TCPSocket_Private)
PROXIED_IFACE(PPB_UDPSocket_Private, PPB_UDPSOCKET_PRIVATE_INTERFACE,
              PPB_UDPSocket_Private)

#include "ppapi/thunk/interfaces_postamble.h"
