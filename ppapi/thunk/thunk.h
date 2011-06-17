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
struct PPB_CursorControl_Dev;
struct PPB_DirectoryReader_Dev;
struct PPB_FileChooser_Dev;
struct PPB_FileIO_Dev;
struct PPB_FileIOTrusted_Dev;
struct PPB_FileRef_Dev;
struct PPB_FileSystem_Dev;
struct PPB_Find_Dev;
struct PPB_Flash_Menu;
struct PPB_Flash_NetConnector;
struct PPB_Font_Dev;
struct PPB_Fullscreen_Dev;
struct PPB_Graphics2D;
struct PPB_ImageData;
struct PPB_Instance;
struct PPB_Instance_Private;
struct PPB_ImageDataTrusted;
struct PPB_URLLoader;
struct PPB_URLLoaderTrusted;
struct PPB_URLRequestInfo;
struct PPB_URLResponseInfo;

#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
struct PPB_Instance_0_4;
typedef PPB_Instance PPB_Instance_0_5;
#else
struct PPB_Instance_0_5;
typedef PPB_Instance PPB_Instance_0_4;
#endif

namespace ppapi {
namespace thunk {

const PPB_Audio* GetPPB_Audio_Thunk();
const PPB_AudioConfig* GetPPB_AudioConfig_Thunk();
const PPB_AudioTrusted* GetPPB_AudioTrusted_Thunk();
const PPB_BrokerTrusted* GetPPB_Broker_Thunk();
const PPB_Buffer_Dev* GetPPB_Buffer_Thunk();
const PPB_BufferTrusted* GetPPB_BufferTrusted_Thunk();
const PPB_CharSet_Dev* GetPPB_CharSet_Thunk();
const PPB_CursorControl_Dev* GetPPB_CursorControl_Thunk();
const PPB_DirectoryReader_Dev* GetPPB_DirectoryReader_Thunk();
const PPB_FileChooser_Dev* GetPPB_FileChooser_Thunk();
const PPB_FileIO_Dev* GetPPB_FileIO_Thunk();
const PPB_FileIOTrusted_Dev* GetPPB_FileIOTrusted_Thunk();
const PPB_FileRef_Dev* GetPPB_FileRef_Thunk();
const PPB_FileSystem_Dev* GetPPB_FileSystem_Thunk();
const PPB_Find_Dev* GetPPB_Find_Thunk();
const PPB_Flash_Menu* GetPPB_Flash_Menu_Thunk();
const PPB_Flash_NetConnector* GetPPB_Flash_NetConnector_Thunk();
const PPB_Font_Dev* GetPPB_Font_Thunk();
const PPB_Fullscreen_Dev* GetPPB_Fullscreen_Thunk();
const PPB_Graphics2D* GetPPB_Graphics2D_Thunk();
const PPB_ImageData* GetPPB_ImageData_Thunk();
const PPB_Instance_0_4* GetPPB_Instance_0_4_Thunk();
const PPB_Instance_0_5* GetPPB_Instance_0_5_Thunk();
const PPB_Instance_Private* GetPPB_Instance_Private_Thunk();
const PPB_ImageDataTrusted* GetPPB_ImageDataTrusted_Thunk();
const PPB_URLLoader* GetPPB_URLLoader_Thunk();
const PPB_URLLoaderTrusted* GetPPB_URLLoaderTrusted_Thunk();
const PPB_URLRequestInfo* GetPPB_URLRequestInfo_Thunk();
const PPB_URLResponseInfo* GetPPB_URLResponseInfo_Thunk();

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_THUNK_H_
