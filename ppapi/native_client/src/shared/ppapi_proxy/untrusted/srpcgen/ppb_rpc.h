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

class NaClFileRpcClient {
 public:
  static NaClSrpcError StreamAsFile(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      char* url,
      int32_t callback_id);
  static NaClSrpcError GetFileDesc(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      char* url,
      NaClSrpcImcDescType* file_desc);

 private:
  NaClFileRpcClient();
  NaClFileRpcClient(const NaClFileRpcClient&);
  void operator=(const NaClFileRpcClient);
};  // class NaClFileRpcClient

class PpbRpcClient {
 public:
  static NaClSrpcError PPB_GetInterface(
      NaClSrpcChannel* channel,
      char* interface_name,
      int32_t* exports_interface_name);

 private:
  PpbRpcClient();
  PpbRpcClient(const PpbRpcClient&);
  void operator=(const PpbRpcClient);
};  // class PpbRpcClient

class PpbAudioRpcClient {
 public:
  static NaClSrpcError PPB_Audio_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource config,
      PP_Resource* out_resource);
  static NaClSrpcError PPB_Audio_IsAudio(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* out_bool);
  static NaClSrpcError PPB_Audio_GetCurrentConfig(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      PP_Resource* out_resource);
  static NaClSrpcError PPB_Audio_StopPlayback(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* out_bool);
  static NaClSrpcError PPB_Audio_StartPlayback(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* out_bool);

 private:
  PpbAudioRpcClient();
  PpbAudioRpcClient(const PpbAudioRpcClient&);
  void operator=(const PpbAudioRpcClient);
};  // class PpbAudioRpcClient

class PpbAudioConfigRpcClient {
 public:
  static NaClSrpcError PPB_AudioConfig_CreateStereo16Bit(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t sample_rate,
      int32_t sample_frame_count,
      PP_Resource* resource);
  static NaClSrpcError PPB_AudioConfig_IsAudioConfig(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* out_bool);
  static NaClSrpcError PPB_AudioConfig_RecommendSampleFrameCount(
      NaClSrpcChannel* channel,
      int32_t request_sample_rate,
      int32_t request_sample_frame_count,
      int32_t* out_sample_frame_count);
  static NaClSrpcError PPB_AudioConfig_GetSampleRate(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* sample_rate);
  static NaClSrpcError PPB_AudioConfig_GetSampleFrameCount(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* sample_frame_count);

 private:
  PpbAudioConfigRpcClient();
  PpbAudioConfigRpcClient(const PpbAudioConfigRpcClient&);
  void operator=(const PpbAudioConfigRpcClient);
};  // class PpbAudioConfigRpcClient

class PpbCoreRpcClient {
 public:
  static NaClSrpcError PPB_Core_AddRefResource(
      NaClSrpcChannel* channel,
      PP_Resource resource);
  static NaClSrpcError PPB_Core_ReleaseResource(
      NaClSrpcChannel* channel,
      PP_Resource resource);
  static NaClSrpcError ReleaseResourceMultipleTimes(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t count);
  static NaClSrpcError PPB_Core_GetTime(
      NaClSrpcChannel* channel,
      double* time);
  static NaClSrpcError PPB_Core_GetTimeTicks(
      NaClSrpcChannel* channel,
      double* time_ticks);
  static NaClSrpcError PPB_Core_CallOnMainThread(
      NaClSrpcChannel* channel,
      int32_t delay_in_milliseconds,
      int32_t callback_id,
      int32_t result);

 private:
  PpbCoreRpcClient();
  PpbCoreRpcClient(const PpbCoreRpcClient&);
  void operator=(const PpbCoreRpcClient);
};  // class PpbCoreRpcClient

class PpbCursorControlRpcClient {
 public:
  static NaClSrpcError PPB_CursorControl_SetCursor(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t type,
      PP_Resource custom_image,
      nacl_abi_size_t hot_spot_bytes, char* hot_spot,
      int32_t* success);
  static NaClSrpcError PPB_CursorControl_LockCursor(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t* success);
  static NaClSrpcError PPB_CursorControl_UnlockCursor(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t* success);
  static NaClSrpcError PPB_CursorControl_HasCursorLock(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t* success);
  static NaClSrpcError PPB_CursorControl_CanLockCursor(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t* success);

 private:
  PpbCursorControlRpcClient();
  PpbCursorControlRpcClient(const PpbCursorControlRpcClient&);
  void operator=(const PpbCursorControlRpcClient);
};  // class PpbCursorControlRpcClient

