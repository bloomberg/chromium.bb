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

class ObjectStubRpcServer {
 public:
  static void HasProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void HasMethod(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void GetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* value_bytes, char* value,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void GetAllPropertyNames(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* property_count,
      nacl_abi_size_t* properties_bytes, char* properties,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void SetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t value_bytes, char* value,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void RemoveProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void Call(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void Construct(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception);
  static void Deallocate(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability);

 private:
  ObjectStubRpcServer();
  ObjectStubRpcServer(const ObjectStubRpcServer&);
  void operator=(const ObjectStubRpcServer);
};  // class ObjectStubRpcServer

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

class PpbFileIODevRpcServer {
 public:
  static void PPB_FileIO_Dev_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      PP_Resource* resource);
  static void PPB_FileIO_Dev_IsFileIO(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource,
      int32_t* success);
  static void PPB_FileIO_Dev_Open(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io,
      PP_Resource file_ref,
      int32_t open_flags,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_FileIO_Dev_Read(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource file_io,
      int64_t offset,
      int32_t bytes_to_read,
      int32_t callback_id,
      nacl_abi_size_t* buffer_bytes, char* buffer,
      int32_t* pp_error_or_bytes);

 private:
  PpbFileIODevRpcServer();
  PpbFileIODevRpcServer(const PpbFileIODevRpcServer&);
  void operator=(const PpbFileIODevRpcServer);
};  // class PpbFileIODevRpcServer

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
  static void PPB_Context3D_BindSurfaces(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource context,
      PP_Resource draw_surface,
      PP_Resource read_surface,
      int32_t* error_code);
  static void PPB_Surface3D_Create(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t config,
      nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
      PP_Resource* resource_id);
  static void PPB_Surface3D_SwapBuffers(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource surface,
      int32_t callback_id,
      int32_t* pp_error);
  static void PPB_Context3DTrusted_CreateRaw(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      int32_t config,
      PP_Resource share_context,
      nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
      PP_Resource* resource_id);
  static void PPB_Context3DTrusted_Initialize(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t size,
      int32_t* success);
  static void PPB_Context3DTrusted_GetRingBuffer(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      NaClSrpcImcDescType* shm_desc,
      int32_t* shm_size);
  static void PPB_Context3DTrusted_GetState(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      nacl_abi_size_t* state_bytes, char* state);
  static void PPB_Context3DTrusted_Flush(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t put_offset);
  static void PPB_Context3DTrusted_FlushSync(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t put_offset,
      nacl_abi_size_t* state_bytes, char* state);
  static void PPB_Context3DTrusted_CreateTransferBuffer(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t size,
      int32_t* id);
  static void PPB_Context3DTrusted_DestroyTransferBuffer(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Resource resource_id,
      int32_t id);
  static void PPB_Context3DTrusted_GetTransferBuffer(
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

class PpbInstanceRpcServer {
 public:
  static void PPB_Instance_GetWindowObject(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t* window_bytes, char* window);
  static void PPB_Instance_GetOwnerElementObject(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t* owner_bytes, char* owner);
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
  static void PPB_Instance_ExecuteScript(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t script_bytes, char* script,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* result_bytes, char* result,
      nacl_abi_size_t* exception_bytes, char* exception);

 private:
  PpbInstanceRpcServer();
  PpbInstanceRpcServer(const PpbInstanceRpcServer&);
  void operator=(const PpbInstanceRpcServer);
};  // class PpbInstanceRpcServer

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

class PpbRpcs {
 public:
  static NaClSrpcHandlerDesc srpc_methods[];
};  // class PpbRpcs

#endif  // GEN_PPAPI_PROXY_PPB_RPC_H_

