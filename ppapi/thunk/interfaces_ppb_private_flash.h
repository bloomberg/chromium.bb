// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

PROXIED_API(PPB_Flash)

PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_INTERFACE_11_0,
              PPB_Flash_11)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_INTERFACE_12_0,
              PPB_Flash_12_0)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_INTERFACE_12_1,
              PPB_Flash_12_1)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_INTERFACE_12_2,
              PPB_Flash_12_2)

#include "ppapi/thunk/interfaces_postamble.h"
