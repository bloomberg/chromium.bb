// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#ifndef GEN_PPAPI_PROXY_PPB_RPC_H_
#define GEN_PPAPI_PROXY_PPB_RPC_H_

#ifndef __native_client__
#include "native_client/src/include/portability.h"
#endif  // __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

class NaClFileRpcServer {
 public:
  static void StreamAsFile(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      char* url,
      int32_t callback_id);
  static void GetFileDesc(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      char* url,
      NaClSrpcImcDescType* file_desc);

 private:
  NaClFileRpcServer();
  NaClFileRpcServer(const NaClFileRpcServer&);
  void operator=(const NaClFileRpcServer);
};  // class NaClFileRpcServer

class PpbRpcServer {
 public:
  static void PPB_GetInterface(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      char* interface_name,
      int32_t* exports_interface_name);

 private:
  PpbRpcServer();
  PpbRpcServer(const PpbRpcServer&);
  void operator=(const PpbRpcServer);
};  // class PpbRpcServer

class PpbAudioRpcServer {
 public:
  static void PPB_Audio_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource config,
      PP_Resource* out_resource);
  static void PPB_Audio_IsAudio(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* out_bool);
  static void PPB_Audio_GetCurrentConfig(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      PP_Resource* out_resource);
  static void PPB_Audio_StopPlayback(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* out_bool);
  static void PPB_Audio_StartPlayback(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* out_bool);

 private:
  PpbAudioRpcServer();
  PpbAudioRpcServer(const PpbAudioRpcServer&);
  void operator=(const PpbAudioRpcServer);
};  // class PpbAudioRpcServer

class PpbAudioConfigRpcServer {
 public:
  static void PPB_AudioConfig_CreateStereo16Bit(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t sample_rate,
      int32_t sample_frame_count,
      PP_Resource* resource);
  static void PPB_AudioConfig_IsAudioConfig(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* out_bool);
  static void PPB_AudioConfig_RecommendSampleFrameCount(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t request_sample_rate,
      int32_t request_sample_frame_count,
      int32_t* out_sample_frame_count);
  static void PPB_AudioConfig_GetSampleRate(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* sample_rate);
  static void PPB_AudioConfig_GetSampleFrameCount(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* sample_frame_count);

 private:
  PpbAudioConfigRpcServer();
  PpbAudioConfigRpcServer(const PpbAudioConfigRpcServer&);
  void operator=(const PpbAudioConfigRpcServer);
};  // class PpbAudioConfigRpcServer

class PpbCoreRpcServer {
 public:
  static void PPB_Core_AddRefResource(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource);
  static void PPB_Core_ReleaseResource(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource);
  static void ReleaseResourceMultipleTimes(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t count);
  static void PPB_Core_GetTime(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      double* time);
  static void PPB_Core_GetTimeTicks(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      double* time_ticks);
  static void PPB_Core_CallOnMainThread(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t delay_in_milliseconds,
      int32_t callback_id,
      int32_t result);

 private:
  PpbCoreRpcServer();
  PpbCoreRpcServer(const PpbCoreRpcServer&);
  void operator=(const PpbCoreRpcServer);
};  // class PpbCoreRpcServer

class PpbCursorControlRpcServer {
 public:
  static void PPB_CursorControl_SetCursor(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t type,
      PP_Resource custom_image,
      nacl_abi_size_t hot_spot_bytes, char* hot_spot,
      int32_t* success);
  static void PPB_CursorControl_LockCursor(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t* success);
  static void PPB_CursorControl_UnlockCursor(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t* success);
  static void PPB_CursorControl_HasCursorLock(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t* success);
  static void PPB_CursorControl_CanLockCursor(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t* success);

 private:
  PpbCursorControlRpcServer();
  PpbCursorControlRpcServer(const PpbCursorControlRpcServer&);
  void operator=(const PpbCursorControlRpcServer);
};  // class PpbCursorControlRpcServer

