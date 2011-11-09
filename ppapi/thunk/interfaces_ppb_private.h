// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

PROXIED_API(PPB_Broker)

PROXIED_IFACE(PPB_Broker, PPB_BROKER_TRUSTED_INTERFACE_0_2, PPB_BrokerTrusted)
PROXIED_IFACE(PPB_Instance, PPB_FLASHFULLSCREEN_INTERFACE, PPB_FlashFullscreen)
// Map the old fullscreen interface string to the Flash one, which is the same
// at the ABI level. TODO(polina): remove this when Flash is updated.
PROXIED_IFACE(PPB_Instance, PPB_FULLSCREEN_DEV_INTERFACE_0_4,
              PPB_FlashFullscreen)

#include "ppapi/thunk/interfaces_postamble.h"