class PpbFileIORpcClient {
 public:
  static NaClSrpcError PPB_FileIO_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource* resource);
  static NaClSrpcError PPB_FileIO_IsFileIO(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* success);
  static NaClSrpcError PPB_FileIO_Open(
      NaClSrpcChannel* channel,
      PP_Resource file_io,
      PP_Resource file_ref,
      int32_t open_flags,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_FileIO_Query(
      NaClSrpcChannel* channel,
      PP_Resource file_io,
      int32_t bytes_to_read,
      int32_t callback_id,
      nacl_abi_size_t* info_bytes, char* info,
      int32_t* pp_error);
  static NaClSrpcError PPB_FileIO_Touch(
      NaClSrpcChannel* channel,
      PP_Resource file_io,
      double last_access_time,
      double last_modified_time,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_FileIO_Read(
      NaClSrpcChannel* channel,
      PP_Resource file_io,
      int64_t offset,
      int32_t bytes_to_read,
      int32_t callback_id,
      nacl_abi_size_t* buffer_bytes, char* buffer,
      int32_t* pp_error_or_bytes);
  static NaClSrpcError PPB_FileIO_Write(
      NaClSrpcChannel* channel,
      PP_Resource file_io,
      int64_t offset,
      nacl_abi_size_t buffer_bytes, char* buffer,
      int32_t bytes_to_write,
      int32_t callback_id,
      int32_t* pp_error_or_bytes);
  static NaClSrpcError PPB_FileIO_SetLength(
      NaClSrpcChannel* channel,
      PP_Resource file_io,
      int64_t length,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_FileIO_Flush(
      NaClSrpcChannel* channel,
      PP_Resource file_io,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_FileIO_Close(
      NaClSrpcChannel* channel,
      PP_Resource file_io);

 private:
  PpbFileIORpcClient();
  PpbFileIORpcClient(const PpbFileIORpcClient&);
  void operator=(const PpbFileIORpcClient);
};  // class PpbFileIORpcClient

class PpbFileRefRpcClient {
 public:
  static NaClSrpcError PPB_FileRef_Create(
      NaClSrpcChannel* channel,
      PP_Resource file_system,
      nacl_abi_size_t path_bytes, char* path,
      PP_Resource* resource);
  static NaClSrpcError PPB_FileRef_IsFileRef(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* success);
  static NaClSrpcError PPB_FileRef_GetFileSystemType(
      NaClSrpcChannel* channel,
      PP_Resource file_ref,
      int32_t* file_system_type);
  static NaClSrpcError PPB_FileRef_GetName(
      NaClSrpcChannel* channel,
      PP_Resource file_ref,
      nacl_abi_size_t* name_bytes, char* name);
  static NaClSrpcError PPB_FileRef_GetPath(
      NaClSrpcChannel* channel,
      PP_Resource file_ref,
      nacl_abi_size_t* path_bytes, char* path);
  static NaClSrpcError PPB_FileRef_GetParent(
      NaClSrpcChannel* channel,
      PP_Resource file_ref,
      PP_Resource* parent);
  static NaClSrpcError PPB_FileRef_MakeDirectory(
      NaClSrpcChannel* channel,
      PP_Resource directory_ref,
      int32_t make_ancestors,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_FileRef_Touch(
      NaClSrpcChannel* channel,
      PP_Resource file_ref,
      double last_access_time,
      double last_modified_time,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_FileRef_Delete(
      NaClSrpcChannel* channel,
      PP_Resource file_ref,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_FileRef_Rename(
      NaClSrpcChannel* channel,
      PP_Resource file_ref,
      PP_Resource new_file_ref,
      int32_t callback_id,
      int32_t* pp_error);

 private:
  PpbFileRefRpcClient();
  PpbFileRefRpcClient(const PpbFileRefRpcClient&);
  void operator=(const PpbFileRefRpcClient);
};  // class PpbFileRefRpcClient

class PpbFileSystemRpcClient {
 public:
  static NaClSrpcError PPB_FileSystem_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t file_system_type,
      PP_Resource* resource);
  static NaClSrpcError PPB_FileSystem_IsFileSystem(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* success);
  static NaClSrpcError PPB_FileSystem_Open(
      NaClSrpcChannel* channel,
      PP_Resource file_system,
      int64_t expected_size,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_FileSystem_GetType(
      NaClSrpcChannel* channel,
      PP_Resource file_system,
      int32_t* type);

 private:
  PpbFileSystemRpcClient();
  PpbFileSystemRpcClient(const PpbFileSystemRpcClient&);
  void operator=(const PpbFileSystemRpcClient);
};  // class PpbFileSystemRpcClient

class PpbFindRpcClient {
 public:
  static NaClSrpcError PPB_Find_NumberOfFindResultsChanged(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t total,
      int32_t final_result);
  static NaClSrpcError PPB_Find_SelectedFindResultChanged(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t index);

 private:
  PpbFindRpcClient();
  PpbFindRpcClient(const PpbFindRpcClient&);
  void operator=(const PpbFindRpcClient);
};  // class PpbFindRpcClient

class PpbFontRpcClient {
 public:
  static NaClSrpcError PPB_Font_GetFontFamilies(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t* font_families_bytes, char* font_families);
  static NaClSrpcError PPB_Font_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t description_bytes, char* description,
      nacl_abi_size_t face_bytes, char* face,
      PP_Resource* font);
  static NaClSrpcError PPB_Font_IsFont(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* is_font);
  static NaClSrpcError PPB_Font_Describe(
      NaClSrpcChannel* channel,
      PP_Resource font,
      nacl_abi_size_t* description_bytes, char* description,
      nacl_abi_size_t* face_bytes, char* face,
      nacl_abi_size_t* metrics_bytes, char* metrics,
      int32_t* success);
  static NaClSrpcError PPB_Font_DrawTextAt(
      NaClSrpcChannel* channel,
      PP_Resource font,
      PP_Resource image_data,
      nacl_abi_size_t text_run_bytes, char* text_run,
      nacl_abi_size_t text_bytes, char* text,
      nacl_abi_size_t position_bytes, char* position,
      int32_t color,
      nacl_abi_size_t clip_bytes, char* clip,
      int32_t image_data_is_opaque,
      int32_t* success);
  static NaClSrpcError PPB_Font_MeasureText(
      NaClSrpcChannel* channel,
      PP_Resource font,
      nacl_abi_size_t text_run_bytes, char* text_run,
      nacl_abi_size_t text_bytes, char* text,
      int32_t* width);
  static NaClSrpcError PPB_Font_CharacterOffsetForPixel(
      NaClSrpcChannel* channel,
      PP_Resource font,
      nacl_abi_size_t text_run_bytes, char* text_run,
      nacl_abi_size_t text_bytes, char* text,
      int32_t pixel_position,
      int32_t* offset);
  static NaClSrpcError PPB_Font_PixelOffsetForCharacter(
      NaClSrpcChannel* channel,
      PP_Resource font,
      nacl_abi_size_t text_run_bytes, char* text_run,
      nacl_abi_size_t text_bytes, char* text,
      int32_t char_offset,
      int32_t* offset);

 private:
  PpbFontRpcClient();
  PpbFontRpcClient(const PpbFontRpcClient&);
  void operator=(const PpbFontRpcClient);
};  // class PpbFontRpcClient

class PpbFullscreenRpcClient {
 public:
  static NaClSrpcError PPB_Fullscreen_SetFullscreen(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t fullscreen,
      int32_t* success);
  static NaClSrpcError PPB_Fullscreen_GetScreenSize(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t* size_bytes, char* size,
      int32_t* success);

 private:
  PpbFullscreenRpcClient();
  PpbFullscreenRpcClient(const PpbFullscreenRpcClient&);
  void operator=(const PpbFullscreenRpcClient);
};  // class PpbFullscreenRpcClient

class PpbGraphics2DRpcClient {
 public:
  static NaClSrpcError PPB_Graphics2D_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t size_bytes, char* size,
      int32_t is_always_opaque,
      PP_Resource* resource);
  static NaClSrpcError PPB_Graphics2D_IsGraphics2D(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* success);
  static NaClSrpcError PPB_Graphics2D_Describe(
      NaClSrpcChannel* channel,
      PP_Resource graphics_2d,
      nacl_abi_size_t* size_bytes, char* size,
      int32_t* is_always_opaque,
      int32_t* success);
  static NaClSrpcError PPB_Graphics2D_PaintImageData(
      NaClSrpcChannel* channel,
      PP_Resource graphics_2d,
      PP_Resource image,
      nacl_abi_size_t top_left_bytes, char* top_left,
      nacl_abi_size_t src_rect_bytes, char* src_rect);
  static NaClSrpcError PPB_Graphics2D_Scroll(
      NaClSrpcChannel* channel,
      PP_Resource graphics_2d,
      nacl_abi_size_t clip_rect_bytes, char* clip_rect,
      nacl_abi_size_t amount_bytes, char* amount);
  static NaClSrpcError PPB_Graphics2D_ReplaceContents(
      NaClSrpcChannel* channel,
      PP_Resource graphics_2d,
      PP_Resource image);
  static NaClSrpcError PPB_Graphics2D_Flush(
      NaClSrpcChannel* channel,
      PP_Resource graphics_2d,
      int32_t callback_id,
      int32_t* pp_error);

 private:
  PpbGraphics2DRpcClient();
  PpbGraphics2DRpcClient(const PpbGraphics2DRpcClient&);
  void operator=(const PpbGraphics2DRpcClient);
};  // class PpbGraphics2DRpcClient

class PpbGraphics3DRpcClient {
 public:
  static NaClSrpcError PPB_Graphics3D_GetAttribMaxValue(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t attribute,
      int32_t* value,
      int32_t* pp_error);
  static NaClSrpcError PPB_Graphics3D_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource share_context,
      nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
      PP_Resource* resource_id);
  static NaClSrpcError PPB_Graphics3D_GetAttribs(
      NaClSrpcChannel* channel,
      PP_Resource context,
      nacl_abi_size_t input_attrib_list_bytes, int32_t* input_attrib_list,
      nacl_abi_size_t* output_attrib_list_bytes, int32_t* output_attrib_list,
      int32_t* pp_error);
  static NaClSrpcError PPB_Graphics3D_SetAttribs(
      NaClSrpcChannel* channel,
      PP_Resource context,
      nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
      int32_t* pp_error);
  static NaClSrpcError PPB_Graphics3D_GetError(
      NaClSrpcChannel* channel,
      PP_Resource context,
      int32_t* pp_error);
  static NaClSrpcError PPB_Graphics3D_SwapBuffers(
      NaClSrpcChannel* channel,
      PP_Resource context,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_Graphics3DTrusted_CreateRaw(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource share_context,
      nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
      PP_Resource* resource_id);
  static NaClSrpcError PPB_Graphics3DTrusted_InitCommandBuffer(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t size,
      int32_t* success);
  static NaClSrpcError PPB_Graphics3DTrusted_GetRingBuffer(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      NaClSrpcImcDescType* shm_desc,
      int32_t* shm_size);
  static NaClSrpcError PPB_Graphics3DTrusted_GetState(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      nacl_abi_size_t* state_bytes, char* state);
  static NaClSrpcError PPB_Graphics3DTrusted_Flush(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t put_offset);
  static NaClSrpcError PPB_Graphics3DTrusted_FlushSync(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t put_offset,
      nacl_abi_size_t* state_bytes, char* state);
  static NaClSrpcError PPB_Graphics3DTrusted_FlushSyncFast(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t put_offset,
      int32_t last_known_offset,
      nacl_abi_size_t* state_bytes, char* state);
  static NaClSrpcError PPB_Graphics3DTrusted_CreateTransferBuffer(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t size,
      int32_t request_id,
      int32_t* id);
  static NaClSrpcError PPB_Graphics3DTrusted_DestroyTransferBuffer(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t id);
  static NaClSrpcError PPB_Graphics3DTrusted_GetTransferBuffer(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t id,
      NaClSrpcImcDescType* shm_desc,
      int32_t* shm_size);

 private:
  PpbGraphics3DRpcClient();
  PpbGraphics3DRpcClient(const PpbGraphics3DRpcClient&);
  void operator=(const PpbGraphics3DRpcClient);
};  // class PpbGraphics3DRpcClient

class PpbImageDataRpcClient {
 public:
  static NaClSrpcError PPB_ImageData_GetNativeImageDataFormat(
      NaClSrpcChannel* channel,
      int32_t* format);
  static NaClSrpcError PPB_ImageData_IsImageDataFormatSupported(
      NaClSrpcChannel* channel,
      int32_t format,
      int32_t* success);
  static NaClSrpcError PPB_ImageData_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t format,
      nacl_abi_size_t size_bytes, char* size,
      int32_t init_to_zero,
      PP_Resource* resource);
  static NaClSrpcError PPB_ImageData_IsImageData(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* success);
  static NaClSrpcError PPB_ImageData_Describe(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      nacl_abi_size_t* desc_bytes, char* desc,
      NaClSrpcImcDescType* shm,
      int32_t* shm_size,
      int32_t* success);

 private:
  PpbImageDataRpcClient();
  PpbImageDataRpcClient(const PpbImageDataRpcClient&);
  void operator=(const PpbImageDataRpcClient);
};  // class PpbImageDataRpcClient

class PpbInputEventRpcClient {
 public:
  static NaClSrpcError PPB_InputEvent_RequestInputEvents(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t event_classes,
      int32_t filtered,
      int32_t* success);
  static NaClSrpcError PPB_InputEvent_ClearInputEventRequest(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t event_classes);
  static NaClSrpcError PPB_InputEvent_CreateMouseInputEvent(
      NaClSrpcChannel* channel,
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
  static NaClSrpcError PPB_InputEvent_CreateWheelInputEvent(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      double time_stamp,
      int32_t modifiers,
      double wheel_delta_x,
      double wheel_delta_y,
      double wheel_ticks_x,
      double wheel_ticks_y,
      int32_t scroll_by_page,
      PP_Resource* resource_id);
  static NaClSrpcError PPB_InputEvent_CreateKeyboardInputEvent(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t type,
      double time_stamp,
      int32_t modifiers,
      int32_t key_code,
      nacl_abi_size_t character_text_bytes, char* character_text,
      PP_Resource* resource_id);

 private:
  PpbInputEventRpcClient();
  PpbInputEventRpcClient(const PpbInputEventRpcClient&);
  void operator=(const PpbInputEventRpcClient);
};  // class PpbInputEventRpcClient

class PpbInstanceRpcClient {
 public:
  static NaClSrpcError PPB_Instance_BindGraphics(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource graphics_device,
      int32_t* success);
  static NaClSrpcError PPB_Instance_IsFullFrame(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t* is_full_frame);

 private:
  PpbInstanceRpcClient();
  PpbInstanceRpcClient(const PpbInstanceRpcClient&);
  void operator=(const PpbInstanceRpcClient);
};  // class PpbInstanceRpcClient

class PpbMessagingRpcClient {
 public:
  static NaClSrpcError PPB_Messaging_PostMessage(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t message_bytes, char* message);

 private:
  PpbMessagingRpcClient();
  PpbMessagingRpcClient(const PpbMessagingRpcClient&);
  void operator=(const PpbMessagingRpcClient);
};  // class PpbMessagingRpcClient

class PpbMouseLockRpcClient {
 public:
  static NaClSrpcError PPB_MouseLock_LockMouse(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_MouseLock_UnlockMouse(
      NaClSrpcChannel* channel,
      PP_Instance instance);

 private:
  PpbMouseLockRpcClient();
  PpbMouseLockRpcClient(const PpbMouseLockRpcClient&);
  void operator=(const PpbMouseLockRpcClient);
};  // class PpbMouseLockRpcClient

class PpbPdfRpcClient {
 public:
  static NaClSrpcError PPB_PDF_GetLocalizedString(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t string_id,
      nacl_abi_size_t* string_bytes, char* string);
  static NaClSrpcError PPB_PDF_GetResourceImage(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t image_id,
      PP_Resource* image);
  static NaClSrpcError PPB_PDF_GetFontFileWithFallback(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t description_bytes, char* description,
      nacl_abi_size_t face_bytes, char* face,
      int32_t charset,
      PP_Resource* font);
  static NaClSrpcError PPB_PDF_GetFontTableForPrivateFontFile(
      NaClSrpcChannel* channel,
      PP_Resource font_file,
      int32_t table,
      nacl_abi_size_t* output_bytes, char* output,
      int32_t* success);
  static NaClSrpcError PPB_PDF_SearchString(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t string_bytes, char* string,
      nacl_abi_size_t term_bytes, char* term,
      int32_t case_sensitive,
      nacl_abi_size_t* results_bytes, char* results,
      int32_t* count);
  static NaClSrpcError PPB_PDF_DidStartLoading(
      NaClSrpcChannel* channel,
      PP_Instance instance);
  static NaClSrpcError PPB_PDF_DidStopLoading(
      NaClSrpcChannel* channel,
      PP_Instance instance);
  static NaClSrpcError PPB_PDF_SetContentRestriction(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t restrictions);
  static NaClSrpcError PPB_PDF_HistogramPDFPageCount(
      NaClSrpcChannel* channel,
      int32_t count);
  static NaClSrpcError PPB_PDF_UserMetricsRecordAction(
      NaClSrpcChannel* channel,
      nacl_abi_size_t action_bytes, char* action);
  static NaClSrpcError PPB_PDF_HasUnsupportedFeature(
      NaClSrpcChannel* channel,
      PP_Instance instance);
  static NaClSrpcError PPB_PDF_SaveAs(
      NaClSrpcChannel* channel,
      PP_Instance instance);

 private:
  PpbPdfRpcClient();
  PpbPdfRpcClient(const PpbPdfRpcClient&);
  void operator=(const PpbPdfRpcClient);
};  // class PpbPdfRpcClient

class PpbScrollbarRpcClient {
 public:
  static NaClSrpcError PPB_Scrollbar_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t vertical,
      PP_Resource* scrollbar);
  static NaClSrpcError PPB_Scrollbar_IsScrollbar(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* is_scrollbar);
  static NaClSrpcError PPB_Scrollbar_IsOverlay(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* is_overlay);
  static NaClSrpcError PPB_Scrollbar_GetThickness(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* thickness);
  static NaClSrpcError PPB_Scrollbar_GetValue(
      NaClSrpcChannel* channel,
      PP_Resource scrollbar,
      int32_t* value);
  static NaClSrpcError PPB_Scrollbar_SetValue(
      NaClSrpcChannel* channel,
      PP_Resource scrollbar,
      int32_t value);
  static NaClSrpcError PPB_Scrollbar_SetDocumentSize(
      NaClSrpcChannel* channel,
      PP_Resource scrollbar,
      int32_t size);
  static NaClSrpcError PPB_Scrollbar_SetTickMarks(
      NaClSrpcChannel* channel,
      PP_Resource scrollbar,
      nacl_abi_size_t tick_marks_bytes, char* tick_marks,
      int32_t count);
  static NaClSrpcError PPB_Scrollbar_ScrollBy(
      NaClSrpcChannel* channel,
      PP_Resource scrollbar,
      int32_t unit,
      int32_t multiplier);

 private:
  PpbScrollbarRpcClient();
  PpbScrollbarRpcClient(const PpbScrollbarRpcClient&);
  void operator=(const PpbScrollbarRpcClient);
};  // class PpbScrollbarRpcClient

class PpbTCPSocketPrivateRpcClient {
 public:
  static NaClSrpcError PPB_TCPSocket_Private_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource* resource);
  static NaClSrpcError PPB_TCPSocket_Private_IsTCPSocket(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* is_tcp_socket);
  static NaClSrpcError PPB_TCPSocket_Private_Connect(
      NaClSrpcChannel* channel,
      PP_Resource tcp_socket,
      char* host,
      int32_t port,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_TCPSocket_Private_ConnectWithNetAddress(
      NaClSrpcChannel* channel,
      PP_Resource tcp_socket,
      nacl_abi_size_t addr_bytes, char* addr,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_TCPSocket_Private_GetLocalAddress(
      NaClSrpcChannel* channel,
      PP_Resource tcp_socket,
      nacl_abi_size_t* local_addr_bytes, char* local_addr,
      int32_t* success);
  static NaClSrpcError PPB_TCPSocket_Private_GetRemoteAddress(
      NaClSrpcChannel* channel,
      PP_Resource tcp_socket,
      nacl_abi_size_t* remote_addr_bytes, char* remote_addr,
      int32_t* success);
  static NaClSrpcError PPB_TCPSocket_Private_SSLHandshake(
      NaClSrpcChannel* channel,
      PP_Resource tcp_socket,
      char* server_name,
      int32_t server_port,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_TCPSocket_Private_Read(
      NaClSrpcChannel* channel,
      PP_Resource tcp_socket,
      int32_t bytes_to_read,
      int32_t callback_id,
      nacl_abi_size_t* buffer_bytes, char* buffer,
      int32_t* pp_error_or_bytes);
  static NaClSrpcError PPB_TCPSocket_Private_Write(
      NaClSrpcChannel* channel,
      PP_Resource tcp_socket,
      nacl_abi_size_t buffer_bytes, char* buffer,
      int32_t bytes_to_write,
      int32_t callback_id,
      int32_t* pp_error_or_bytes);
  static NaClSrpcError PPB_TCPSocket_Private_Disconnect(
      NaClSrpcChannel* channel,
      PP_Resource tcp_socket);

 private:
  PpbTCPSocketPrivateRpcClient();
  PpbTCPSocketPrivateRpcClient(const PpbTCPSocketPrivateRpcClient&);
  void operator=(const PpbTCPSocketPrivateRpcClient);
};  // class PpbTCPSocketPrivateRpcClient

class PpbTestingRpcClient {
 public:
  static NaClSrpcError PPB_Testing_ReadImageData(
      NaClSrpcChannel* channel,
      PP_Resource device_context_2d,
      PP_Resource image,
      nacl_abi_size_t top_left_bytes, char* top_left,
      int32_t* success);
  static NaClSrpcError PPB_Testing_RunMessageLoop(
      NaClSrpcChannel* channel,
      PP_Instance instance);
  static NaClSrpcError PPB_Testing_QuitMessageLoop(
      NaClSrpcChannel* channel,
      PP_Instance instance);
  static NaClSrpcError PPB_Testing_GetLiveObjectsForInstance(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t* live_object_count);

 private:
  PpbTestingRpcClient();
  PpbTestingRpcClient(const PpbTestingRpcClient&);
  void operator=(const PpbTestingRpcClient);
};  // class PpbTestingRpcClient

class PpbUDPSocketPrivateRpcClient {
 public:
  static NaClSrpcError PPB_UDPSocket_Private_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance_id,
      PP_Resource* resource);
  static NaClSrpcError PPB_UDPSocket_Private_IsUDPSocket(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t* is_udp_socket_private);
  static NaClSrpcError PPB_UDPSocket_Private_Bind(
      NaClSrpcChannel* channel,
      PP_Resource udp_socket,
      nacl_abi_size_t addr_bytes, char* addr,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_UDPSocket_Private_RecvFrom(
      NaClSrpcChannel* channel,
      PP_Resource udp_socket,
      int32_t num_bytes,
      int32_t callback_id,
      nacl_abi_size_t* buffer_bytes, char* buffer,
      int32_t* pp_error_or_bytes);
  static NaClSrpcError PPB_UDPSocket_Private_GetRecvFromAddress(
      NaClSrpcChannel* channel,
      PP_Resource udp_socket,
      nacl_abi_size_t* addr_bytes, char* addr,
      int32_t* success);
  static NaClSrpcError PPB_UDPSocket_Private_SendTo(
      NaClSrpcChannel* channel,
      PP_Resource udp_socket,
      nacl_abi_size_t buffer_bytes, char* buffer,
      int32_t num_bytes,
      nacl_abi_size_t addr_bytes, char* addr,
      int32_t callback_id,
      int32_t* pp_error_or_bytes);
  static NaClSrpcError PPB_UDPSocket_Private_Close(
      NaClSrpcChannel* channel,
      PP_Resource udp_socket);

 private:
  PpbUDPSocketPrivateRpcClient();
  PpbUDPSocketPrivateRpcClient(const PpbUDPSocketPrivateRpcClient&);
  void operator=(const PpbUDPSocketPrivateRpcClient);
};  // class PpbUDPSocketPrivateRpcClient

class PpbURLLoaderRpcClient {
 public:
  static NaClSrpcError PPB_URLLoader_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource* resource);
  static NaClSrpcError PPB_URLLoader_IsURLLoader(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* is_url_loader);
  static NaClSrpcError PPB_URLLoader_Open(
      NaClSrpcChannel* channel,
      PP_Resource loader,
      PP_Resource request,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_URLLoader_FollowRedirect(
      NaClSrpcChannel* channel,
      PP_Resource loader,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_URLLoader_GetUploadProgress(
      NaClSrpcChannel* channel,
      PP_Resource loader,
      int64_t* bytes_sent,
      int64_t* total_bytes_to_be_sent,
      int32_t* success);
  static NaClSrpcError PPB_URLLoader_GetDownloadProgress(
      NaClSrpcChannel* channel,
      PP_Resource loader,
      int64_t* bytes_received,
      int64_t* total_bytes_to_be_received,
      int32_t* success);
  static NaClSrpcError PPB_URLLoader_GetResponseInfo(
      NaClSrpcChannel* channel,
      PP_Resource loader,
      PP_Resource* response);
  static NaClSrpcError PPB_URLLoader_ReadResponseBody(
      NaClSrpcChannel* channel,
      PP_Resource loader,
      int32_t bytes_to_read,
      int32_t callback_id,
      nacl_abi_size_t* buffer_bytes, char* buffer,
      int32_t* pp_error_or_bytes);
  static NaClSrpcError PPB_URLLoader_FinishStreamingToFile(
      NaClSrpcChannel* channel,
      PP_Resource loader,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_URLLoader_Close(
      NaClSrpcChannel* channel,
      PP_Resource loader);

 private:
  PpbURLLoaderRpcClient();
  PpbURLLoaderRpcClient(const PpbURLLoaderRpcClient&);
  void operator=(const PpbURLLoaderRpcClient);
};  // class PpbURLLoaderRpcClient

class PpbURLRequestInfoRpcClient {
 public:
  static NaClSrpcError PPB_URLRequestInfo_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource* resource);
  static NaClSrpcError PPB_URLRequestInfo_IsURLRequestInfo(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* success);
  static NaClSrpcError PPB_URLRequestInfo_SetProperty(
      NaClSrpcChannel* channel,
      PP_Resource request,
      int32_t property,
      nacl_abi_size_t value_bytes, char* value,
      int32_t* success);
  static NaClSrpcError PPB_URLRequestInfo_AppendDataToBody(
      NaClSrpcChannel* channel,
      PP_Resource request,
      nacl_abi_size_t data_bytes, char* data,
      int32_t* success);
  static NaClSrpcError PPB_URLRequestInfo_AppendFileToBody(
      NaClSrpcChannel* channel,
      PP_Resource request,
      PP_Resource file_ref,
      int64_t start_offset,
      int64_t number_of_bytes,
      double expected_last_modified_time,
      int32_t* success);

 private:
  PpbURLRequestInfoRpcClient();
  PpbURLRequestInfoRpcClient(const PpbURLRequestInfoRpcClient&);
  void operator=(const PpbURLRequestInfoRpcClient);
};  // class PpbURLRequestInfoRpcClient

class PpbURLResponseInfoRpcClient {
 public:
  static NaClSrpcError PPB_URLResponseInfo_IsURLResponseInfo(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* success);
  static NaClSrpcError PPB_URLResponseInfo_GetProperty(
      NaClSrpcChannel* channel,
      PP_Resource response,
      int32_t property,
      nacl_abi_size_t* value_bytes, char* value);
  static NaClSrpcError PPB_URLResponseInfo_GetBodyAsFileRef(
      NaClSrpcChannel* channel,
      PP_Resource response,
      PP_Resource* file_ref);

 private:
  PpbURLResponseInfoRpcClient();
  PpbURLResponseInfoRpcClient(const PpbURLResponseInfoRpcClient&);
  void operator=(const PpbURLResponseInfoRpcClient);
};  // class PpbURLResponseInfoRpcClient

class PpbWidgetRpcClient {
 public:
  static NaClSrpcError PPB_Widget_IsWidget(
      NaClSrpcChannel* channel,
      PP_Resource resource,
      int32_t* is_widget);
  static NaClSrpcError PPB_Widget_Paint(
      NaClSrpcChannel* channel,
      PP_Resource widget,
      nacl_abi_size_t rect_bytes, char* rect,
      PP_Resource image,
      int32_t* success);
  static NaClSrpcError PPB_Widget_HandleEvent(
      NaClSrpcChannel* channel,
      PP_Resource widget,
      PP_Resource event,
      int32_t* handled);
  static NaClSrpcError PPB_Widget_GetLocation(
      NaClSrpcChannel* channel,
      PP_Resource widget,
      nacl_abi_size_t* location_bytes, char* location,
      int32_t* visible);
  static NaClSrpcError PPB_Widget_SetLocation(
      NaClSrpcChannel* channel,
      PP_Resource widget,
      nacl_abi_size_t location_bytes, char* location);

 private:
  PpbWidgetRpcClient();
  PpbWidgetRpcClient(const PpbWidgetRpcClient&);
  void operator=(const PpbWidgetRpcClient);
};  // class PpbWidgetRpcClient

class PpbZoomRpcClient {
 public:
  static NaClSrpcError PPB_Zoom_ZoomChanged(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      double factor);
  static NaClSrpcError PPB_Zoom_ZoomLimitsChanged(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      double minimum_factor,
      double maximum_factor);

 private:
  PpbZoomRpcClient();
  PpbZoomRpcClient(const PpbZoomRpcClient&);
  void operator=(const PpbZoomRpcClient);
};  // class PpbZoomRpcClient




#endif  // GEN_PPAPI_PROXY_PPB_RPC_H_

