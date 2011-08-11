// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_THUNK_H_
#define PPAPI_THUNK_THUNK_H_

struct PPB_Audio;
struct PPB_AudioConfig;
struct PPB_AudioTrusted;
struct PPB_BrokerTrusted;
struct PPB_Buffer_Dev;
struct PPB_BufferTrusted;
struct PPB_CharSet_Dev;
struct PPB_Context3D_Dev;
struct PPB_Context3DTrusted_Dev;
struct PPB_CursorControl_Dev;
struct PPB_DirectoryReader_Dev;
struct PPB_FileChooser_Dev;
struct PPB_FileIO;
struct PPB_FileIOTrusted;
struct PPB_FileRef;
struct PPB_FileSystem;
struct PPB_Find_Dev;
struct PPB_Flash_Menu;
struct PPB_Flash_NetConnector;
struct PPB_Flash_TCPSocket;
struct PPB_Font_Dev;
struct PPB_Fullscreen_Dev;
struct PPB_GLESChromiumTextureMapping_Dev;
struct PPB_Graphics2D;
struct PPB_Graphics3D_Dev;
struct PPB_Graphics3DTrusted_Dev;
struct PPB_ImageData;
struct PPB_ImageDataTrusted;
struct PPB_InputEvent;
struct PPB_Instance;
struct PPB_Instance_Private;
struct PPB_KeyboardInputEvent;
struct PPB_LayerCompositor_Dev;
struct PPB_Messaging;
struct PPB_MouseInputEvent;
struct PPB_QueryPolicy_Dev;
struct PPB_Scrollbar_0_5_Dev;
struct PPB_Surface3D_Dev;
struct PPB_Transport_Dev;
struct PPB_URLLoader;
struct PPB_URLLoaderTrusted;
struct PPB_URLRequestInfo;
struct PPB_URLResponseInfo;
struct PPB_VideoCapture_Dev;
struct PPB_VideoDecoder_Dev;
struct PPB_VideoLayer_Dev;
struct PPB_WheelInputEvent;
struct PPB_Widget_Dev;
struct PPB_Zoom_Dev;

typedef PPB_Instance PPB_Instance_1_0;

namespace ppapi {
namespace thunk {

const PPB_Audio* GetPPB_Audio_Thunk();
const PPB_AudioConfig* GetPPB_AudioConfig_Thunk();
const PPB_AudioTrusted* GetPPB_AudioTrusted_Thunk();
const PPB_BrokerTrusted* GetPPB_Broker_Thunk();
const PPB_Buffer_Dev* GetPPB_Buffer_Thunk();
const PPB_BufferTrusted* GetPPB_BufferTrusted_Thunk();
const PPB_CharSet_Dev* GetPPB_CharSet_Thunk();
const PPB_Context3D_Dev* GetPPB_Context3D_Thunk();
const PPB_Context3DTrusted_Dev* GetPPB_Context3DTrusted_Thunk();
const PPB_CursorControl_Dev* GetPPB_CursorControl_Thunk();
const PPB_DirectoryReader_Dev* GetPPB_DirectoryReader_Thunk();
const PPB_FileChooser_Dev* GetPPB_FileChooser_Thunk();
const PPB_FileIO* GetPPB_FileIO_Thunk();
const PPB_FileIOTrusted* GetPPB_FileIOTrusted_Thunk();
const PPB_FileRef* GetPPB_FileRef_Thunk();
const PPB_FileSystem* GetPPB_FileSystem_Thunk();
const PPB_Find_Dev* GetPPB_Find_Thunk();
const PPB_Flash_Menu* GetPPB_Flash_Menu_Thunk();
const PPB_Flash_NetConnector* GetPPB_Flash_NetConnector_Thunk();
const PPB_Flash_TCPSocket* GetPPB_Flash_TCPSocket_Thunk();
const PPB_Font_Dev* GetPPB_Font_Thunk();
const PPB_Fullscreen_Dev* GetPPB_Fullscreen_Thunk();
const PPB_GLESChromiumTextureMapping_Dev*
    GetPPB_GLESChromiumTextureMapping_Thunk();
const PPB_Graphics2D* GetPPB_Graphics2D_Thunk();
const PPB_Graphics3D_Dev* GetPPB_Graphics3D_Thunk();
const PPB_Graphics3DTrusted_Dev* GetPPB_Graphics3DTrusted_Thunk();
const PPB_InputEvent* GetPPB_InputEvent_Thunk();
const PPB_ImageData* GetPPB_ImageData_Thunk();
const PPB_ImageDataTrusted* GetPPB_ImageDataTrusted_Thunk();
const PPB_Instance_1_0* GetPPB_Instance_1_0_Thunk();
const PPB_Instance_Private* GetPPB_Instance_Private_Thunk();
const PPB_KeyboardInputEvent* GetPPB_KeyboardInputEvent_Thunk();
const PPB_LayerCompositor_Dev* GetPPB_LayerCompositor_Thunk();
const PPB_QueryPolicy_Dev* GetPPB_QueryPolicy_Thunk();
const PPB_Messaging* GetPPB_Messaging_Thunk();
const PPB_MouseInputEvent* GetPPB_MouseInputEvent_Thunk();
const PPB_Scrollbar_0_5_Dev* GetPPB_Scrollbar_Thunk();
const PPB_Surface3D_Dev* GetPPB_Surface3D_Thunk();
const PPB_Transport_Dev* GetPPB_Transport_Thunk();
const PPB_URLLoader* GetPPB_URLLoader_Thunk();
const PPB_URLLoaderTrusted* GetPPB_URLLoaderTrusted_Thunk();
const PPB_URLRequestInfo* GetPPB_URLRequestInfo_Thunk();
const PPB_URLResponseInfo* GetPPB_URLResponseInfo_Thunk();
const PPB_VideoCapture_Dev* GetPPB_VideoCapture_Thunk();
const PPB_VideoDecoder_Dev* GetPPB_VideoDecoder_Thunk();
const PPB_VideoLayer_Dev* GetPPB_VideoLayer_Thunk();
const PPB_WheelInputEvent* GetPPB_WheelInputEvent_Thunk();
const PPB_Widget_Dev* GetPPB_Widget_Thunk();
const PPB_Zoom_Dev* GetPPB_Zoom_Thunk();

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_THUNK_H_
