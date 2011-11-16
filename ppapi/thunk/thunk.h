// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_THUNK_H_
#define PPAPI_THUNK_THUNK_H_

#include "ppapi/thunk/ppapi_thunk_export.h"

// Declares a getter for the interface thunk of the form:
//
//   const PPB_Foo* ppapi::thunk::GetPPB_Foo_Thunk();
//
#define IFACE(api_name, interface_name, InterfaceType) \
  struct InterfaceType; \
  namespace ppapi { namespace thunk { \
  PPAPI_THUNK_EXPORT const InterfaceType* Get##InterfaceType##_Thunk(); \
  } }
#define PROXIED_IFACE IFACE
#define UNPROXIED_IFACE IFACE

#include "ppapi/thunk/interfaces_ppb_private.h"
#include "ppapi/thunk/interfaces_ppb_public_stable.h"
#include "ppapi/thunk/interfaces_ppb_public_dev.h"

#undef UNPROXIED_IFACE
#undef PROXIED_IFACE
#undef IFACE

struct PPB_AudioTrusted;
struct PPB_BrokerTrusted;
struct PPB_BufferTrusted;
struct PPB_Context3DTrusted_Dev;
struct PPB_FileChooserTrusted;
struct PPB_FileIOTrusted;
struct PPB_Flash_Clipboard;
struct PPB_Flash_Menu;
struct PPB_Flash_NetConnector;
struct PPB_Graphics3D;
struct PPB_Graphics3DTrusted;
struct PPB_ImageDataTrusted;
struct PPB_Instance_Private;
struct PPB_TCPSocket_Private;
struct PPB_UDPSocket_Private;
struct PPB_URLLoaderTrusted;

typedef PPB_Instance PPB_Instance_1_0;

namespace ppapi {
namespace thunk {

// Old-style thunk getters. Only put trusted/private stuff here (it hasn't
// yet been converted to the new system). Otherwise, add the declaration to
// the appropriate interfaces_*.h file.
PPAPI_THUNK_EXPORT const PPB_AudioTrusted* GetPPB_AudioTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_BrokerTrusted* GetPPB_Broker_Thunk();
PPAPI_THUNK_EXPORT const PPB_BufferTrusted* GetPPB_BufferTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_Context3DTrusted_Dev*
    GetPPB_Context3DTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_FileChooserTrusted*
    GetPPB_FileChooser_Trusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_FileIOTrusted* GetPPB_FileIOTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_Flash_Clipboard* GetPPB_Flash_Clipboard_Thunk();
PPAPI_THUNK_EXPORT const PPB_Flash_Menu* GetPPB_Flash_Menu_Thunk();
PPAPI_THUNK_EXPORT const PPB_Flash_NetConnector*
    GetPPB_Flash_NetConnector_Thunk();
PPAPI_THUNK_EXPORT const PPB_Graphics3DTrusted*
    GetPPB_Graphics3DTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_ImageDataTrusted* GetPPB_ImageDataTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_Instance_Private* GetPPB_Instance_Private_Thunk();
PPAPI_THUNK_EXPORT const PPB_TCPSocket_Private*
    GetPPB_TCPSocket_Private_Thunk();
PPAPI_THUNK_EXPORT const PPB_UDPSocket_Private*
    GetPPB_UDPSocket_Private_Thunk();
PPAPI_THUNK_EXPORT const PPB_URLLoaderTrusted* GetPPB_URLLoaderTrusted_Thunk();

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_THUNK_H_
