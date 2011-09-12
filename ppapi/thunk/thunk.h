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

#include "ppapi/thunk/interfaces_ppb_public_stable.h"
#include "ppapi/thunk/interfaces_ppb_public_dev.h"

#undef UNPROXIED_IFACE
#undef PROXIED_IFACE
#undef IFACE

struct PPB_AudioTrusted;
struct PPB_BrokerTrusted;
struct PPB_Buffer_Dev;
struct PPB_BufferTrusted;
struct PPB_CharSet_Dev;
struct PPB_Console_Dev;
struct PPB_Context3DTrusted_Dev;
struct PPB_CursorControl_Dev;
struct PPB_DirectoryReader_Dev;
struct PPB_FileChooser_Dev;
struct PPB_FileIOTrusted;
struct PPB_Find_Dev;
struct PPB_Flash_Menu;
struct PPB_Flash_NetConnector;
struct PPB_Flash_TCPSocket;
struct PPB_Font_Dev;
struct PPB_Fullscreen_Dev;
struct PPB_Graphics3D;
struct PPB_Graphics3DTrusted;
struct PPB_ImageData;
struct PPB_ImageDataTrusted;
struct PPB_Instance_Private;
struct PPB_LayerCompositor_Dev;
struct PPB_QueryPolicy_Dev;
struct PPB_Scrollbar_0_5_Dev;
struct PPB_Surface3D_Dev;
struct PPB_Transport_Dev;
struct PPB_URLLoaderTrusted;
struct PPB_VideoCapture_Dev;
struct PPB_VideoDecoder_Dev;
struct PPB_VideoLayer_Dev;
struct PPB_Widget_Dev;
struct PPB_Zoom_Dev;

typedef PPB_Instance PPB_Instance_1_0;

namespace ppapi {
namespace thunk {

PPAPI_THUNK_EXPORT const PPB_AudioTrusted* GetPPB_AudioTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_BrokerTrusted* GetPPB_Broker_Thunk();
PPAPI_THUNK_EXPORT const PPB_BufferTrusted* GetPPB_BufferTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_Console_Dev* GetPPB_Console_Dev_Thunk();
PPAPI_THUNK_EXPORT const PPB_Context3DTrusted_Dev*
    GetPPB_Context3DTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_CursorControl_Dev* GetPPB_CursorControl_Thunk();
PPAPI_THUNK_EXPORT const PPB_DirectoryReader_Dev*
    GetPPB_DirectoryReader_Dev_Thunk();
PPAPI_THUNK_EXPORT const PPB_FileIOTrusted* GetPPB_FileIOTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_Find_Dev* GetPPB_Find_Thunk();
PPAPI_THUNK_EXPORT const PPB_Flash_Menu* GetPPB_Flash_Menu_Thunk();
PPAPI_THUNK_EXPORT const PPB_Flash_NetConnector*
    GetPPB_Flash_NetConnector_Thunk();
PPAPI_THUNK_EXPORT const PPB_Flash_TCPSocket* GetPPB_Flash_TCPSocket_Thunk();
PPAPI_THUNK_EXPORT const PPB_Fullscreen_Dev* GetPPB_Fullscreen_Thunk();
PPAPI_THUNK_EXPORT const PPB_Graphics3DTrusted*
    GetPPB_Graphics3DTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_ImageDataTrusted* GetPPB_ImageDataTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_Instance_Private* GetPPB_Instance_Private_Thunk();
PPAPI_THUNK_EXPORT const PPB_LayerCompositor_Dev*
    GetPPB_LayerCompositor_Thunk();
PPAPI_THUNK_EXPORT const PPB_QueryPolicy_Dev* GetPPB_QueryPolicy_Thunk();
PPAPI_THUNK_EXPORT const PPB_Scrollbar_0_5_Dev* GetPPB_Scrollbar_Thunk();
PPAPI_THUNK_EXPORT const PPB_Transport_Dev* GetPPB_Transport_Dev_Thunk();
PPAPI_THUNK_EXPORT const PPB_URLLoaderTrusted* GetPPB_URLLoaderTrusted_Thunk();
PPAPI_THUNK_EXPORT const PPB_VideoLayer_Dev* GetPPB_VideoLayer_Dev_Thunk();
PPAPI_THUNK_EXPORT const PPB_Widget_Dev* GetPPB_Widget_Dev_Thunk();
PPAPI_THUNK_EXPORT const PPB_Zoom_Dev* GetPPB_Zoom_Thunk();

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_THUNK_H_
