// Copyright (c) 2010 The Native Client Authors. All rights reserved.
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

class ObjectStubRpcClient {
 public:
  static NaClSrpcError HasProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError HasMethod(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError GetProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* value_bytes, char* value,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError GetAllPropertyNames(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* property_count,
      nacl_abi_size_t* properties_bytes, char* properties,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError SetProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t value_bytes, char* value,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError RemoveProperty(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError Call(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError Construct(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception);
  static NaClSrpcError Deallocate(
      NaClSrpcChannel* channel,
      nacl_abi_size_t capability_bytes, char* capability);

 private:
  ObjectStubRpcClient();
  ObjectStubRpcClient(const ObjectStubRpcClient&);
  void operator=(const ObjectStubRpcClient);
};  // class ObjectStubRpcClient

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
  static NaClSrpcError PPB_Context3D_BindSurfaces(
      NaClSrpcChannel* channel,
      PP_Resource context,
      PP_Resource draw_surface,
      PP_Resource read_surface,
      int32_t* error_code);
  static NaClSrpcError PPB_Surface3D_Create(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t config,
      nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
      PP_Resource* resource_id);
  static NaClSrpcError PPB_Surface3D_SwapBuffers(
      NaClSrpcChannel* channel,
      PP_Resource surface,
      int32_t callback_id,
      int32_t* pp_error);
  static NaClSrpcError PPB_Context3DTrusted_CreateRaw(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t config,
      PP_Resource share_context,
      nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
      PP_Resource* resource_id);
  static NaClSrpcError PPB_Context3DTrusted_Initialize(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t size,
      int32_t* success);
  static NaClSrpcError PPB_Context3DTrusted_GetRingBuffer(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      NaClSrpcImcDescType* shm_desc,
      int32_t* shm_size);
  static NaClSrpcError PPB_Context3DTrusted_GetState(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      nacl_abi_size_t* state_bytes, char* state);
  static NaClSrpcError PPB_Context3DTrusted_Flush(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t put_offset);
  static NaClSrpcError PPB_Context3DTrusted_FlushSync(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t put_offset,
      nacl_abi_size_t* state_bytes, char* state);
  static NaClSrpcError PPB_Context3DTrusted_CreateTransferBuffer(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t size,
      int32_t* id);
  static NaClSrpcError PPB_Context3DTrusted_DestroyTransferBuffer(
      NaClSrpcChannel* channel,
      PP_Resource resource_id,
      int32_t id);
  static NaClSrpcError PPB_Context3DTrusted_GetTransferBuffer(
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

class PpbInstanceRpcClient {
 public:
  static NaClSrpcError PPB_Instance_GetWindowObject(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t* window_bytes, char* window);
  static NaClSrpcError PPB_Instance_GetOwnerElementObject(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t* owner_bytes, char* owner);
  static NaClSrpcError PPB_Instance_BindGraphics(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      PP_Resource graphics_device,
      int32_t* success);
  static NaClSrpcError PPB_Instance_IsFullFrame(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      int32_t* is_full_frame);
  static NaClSrpcError PPB_Instance_ExecuteScript(
      NaClSrpcChannel* channel,
      PP_Instance instance,
      nacl_abi_size_t script_bytes, char* script,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* result_bytes, char* result,
      nacl_abi_size_t* exception_bytes, char* exception);

 private:
  PpbInstanceRpcClient();
  PpbInstanceRpcClient(const PpbInstanceRpcClient&);
  void operator=(const PpbInstanceRpcClient);
};  // class PpbInstanceRpcClient

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



#endif  // GEN_PPAPI_PROXY_PPB_RPC_H_

