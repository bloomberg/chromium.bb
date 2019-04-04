// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

#include "ppapi/thunk/interfaces_preamble.h"

// See interfaces_ppp_public_stable.h for documentation on these macros.
PROXIED_IFACE(PPB_TCPSOCKET_INTERFACE_1_0, PPB_TCPSocket_1_0)
PROXIED_IFACE(PPB_TCPSOCKET_INTERFACE_1_1, PPB_TCPSocket_1_1)
PROXIED_IFACE(PPB_TCPSOCKET_INTERFACE_1_2, PPB_TCPSocket_1_2)
PROXIED_IFACE(PPB_UDPSOCKET_INTERFACE_1_0, PPB_UDPSocket_1_0)
PROXIED_IFACE(PPB_UDPSOCKET_INTERFACE_1_1, PPB_UDPSocket_1_1)
PROXIED_IFACE(PPB_UDPSOCKET_INTERFACE_1_2, PPB_UDPSocket_1_2)

#include "ppapi/thunk/interfaces_postamble.h"