class PpbFileIORpcServer {
 public:
  static void PPB_FileIO_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource* resource);
  static void PPB_FileIO_IsFileIO(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* success);
  static void PPB_FileIO_Open(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io,
      PP_Resource file_ref,
      int32_t open_flags,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_FileIO_Query(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io,
      int32_t bytes_to_read,
      int32_t callback_id,
      nacl_abi_size_t* info_bytes, char* info,
      int32_t* pp_error);
  static void PPB_FileIO_Touch(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io,
      double last_access_time,
      double last_modified_time,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_FileIO_Read(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io,
      int64_t offset,
      int32_t bytes_to_read,
      int32_t callback_id,
      nacl_abi_size_t* buffer_bytes, char* buffer,
      int32_t* pp_error_or_bytes);
  static void PPB_FileIO_Write(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io,
      int64_t offset,
      nacl_abi_size_t buffer_bytes, char* buffer,
      int32_t bytes_to_write,
      int32_t callback_id,
      int32_t* pp_error_or_bytes);
  static void PPB_FileIO_SetLength(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io,
      int64_t length,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_FileIO_Flush(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_FileIO_Close(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io);

 private:
  PpbFileIORpcServer();
  PpbFileIORpcServer(const PpbFileIORpcServer&);
  void operator=(const PpbFileIORpcServer);
};  // class PpbFileIORpcServer

class PpbFileRefRpcServer {
 public:
  static void PPB_FileRef_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_system,
      nacl_abi_size_t path_bytes, char* path,
      PP_Resource* resource);
  static void PPB_FileRef_IsFileRef(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* success);
  static void PPB_FileRef_GetFileSystemType(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_ref,
      int32_t* file_system_type);
  static void PPB_FileRef_GetName(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_ref,
      nacl_abi_size_t* name_bytes, char* name);
  static void PPB_FileRef_GetPath(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_ref,
      nacl_abi_size_t* path_bytes, char* path);
  static void PPB_FileRef_GetParent(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_ref,
      PP_Resource* parent);
  static void PPB_FileRef_MakeDirectory(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource directory_ref,
      int32_t make_ancestors,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_FileRef_Touch(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_ref,
      double last_access_time,
      double last_modified_time,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_FileRef_Delete(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_ref,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_FileRef_Rename(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_ref,
      PP_Resource new_file_ref,
      int32_t callback_id,
      int32_t* pp_error);

 private:
  PpbFileRefRpcServer();
  PpbFileRefRpcServer(const PpbFileRefRpcServer&);
  void operator=(const PpbFileRefRpcServer);
};  // class PpbFileRefRpcServer

class PpbFileSystemRpcServer {
 public:
  static void PPB_FileSystem_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t file_system_type,
      PP_Resource* resource);
  static void PPB_FileSystem_IsFileSystem(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* success);
  static void PPB_FileSystem_Open(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_system,
      int64_t expected_size,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_FileSystem_GetType(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_system,
      int32_t* type);

 private:
  PpbFileSystemRpcServer();
  PpbFileSystemRpcServer(const PpbFileSystemRpcServer&);
  void operator=(const PpbFileSystemRpcServer);
};  // class PpbFileSystemRpcServer

class PpbFindRpcServer {
 public:
  static void PPB_Find_NumberOfFindResultsChanged(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t total,
      int32_t final_result);
  static void PPB_Find_SelectedFindResultChanged(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t index);

 private:
  PpbFindRpcServer();
  PpbFindRpcServer(const PpbFindRpcServer&);
  void operator=(const PpbFindRpcServer);
};  // class PpbFindRpcServer

class PpbFontRpcServer {
 public:
  static void PPB_Font_GetFontFamilies(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t* font_families_bytes, char* font_families);
  static void PPB_Font_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t description_bytes, char* description,
      nacl_abi_size_t face_bytes, char* face,
      PP_Resource* font);
  static void PPB_Font_IsFont(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* is_font);
  static void PPB_Font_Describe(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource font,
      nacl_abi_size_t* description_bytes, char* description,
      nacl_abi_size_t* face_bytes, char* face,
      nacl_abi_size_t* metrics_bytes, char* metrics,
      int32_t* success);
  static void PPB_Font_DrawTextAt(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource font,
      PP_Resource image_data,
      nacl_abi_size_t text_run_bytes, char* text_run,
      nacl_abi_size_t text_bytes, char* text,
      nacl_abi_size_t position_bytes, char* position,
      int32_t color,
      nacl_abi_size_t clip_bytes, char* clip,
      int32_t image_data_is_opaque,
      int32_t* success);
  static void PPB_Font_MeasureText(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource font,
      nacl_abi_size_t text_run_bytes, char* text_run,
      nacl_abi_size_t text_bytes, char* text,
      int32_t* width);
  static void PPB_Font_CharacterOffsetForPixel(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource font,
      nacl_abi_size_t text_run_bytes, char* text_run,
      nacl_abi_size_t text_bytes, char* text,
      int32_t pixel_position,
      int32_t* offset);
  static void PPB_Font_PixelOffsetForCharacter(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource font,
      nacl_abi_size_t text_run_bytes, char* text_run,
      nacl_abi_size_t text_bytes, char* text,
      int32_t char_offset,
      int32_t* offset);

 private:
  PpbFontRpcServer();
  PpbFontRpcServer(const PpbFontRpcServer&);
  void operator=(const PpbFontRpcServer);
};  // class PpbFontRpcServer

class PpbFullscreenRpcServer {
 public:
  static void PPB_Fullscreen_SetFullscreen(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t fullscreen,
      int32_t* success);
  static void PPB_Fullscreen_GetScreenSize(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t* size_bytes, char* size,
      int32_t* success);

 private:
  PpbFullscreenRpcServer();
  PpbFullscreenRpcServer(const PpbFullscreenRpcServer&);
  void operator=(const PpbFullscreenRpcServer);
};  // class PpbFullscreenRpcServer

class PpbGraphics2DRpcServer {
 public:
  static void PPB_Graphics2D_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t size_bytes, char* size,
      int32_t is_always_opaque,
      PP_Resource* resource);
  static void PPB_Graphics2D_IsGraphics2D(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* success);
  static void PPB_Graphics2D_Describe(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource graphics_2d,
      nacl_abi_size_t* size_bytes, char* size,
      int32_t* is_always_opaque,
      int32_t* success);
  static void PPB_Graphics2D_PaintImageData(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource graphics_2d,
      PP_Resource image,
      nacl_abi_size_t top_left_bytes, char* top_left,
      nacl_abi_size_t src_rect_bytes, char* src_rect);
  static void PPB_Graphics2D_Scroll(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource graphics_2d,
      nacl_abi_size_t clip_rect_bytes, char* clip_rect,
      nacl_abi_size_t amount_bytes, char* amount);
  static void PPB_Graphics2D_ReplaceContents(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource graphics_2d,
      PP_Resource image);
  static void PPB_Graphics2D_Flush(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource graphics_2d,
      int32_t callback_id,
      int32_t* pp_error);

 private:
  PpbGraphics2DRpcServer();
  PpbGraphics2DRpcServer(const PpbGraphics2DRpcServer&);
  void operator=(const PpbGraphics2DRpcServer);
};  // class PpbGraphics2DRpcServer

class PpbGraphics3DRpcServer {
 public:
  static void PPB_Graphics3D_GetAttribMaxValue(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t attribute,
      int32_t* value,
      int32_t* pp_error);
  static void PPB_Graphics3D_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource share_context,
      nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
      PP_Resource* resource_id);
  static void PPB_Graphics3D_GetAttribs(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource context,
      nacl_abi_size_t input_attrib_list_bytes, int32_t* input_attrib_list,
      nacl_abi_size_t* output_attrib_list_bytes, int32_t* output_attrib_list,
      int32_t* pp_error);
  static void PPB_Graphics3D_SetAttribs(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource context,
      nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
      int32_t* pp_error);
  static void PPB_Graphics3D_GetError(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource context,
      int32_t* pp_error);
  static void PPB_Graphics3D_SwapBuffers(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource context,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_Graphics3DTrusted_CreateRaw(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource share_context,
      nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
      PP_Resource* resource_id);
  static void PPB_Graphics3DTrusted_InitCommandBuffer(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t size,
      int32_t* success);
  static void PPB_Graphics3DTrusted_GetRingBuffer(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      NaClSrpcImcDescType* shm_desc,
      int32_t* shm_size);
  static void PPB_Graphics3DTrusted_GetState(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      nacl_abi_size_t* state_bytes, char* state);
  static void PPB_Graphics3DTrusted_Flush(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t put_offset);
  static void PPB_Graphics3DTrusted_FlushSync(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t put_offset,
      nacl_abi_size_t* state_bytes, char* state);
  static void PPB_Graphics3DTrusted_FlushSyncFast(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t put_offset,
      int32_t last_known_offset,
      nacl_abi_size_t* state_bytes, char* state);
  static void PPB_Graphics3DTrusted_CreateTransferBuffer(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t size,
      int32_t request_id,
      int32_t* id);
  static void PPB_Graphics3DTrusted_DestroyTransferBuffer(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t id);
  static void PPB_Graphics3DTrusted_GetTransferBuffer(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t id,
      NaClSrpcImcDescType* shm_desc,
      int32_t* shm_size);

 private:
  PpbGraphics3DRpcServer();
  PpbGraphics3DRpcServer(const PpbGraphics3DRpcServer&);
  void operator=(const PpbGraphics3DRpcServer);
};  // class PpbGraphics3DRpcServer

class PpbImageDataRpcServer {
 public:
  static void PPB_ImageData_GetNativeImageDataFormat(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t* format);
  static void PPB_ImageData_IsImageDataFormatSupported(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t format,
      int32_t* success);
  static void PPB_ImageData_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t format,
      nacl_abi_size_t size_bytes, char* size,
      int32_t init_to_zero,
      PP_Resource* resource);
  static void PPB_ImageData_IsImageData(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* success);
  static void PPB_ImageData_Describe(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      nacl_abi_size_t* desc_bytes, char* desc,
      NaClSrpcImcDescType* shm,
      int32_t* shm_size,
      int32_t* success);

 private:
  PpbImageDataRpcServer();
  PpbImageDataRpcServer(const PpbImageDataRpcServer&);
  void operator=(const PpbImageDataRpcServer);
};  // class PpbImageDataRpcServer

class PpbInputEventRpcServer {
 public:
  static void PPB_InputEvent_RequestInputEvents(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t event_classes,
      int32_t filtered,
      int32_t* success);
  static void PPB_InputEvent_ClearInputEventRequest(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t event_classes);
  static void PPB_InputEvent_CreateMouseInputEvent(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t type,
      double time_stamp,
      int32_t modifiers,
      int32_t mouse_button,
      int32_t mouse_position_x,
      int32_t mouse_position_y,
      int32_t click_count,
      int32_t mouse_movement_x,
      int32_t mouse_movement_y,
      PP_Resource* resource_id);
  static void PPB_InputEvent_CreateWheelInputEvent(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      double time_stamp,
      int32_t modifiers,
      double wheel_delta_x,
      double wheel_delta_y,
      double wheel_ticks_x,
      double wheel_ticks_y,
      int32_t scroll_by_page,
      PP_Resource* resource_id);
  static void PPB_InputEvent_CreateKeyboardInputEvent(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t type,
      double time_stamp,
      int32_t modifiers,
      int32_t key_code,
      nacl_abi_size_t character_text_bytes, char* character_text,
      PP_Resource* resource_id);

 private:
  PpbInputEventRpcServer();
  PpbInputEventRpcServer(const PpbInputEventRpcServer&);
  void operator=(const PpbInputEventRpcServer);
};  // class PpbInputEventRpcServer

class PpbInstanceRpcServer {
 public:
  static void PPB_Instance_BindGraphics(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource graphics_device,
      int32_t* success);
  static void PPB_Instance_IsFullFrame(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t* is_full_frame);

 private:
  PpbInstanceRpcServer();
  PpbInstanceRpcServer(const PpbInstanceRpcServer&);
  void operator=(const PpbInstanceRpcServer);
};  // class PpbInstanceRpcServer

class PpbMessagingRpcServer {
 public:
  static void PPB_Messaging_PostMessage(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t message_bytes, char* message);

 private:
  PpbMessagingRpcServer();
  PpbMessagingRpcServer(const PpbMessagingRpcServer&);
  void operator=(const PpbMessagingRpcServer);
};  // class PpbMessagingRpcServer

class PpbMouseLockRpcServer {
 public:
  static void PPB_MouseLock_LockMouse(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_MouseLock_UnlockMouse(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);

 private:
  PpbMouseLockRpcServer();
  PpbMouseLockRpcServer(const PpbMouseLockRpcServer&);
  void operator=(const PpbMouseLockRpcServer);
};  // class PpbMouseLockRpcServer

class PpbPdfRpcServer {
 public:
  static void PPB_PDF_GetLocalizedString(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t string_id,
      nacl_abi_size_t* string_bytes, char* string);
  static void PPB_PDF_GetResourceImage(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t image_id,
      PP_Resource* image);
  static void PPB_PDF_GetFontFileWithFallback(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t description_bytes, char* description,
      nacl_abi_size_t face_bytes, char* face,
      int32_t charset,
      PP_Resource* font);
  static void PPB_PDF_GetFontTableForPrivateFontFile(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource font_file,
      int32_t table,
      nacl_abi_size_t* output_bytes, char* output,
      int32_t* success);
  static void PPB_PDF_SearchString(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t string_bytes, char* string,
      nacl_abi_size_t term_bytes, char* term,
      int32_t case_sensitive,
      nacl_abi_size_t* results_bytes, char* results,
      int32_t* count);
  static void PPB_PDF_DidStartLoading(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);
  static void PPB_PDF_DidStopLoading(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);
  static void PPB_PDF_SetContentRestriction(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t restrictions);
  static void PPB_PDF_HistogramPDFPageCount(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t count);
  static void PPB_PDF_UserMetricsRecordAction(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t action_bytes, char* action);
  static void PPB_PDF_HasUnsupportedFeature(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);
  static void PPB_PDF_SaveAs(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);

 private:
  PpbPdfRpcServer();
  PpbPdfRpcServer(const PpbPdfRpcServer&);
  void operator=(const PpbPdfRpcServer);
};  // class PpbPdfRpcServer

class PpbScrollbarRpcServer {
 public:
  static void PPB_Scrollbar_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t vertical,
      PP_Resource* scrollbar);
  static void PPB_Scrollbar_IsScrollbar(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* is_scrollbar);
  static void PPB_Scrollbar_IsOverlay(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* is_overlay);
  static void PPB_Scrollbar_GetThickness(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* thickness);
  static void PPB_Scrollbar_GetValue(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource scrollbar,
      int32_t* value);
  static void PPB_Scrollbar_SetValue(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource scrollbar,
      int32_t value);
  static void PPB_Scrollbar_SetDocumentSize(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource scrollbar,
      int32_t size);
  static void PPB_Scrollbar_SetTickMarks(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource scrollbar,
      nacl_abi_size_t tick_marks_bytes, char* tick_marks,
      int32_t count);
  static void PPB_Scrollbar_ScrollBy(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource scrollbar,
      int32_t unit,
      int32_t multiplier);

 private:
  PpbScrollbarRpcServer();
  PpbScrollbarRpcServer(const PpbScrollbarRpcServer&);
  void operator=(const PpbScrollbarRpcServer);
};  // class PpbScrollbarRpcServer

class PpbTCPSocketPrivateRpcServer {
 public:
  static void PPB_TCPSocket_Private_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource* resource);
  static void PPB_TCPSocket_Private_IsTCPSocket(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* is_tcp_socket);
  static void PPB_TCPSocket_Private_Connect(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource tcp_socket,
      char* host,
      int32_t port,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_TCPSocket_Private_ConnectWithNetAddress(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource tcp_socket,
      nacl_abi_size_t addr_bytes, char* addr,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_TCPSocket_Private_GetLocalAddress(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource tcp_socket,
      nacl_abi_size_t* local_addr_bytes, char* local_addr,
      int32_t* success);
  static void PPB_TCPSocket_Private_GetRemoteAddress(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource tcp_socket,
      nacl_abi_size_t* remote_addr_bytes, char* remote_addr,
      int32_t* success);
  static void PPB_TCPSocket_Private_SSLHandshake(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource tcp_socket,
      char* server_name,
      int32_t server_port,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_TCPSocket_Private_Read(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource tcp_socket,
      int32_t bytes_to_read,
      int32_t callback_id,
      nacl_abi_size_t* buffer_bytes, char* buffer,
      int32_t* pp_error_or_bytes);
  static void PPB_TCPSocket_Private_Write(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource tcp_socket,
      nacl_abi_size_t buffer_bytes, char* buffer,
      int32_t bytes_to_write,
      int32_t callback_id,
      int32_t* pp_error_or_bytes);
  static void PPB_TCPSocket_Private_Disconnect(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource tcp_socket);

 private:
  PpbTCPSocketPrivateRpcServer();
  PpbTCPSocketPrivateRpcServer(const PpbTCPSocketPrivateRpcServer&);
  void operator=(const PpbTCPSocketPrivateRpcServer);
};  // class PpbTCPSocketPrivateRpcServer

class PpbTestingRpcServer {
 public:
  static void PPB_Testing_ReadImageData(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource device_context_2d,
      PP_Resource image,
      nacl_abi_size_t top_left_bytes, char* top_left,
      int32_t* success);
  static void PPB_Testing_RunMessageLoop(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);
  static void PPB_Testing_QuitMessageLoop(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance);
  static void PPB_Testing_GetLiveObjectsForInstance(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t* live_object_count);

 private:
  PpbTestingRpcServer();
  PpbTestingRpcServer(const PpbTestingRpcServer&);
  void operator=(const PpbTestingRpcServer);
};  // class PpbTestingRpcServer

class PpbUDPSocketPrivateRpcServer {
 public:
  static void PPB_UDPSocket_Private_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance_id,
      PP_Resource* resource);
  static void PPB_UDPSocket_Private_IsUDPSocket(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t* is_udp_socket_private);
  static void PPB_UDPSocket_Private_Bind(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource udp_socket,
      nacl_abi_size_t addr_bytes, char* addr,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_UDPSocket_Private_RecvFrom(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource udp_socket,
      int32_t num_bytes,
      int32_t callback_id,
      nacl_abi_size_t* buffer_bytes, char* buffer,
      int32_t* pp_error_or_bytes);
  static void PPB_UDPSocket_Private_GetRecvFromAddress(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource udp_socket,
      nacl_abi_size_t* addr_bytes, char* addr,
      int32_t* success);
  static void PPB_UDPSocket_Private_SendTo(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource udp_socket,
      nacl_abi_size_t buffer_bytes, char* buffer,
      int32_t num_bytes,
      nacl_abi_size_t addr_bytes, char* addr,
      int32_t callback_id,
      int32_t* pp_error_or_bytes);
  static void PPB_UDPSocket_Private_Close(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource udp_socket);

 private:
  PpbUDPSocketPrivateRpcServer();
  PpbUDPSocketPrivateRpcServer(const PpbUDPSocketPrivateRpcServer&);
  void operator=(const PpbUDPSocketPrivateRpcServer);
};  // class PpbUDPSocketPrivateRpcServer

class PpbURLLoaderRpcServer {
 public:
  static void PPB_URLLoader_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource* resource);
  static void PPB_URLLoader_IsURLLoader(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* is_url_loader);
  static void PPB_URLLoader_Open(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource loader,
      PP_Resource request,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_URLLoader_FollowRedirect(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource loader,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_URLLoader_GetUploadProgress(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource loader,
      int64_t* bytes_sent,
      int64_t* total_bytes_to_be_sent,
      int32_t* success);
  static void PPB_URLLoader_GetDownloadProgress(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource loader,
      int64_t* bytes_received,
      int64_t* total_bytes_to_be_received,
      int32_t* success);
  static void PPB_URLLoader_GetResponseInfo(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource loader,
      PP_Resource* response);
  static void PPB_URLLoader_ReadResponseBody(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource loader,
      int32_t bytes_to_read,
      int32_t callback_id,
      nacl_abi_size_t* buffer_bytes, char* buffer,
      int32_t* pp_error_or_bytes);
  static void PPB_URLLoader_FinishStreamingToFile(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource loader,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_URLLoader_Close(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource loader);

 private:
  PpbURLLoaderRpcServer();
  PpbURLLoaderRpcServer(const PpbURLLoaderRpcServer&);
  void operator=(const PpbURLLoaderRpcServer);
};  // class PpbURLLoaderRpcServer

class PpbURLRequestInfoRpcServer {
 public:
  static void PPB_URLRequestInfo_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource* resource);
  static void PPB_URLRequestInfo_IsURLRequestInfo(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* success);
  static void PPB_URLRequestInfo_SetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource request,
      int32_t property,
      nacl_abi_size_t value_bytes, char* value,
      int32_t* success);
  static void PPB_URLRequestInfo_AppendDataToBody(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource request,
      nacl_abi_size_t data_bytes, char* data,
      int32_t* success);
  static void PPB_URLRequestInfo_AppendFileToBody(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource request,
      PP_Resource file_ref,
      int64_t start_offset,
      int64_t number_of_bytes,
      double expected_last_modified_time,
      int32_t* success);

 private:
  PpbURLRequestInfoRpcServer();
  PpbURLRequestInfoRpcServer(const PpbURLRequestInfoRpcServer&);
  void operator=(const PpbURLRequestInfoRpcServer);
};  // class PpbURLRequestInfoRpcServer

class PpbURLResponseInfoRpcServer {
 public:
  static void PPB_URLResponseInfo_IsURLResponseInfo(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* success);
  static void PPB_URLResponseInfo_GetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource response,
      int32_t property,
      nacl_abi_size_t* value_bytes, char* value);
  static void PPB_URLResponseInfo_GetBodyAsFileRef(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource response,
      PP_Resource* file_ref);

 private:
  PpbURLResponseInfoRpcServer();
  PpbURLResponseInfoRpcServer(const PpbURLResponseInfoRpcServer&);
  void operator=(const PpbURLResponseInfoRpcServer);
};  // class PpbURLResponseInfoRpcServer

class PpbWidgetRpcServer {
 public:
  static void PPB_Widget_IsWidget(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* is_widget);
  static void PPB_Widget_Paint(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource widget,
      nacl_abi_size_t rect_bytes, char* rect,
      PP_Resource image,
      int32_t* success);
  static void PPB_Widget_HandleEvent(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource widget,
      PP_Resource event,
      int32_t* handled);
  static void PPB_Widget_GetLocation(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource widget,
      nacl_abi_size_t* location_bytes, char* location,
      int32_t* visible);
  static void PPB_Widget_SetLocation(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource widget,
      nacl_abi_size_t location_bytes, char* location);

 private:
  PpbWidgetRpcServer();
  PpbWidgetRpcServer(const PpbWidgetRpcServer&);
  void operator=(const PpbWidgetRpcServer);
};  // class PpbWidgetRpcServer

class PpbZoomRpcServer {
 public:
  static void PPB_Zoom_ZoomChanged(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      double factor);
  static void PPB_Zoom_ZoomLimitsChanged(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      double minimum_factor,
      double maximum_factor);

 private:
  PpbZoomRpcServer();
  PpbZoomRpcServer(const PpbZoomRpcServer&);
  void operator=(const PpbZoomRpcServer);
};  // class PpbZoomRpcServer

class PpbRpcs {
 public:
  static NaClSrpcHandlerDesc srpc_methods[];
};  // class PpbRpcs


#endif  // GEN_PPAPI_PROXY_PPB_RPC_H_

