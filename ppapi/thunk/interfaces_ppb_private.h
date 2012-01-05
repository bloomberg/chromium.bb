// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

PROXIED_API(PPB_Broker)
PROXIED_API(PPB_TCPSocket_Private)
PROXIED_API(PPB_UDPSocket_Private)

PROXIED_IFACE(PPB_Broker, PPB_BROKER_TRUSTED_INTERFACE_0_2,
              PPB_BrokerTrusted_0_2)
PROXIED_IFACE(PPB_FileRef, PPB_FILEREFPRIVATE_INTERFACE_0_1,
              PPB_FileRefPrivate_0_1)
// This uses the FileIO API which is declared in the public stable file.
PROXIED_IFACE(PPB_FileIO, PPB_FILEIOTRUSTED_INTERFACE_0_4,
              PPB_FileIOTrusted_0_4)
PROXIED_IFACE(PPB_Instance, PPB_FLASHFULLSCREEN_INTERFACE_0_1,
              PPB_FlashFullscreen_0_1)
PROXIED_IFACE(NoAPIName, PPB_NETADDRESS_PRIVATE_INTERFACE_0_1,
              PPB_NetAddress_Private_0_1)
PROXIED_IFACE(PPB_TCPSocket_Private, PPB_TCPSOCKET_PRIVATE_INTERFACE_0_3,
              PPB_TCPSocket_Private_0_3)
PROXIED_IFACE(PPB_UDPSocket_Private, PPB_UDPSOCKET_PRIVATE_INTERFACE_0_2,
              PPB_UDPSocket_Private_0_2)

#include "ppapi/thunk/interfaces_postamble.h"
