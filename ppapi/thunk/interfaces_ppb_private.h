// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

PROXIED_API(PPB_Broker)
PROXIED_API(PPB_HostResolver_Private)
PROXIED_API(PPB_Talk_Private)
PROXIED_API(PPB_TCPServerSocket_Private)
PROXIED_API(PPB_TCPSocket_Private)
PROXIED_API(PPB_UDPSocket_Private)
PROXIED_API(PPB_X509Certificate_Private)
UNPROXIED_API(PPB_NetworkList_Private)
PROXIED_API(PPB_NetworkMonitor_Private)

PROXIED_IFACE(PPB_Broker, PPB_BROKER_TRUSTED_INTERFACE_0_2,
              PPB_BrokerTrusted_0_2)
PROXIED_IFACE(PPB_Instance, PPB_BROWSERFONT_TRUSTED_INTERFACE_1_0,
              PPB_BrowserFont_Trusted_1_0)
PROXIED_IFACE(PPB_Instance, PPB_CHARSET_TRUSTED_INTERFACE_1_0,
              PPB_CharSet_Trusted_1_0)
PROXIED_IFACE(PPB_FileChooser, PPB_FILECHOOSER_TRUSTED_INTERFACE_0_5,
              PPB_FileChooserTrusted_0_5)
PROXIED_IFACE(PPB_FileChooser, PPB_FILECHOOSER_TRUSTED_INTERFACE_0_6,
              PPB_FileChooserTrusted_0_6)
PROXIED_IFACE(PPB_FileRef, PPB_FILEREFPRIVATE_INTERFACE_0_1,
              PPB_FileRefPrivate_0_1)
// This uses the FileIO API which is declared in the public stable file.
PROXIED_IFACE(PPB_FileIO, PPB_FILEIOTRUSTED_INTERFACE_0_4,
              PPB_FileIOTrusted_0_4)
PROXIED_IFACE(PPB_HostResolver_Private, PPB_HOSTRESOLVER_PRIVATE_INTERFACE_0_1,
              PPB_HostResolver_Private_0_1)
PROXIED_IFACE(PPB_Instance, PPB_FLASHFULLSCREEN_INTERFACE_0_1,
              PPB_FlashFullscreen_0_1)
PROXIED_IFACE(NoAPIName, PPB_NETADDRESS_PRIVATE_INTERFACE_0_1,
              PPB_NetAddress_Private_0_1)
PROXIED_IFACE(NoAPIName, PPB_NETADDRESS_PRIVATE_INTERFACE_1_0,
              PPB_NetAddress_Private_1_0)
PROXIED_IFACE(NoAPIName, PPB_NETADDRESS_PRIVATE_INTERFACE_1_1,
              PPB_NetAddress_Private_1_1)
PROXIED_IFACE(PPB_Talk_Private, PPB_TALK_PRIVATE_INTERFACE_1_0,
              PPB_Talk_Private_1_0)
PROXIED_IFACE(PPB_TCPServerSocket_Private,
              PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE_0_1,
              PPB_TCPServerSocket_Private_0_1)
PROXIED_IFACE(PPB_TCPSocket_Private, PPB_TCPSOCKET_PRIVATE_INTERFACE_0_3,
              PPB_TCPSocket_Private_0_3)
PROXIED_IFACE(PPB_TCPSocket_Private, PPB_TCPSOCKET_PRIVATE_INTERFACE_0_4,
              PPB_TCPSocket_Private_0_4)
PROXIED_IFACE(PPB_UDPSocket_Private, PPB_UDPSOCKET_PRIVATE_INTERFACE_0_2,
              PPB_UDPSocket_Private_0_2)
PROXIED_IFACE(PPB_UDPSocket_Private, PPB_UDPSOCKET_PRIVATE_INTERFACE_0_3,
              PPB_UDPSocket_Private_0_3)
PROXIED_IFACE(PPB_X509Certificate_Private,
              PPB_X509CERTIFICATE_PRIVATE_INTERFACE_0_1,
              PPB_X509Certificate_Private_0_1)

PROXIED_IFACE(NoAPIName, PPB_NETWORKLIST_PRIVATE_INTERFACE_0_2,
              PPB_NetworkList_Private_0_2)
PROXIED_IFACE(PPB_NetworkMonitor_Private,
              PPB_NETWORKMONITOR_PRIVATE_INTERFACE_0_2,
              PPB_NetworkMonitor_Private_0_2)

// Hack to keep font working. The Font 0.6 API is binary compatible with
// BrowserFont 1.0, so just map the string to the same thing.
// TODO(brettw) remove support for the old Font API.
PROXIED_IFACE(PPB_Instance, PPB_FONT_DEV_INTERFACE_0_6,
              PPB_BrowserFont_Trusted_1_0)

#include "ppapi/thunk/interfaces_postamble.h"
