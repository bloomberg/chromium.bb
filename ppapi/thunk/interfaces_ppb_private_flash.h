// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

PROXIED_API(PPB_Flash)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_INTERFACE_12_0,
              PPB_Flash_12_0)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_INTERFACE_12_1,
              PPB_Flash_12_1)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_INTERFACE_12_2,
              PPB_Flash_12_2)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_INTERFACE_12_3,
              PPB_Flash_12_3)

PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_CLIPBOARD_INTERFACE_3_LEGACY,
              PPB_Flash_Clipboard_3_0)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_CLIPBOARD_INTERFACE_3_0,
              PPB_Flash_Clipboard_3_0)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_CLIPBOARD_INTERFACE_4_0,
              PPB_Flash_Clipboard_4_0)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_FILE_MODULELOCAL_INTERFACE,
              PPB_Flash_File_ModuleLocal)
PROXIED_IFACE(PPB_Flash,
              PPB_FLASH_FILE_FILEREF_INTERFACE,
              PPB_Flash_File_FileRef)

PROXIED_API(PPB_Flash_Menu)
PROXIED_IFACE(PPB_Flash_Menu,
              PPB_FLASH_MENU_INTERFACE_0_2,
              PPB_Flash_Menu_0_2)

PROXIED_API(PPB_Flash_MessageLoop)
PROXIED_IFACE(PPB_Flash_MessageLoop,
              PPB_FLASH_MESSAGELOOP_INTERFACE_0_1,
              PPB_Flash_MessageLoop_0_1)

// TCPSocketPrivate is defined in the normal private interfaces.
PROXIED_IFACE(PPB_TCPSocket_Private,
              PPB_FLASH_TCPSOCKET_INTERFACE_0_2,
              PPB_TCPSocket_Private_0_3)

#include "ppapi/thunk/interfaces_postamble.h"
