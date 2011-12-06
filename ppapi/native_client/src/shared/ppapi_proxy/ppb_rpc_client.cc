// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "untrusted/srpcgen/ppb_rpc.h"
#ifdef __native_client__
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif  // UNREFERENCED_PARAMETER
#else
#include "native_client/src/include/portability.h"
#endif  // __native_client__
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_core.h"

NaClSrpcError NaClFileRpcClient::StreamAsFile(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    char* url,
    int32_t callback_id)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "StreamAsFile:isi:",
      instance,
      url,
      callback_id
  );
  return retval;
}

NaClSrpcError NaClFileRpcClient::GetFileDesc(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    char* url,
    NaClSrpcImcDescType* file_desc)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "GetFileDesc:is:h",
      instance,
      url,
      file_desc
  );
  return retval;
}

NaClSrpcError PpbRpcClient::PPB_GetInterface(
    NaClSrpcChannel* channel,
    char* interface_name,
    int32_t* exports_interface_name)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_GetInterface:s:i",
      interface_name,
      exports_interface_name
  );
  return retval;
}

NaClSrpcError PpbAudioRpcClient::PPB_Audio_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource config,
    PP_Resource* out_resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_Create:ii:i",
      instance,
      config,
      out_resource
  );
  return retval;
}

NaClSrpcError PpbAudioRpcClient::PPB_Audio_IsAudio(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_IsAudio:i:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioRpcClient::PPB_Audio_GetCurrentConfig(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    PP_Resource* out_resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_GetCurrentConfig:i:i",
      resource,
      out_resource
  );
  return retval;
}

NaClSrpcError PpbAudioRpcClient::PPB_Audio_StopPlayback(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_StopPlayback:i:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioRpcClient::PPB_Audio_StartPlayback(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_StartPlayback:i:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioConfigRpcClient::PPB_AudioConfig_CreateStereo16Bit(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t sample_rate,
    int32_t sample_frame_count,
    PP_Resource* resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_CreateStereo16Bit:iii:i",
      instance,
      sample_rate,
      sample_frame_count,
      resource
  );
  return retval;
}

NaClSrpcError PpbAudioConfigRpcClient::PPB_AudioConfig_IsAudioConfig(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_IsAudioConfig:i:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioConfigRpcClient::PPB_AudioConfig_RecommendSampleFrameCount(
    NaClSrpcChannel* channel,
    int32_t request_sample_rate,
    int32_t request_sample_frame_count,
    int32_t* out_sample_frame_count)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_RecommendSampleFrameCount:ii:i",
      request_sample_rate,
      request_sample_frame_count,
      out_sample_frame_count
  );
  return retval;
}

NaClSrpcError PpbAudioConfigRpcClient::PPB_AudioConfig_GetSampleRate(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* sample_rate)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_GetSampleRate:i:i",
      resource,
      sample_rate
  );
  return retval;
}

NaClSrpcError PpbAudioConfigRpcClient::PPB_AudioConfig_GetSampleFrameCount(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* sample_frame_count)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_GetSampleFrameCount:i:i",
      resource,
      sample_frame_count
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_AddRefResource(
    NaClSrpcChannel* channel,
    PP_Resource resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_AddRefResource:i:",
      resource
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_ReleaseResource(
    NaClSrpcChannel* channel,
    PP_Resource resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_ReleaseResource:i:",
      resource
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::ReleaseResourceMultipleTimes(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t count)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "ReleaseResourceMultipleTimes:ii:",
      resource,
      count
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_GetTime(
    NaClSrpcChannel* channel,
    double* time)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_GetTime::d",
      time
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_GetTimeTicks(
    NaClSrpcChannel* channel,
    double* time_ticks)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_GetTimeTicks::d",
      time_ticks
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_CallOnMainThread(
    NaClSrpcChannel* channel,
    int32_t delay_in_milliseconds,
    int32_t callback_id,
    int32_t result)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_CallOnMainThread:iii:",
      delay_in_milliseconds,
      callback_id,
      result
  );
  return retval;
}

NaClSrpcError PpbCursorControlRpcClient::PPB_CursorControl_SetCursor(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t type,
    PP_Resource custom_image,
    nacl_abi_size_t hot_spot_bytes, char* hot_spot,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_CursorControl_SetCursor:iiiC:i",
      instance,
      type,
      custom_image,
      hot_spot_bytes, hot_spot,
      success
  );
  return retval;
}

NaClSrpcError PpbCursorControlRpcClient::PPB_CursorControl_LockCursor(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_CursorControl_LockCursor:i:i",
      instance,
      success
  );
  return retval;
}

NaClSrpcError PpbCursorControlRpcClient::PPB_CursorControl_UnlockCursor(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_CursorControl_UnlockCursor:i:i",
      instance,
      success
  );
  return retval;
}

NaClSrpcError PpbCursorControlRpcClient::PPB_CursorControl_HasCursorLock(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_CursorControl_HasCursorLock:i:i",
      instance,
      success
  );
  return retval;
}

NaClSrpcError PpbCursorControlRpcClient::PPB_CursorControl_CanLockCursor(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_CursorControl_CanLockCursor:i:i",
      instance,
      success
  );
  return retval;
}

NaClSrpcError PpbFileIORpcClient::PPB_FileIO_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource* resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Create:i:i",
      instance,
      resource
  );
  return retval;
}

NaClSrpcError PpbFileIORpcClient::PPB_FileIO_IsFileIO(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_IsFileIO:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbFileIORpcClient::PPB_FileIO_Open(
    NaClSrpcChannel* channel,
    PP_Resource file_io,
    PP_Resource file_ref,
    int32_t open_flags,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Open:iiii:i",
      file_io,
      file_ref,
      open_flags,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileIORpcClient::PPB_FileIO_Query(
    NaClSrpcChannel* channel,
    PP_Resource file_io,
    int32_t bytes_to_read,
    int32_t callback_id,
    nacl_abi_size_t* info_bytes, char* info,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Query:iii:Ci",
      file_io,
      bytes_to_read,
      callback_id,
      info_bytes, info,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileIORpcClient::PPB_FileIO_Touch(
    NaClSrpcChannel* channel,
    PP_Resource file_io,
    double last_access_time,
    double last_modified_time,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Touch:iddi:i",
      file_io,
      last_access_time,
      last_modified_time,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileIORpcClient::PPB_FileIO_Read(
    NaClSrpcChannel* channel,
    PP_Resource file_io,
    int64_t offset,
    int32_t bytes_to_read,
    int32_t callback_id,
    nacl_abi_size_t* buffer_bytes, char* buffer,
    int32_t* pp_error_or_bytes)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Read:ilii:Ci",
      file_io,
      offset,
      bytes_to_read,
      callback_id,
      buffer_bytes, buffer,
      pp_error_or_bytes
  );
  return retval;
}

NaClSrpcError PpbFileIORpcClient::PPB_FileIO_Write(
    NaClSrpcChannel* channel,
    PP_Resource file_io,
    int64_t offset,
    nacl_abi_size_t buffer_bytes, char* buffer,
    int32_t bytes_to_write,
    int32_t callback_id,
    int32_t* pp_error_or_bytes)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Write:ilCii:i",
      file_io,
      offset,
      buffer_bytes, buffer,
      bytes_to_write,
      callback_id,
      pp_error_or_bytes
  );
  return retval;
}

NaClSrpcError PpbFileIORpcClient::PPB_FileIO_SetLength(
    NaClSrpcChannel* channel,
    PP_Resource file_io,
    int64_t length,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_SetLength:ili:i",
      file_io,
      length,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileIORpcClient::PPB_FileIO_Flush(
    NaClSrpcChannel* channel,
    PP_Resource file_io,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Flush:ii:i",
      file_io,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileIORpcClient::PPB_FileIO_Close(
    NaClSrpcChannel* channel,
    PP_Resource file_io)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Close:i:",
      file_io
  );
  return retval;
}

NaClSrpcError PpbFileRefRpcClient::PPB_FileRef_Create(
    NaClSrpcChannel* channel,
    PP_Resource file_system,
    nacl_abi_size_t path_bytes, char* path,
    PP_Resource* resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileRef_Create:iC:i",
      file_system,
      path_bytes, path,
      resource
  );
  return retval;
}

NaClSrpcError PpbFileRefRpcClient::PPB_FileRef_IsFileRef(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileRef_IsFileRef:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbFileRefRpcClient::PPB_FileRef_GetFileSystemType(
    NaClSrpcChannel* channel,
    PP_Resource file_ref,
    int32_t* file_system_type)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileRef_GetFileSystemType:i:i",
      file_ref,
      file_system_type
  );
  return retval;
}

NaClSrpcError PpbFileRefRpcClient::PPB_FileRef_GetName(
    NaClSrpcChannel* channel,
    PP_Resource file_ref,
    nacl_abi_size_t* name_bytes, char* name)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileRef_GetName:i:C",
      file_ref,
      name_bytes, name
  );
  return retval;
}

NaClSrpcError PpbFileRefRpcClient::PPB_FileRef_GetPath(
    NaClSrpcChannel* channel,
    PP_Resource file_ref,
    nacl_abi_size_t* path_bytes, char* path)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileRef_GetPath:i:C",
      file_ref,
      path_bytes, path
  );
  return retval;
}

NaClSrpcError PpbFileRefRpcClient::PPB_FileRef_GetParent(
    NaClSrpcChannel* channel,
    PP_Resource file_ref,
    PP_Resource* parent)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileRef_GetParent:i:i",
      file_ref,
      parent
  );
  return retval;
}

NaClSrpcError PpbFileRefRpcClient::PPB_FileRef_MakeDirectory(
    NaClSrpcChannel* channel,
    PP_Resource directory_ref,
    int32_t make_ancestors,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileRef_MakeDirectory:iii:i",
      directory_ref,
      make_ancestors,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileRefRpcClient::PPB_FileRef_Touch(
    NaClSrpcChannel* channel,
    PP_Resource file_ref,
    double last_access_time,
    double last_modified_time,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileRef_Touch:iddi:i",
      file_ref,
      last_access_time,
      last_modified_time,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileRefRpcClient::PPB_FileRef_Delete(
    NaClSrpcChannel* channel,
    PP_Resource file_ref,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileRef_Delete:ii:i",
      file_ref,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileRefRpcClient::PPB_FileRef_Rename(
    NaClSrpcChannel* channel,
    PP_Resource file_ref,
    PP_Resource new_file_ref,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileRef_Rename:iii:i",
      file_ref,
      new_file_ref,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileSystemRpcClient::PPB_FileSystem_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t file_system_type,
    PP_Resource* resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileSystem_Create:ii:i",
      instance,
      file_system_type,
      resource
  );
  return retval;
}

NaClSrpcError PpbFileSystemRpcClient::PPB_FileSystem_IsFileSystem(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileSystem_IsFileSystem:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbFileSystemRpcClient::PPB_FileSystem_Open(
    NaClSrpcChannel* channel,
    PP_Resource file_system,
    int64_t expected_size,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileSystem_Open:ili:i",
      file_system,
      expected_size,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileSystemRpcClient::PPB_FileSystem_GetType(
    NaClSrpcChannel* channel,
    PP_Resource file_system,
    int32_t* type)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileSystem_GetType:i:i",
      file_system,
      type
  );
  return retval;
}

NaClSrpcError PpbFindRpcClient::PPB_Find_NumberOfFindResultsChanged(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t total,
    int32_t final_result)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Find_NumberOfFindResultsChanged:iii:",
      instance,
      total,
      final_result
  );
  return retval;
}

NaClSrpcError PpbFindRpcClient::PPB_Find_SelectedFindResultChanged(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t index)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Find_SelectedFindResultChanged:ii:",
      instance,
      index
  );
  return retval;
}

NaClSrpcError PpbFontRpcClient::PPB_Font_GetFontFamilies(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t* font_families_bytes, char* font_families)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Font_GetFontFamilies:i:C",
      instance,
      font_families_bytes, font_families
  );
  return retval;
}

NaClSrpcError PpbFontRpcClient::PPB_Font_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t description_bytes, char* description,
    nacl_abi_size_t face_bytes, char* face,
    PP_Resource* font)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Font_Create:iCC:i",
      instance,
      description_bytes, description,
      face_bytes, face,
      font
  );
  return retval;
}

NaClSrpcError PpbFontRpcClient::PPB_Font_IsFont(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* is_font)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Font_IsFont:i:i",
      resource,
      is_font
  );
  return retval;
}

NaClSrpcError PpbFontRpcClient::PPB_Font_Describe(
    NaClSrpcChannel* channel,
    PP_Resource font,
    nacl_abi_size_t* description_bytes, char* description,
    nacl_abi_size_t* face_bytes, char* face,
    nacl_abi_size_t* metrics_bytes, char* metrics,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Font_Describe:i:CCCi",
      font,
      description_bytes, description,
      face_bytes, face,
      metrics_bytes, metrics,
      success
  );
  return retval;
}

NaClSrpcError PpbFontRpcClient::PPB_Font_DrawTextAt(
    NaClSrpcChannel* channel,
    PP_Resource font,
    PP_Resource image_data,
    nacl_abi_size_t text_run_bytes, char* text_run,
    nacl_abi_size_t text_bytes, char* text,
    nacl_abi_size_t position_bytes, char* position,
    int32_t color,
    nacl_abi_size_t clip_bytes, char* clip,
    int32_t image_data_is_opaque,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Font_DrawTextAt:iiCCCiCi:i",
      font,
      image_data,
      text_run_bytes, text_run,
      text_bytes, text,
      position_bytes, position,
      color,
      clip_bytes, clip,
      image_data_is_opaque,
      success
  );
  return retval;
}

NaClSrpcError PpbFontRpcClient::PPB_Font_MeasureText(
    NaClSrpcChannel* channel,
    PP_Resource font,
    nacl_abi_size_t text_run_bytes, char* text_run,
    nacl_abi_size_t text_bytes, char* text,
    int32_t* width)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Font_MeasureText:iCC:i",
      font,
      text_run_bytes, text_run,
      text_bytes, text,
      width
  );
  return retval;
}

NaClSrpcError PpbFontRpcClient::PPB_Font_CharacterOffsetForPixel(
    NaClSrpcChannel* channel,
    PP_Resource font,
    nacl_abi_size_t text_run_bytes, char* text_run,
    nacl_abi_size_t text_bytes, char* text,
    int32_t pixel_position,
    int32_t* offset)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Font_CharacterOffsetForPixel:iCCi:i",
      font,
      text_run_bytes, text_run,
      text_bytes, text,
      pixel_position,
      offset
  );
  return retval;
}

NaClSrpcError PpbFontRpcClient::PPB_Font_PixelOffsetForCharacter(
    NaClSrpcChannel* channel,
    PP_Resource font,
    nacl_abi_size_t text_run_bytes, char* text_run,
    nacl_abi_size_t text_bytes, char* text,
    int32_t char_offset,
    int32_t* offset)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Font_PixelOffsetForCharacter:iCCi:i",
      font,
      text_run_bytes, text_run,
      text_bytes, text,
      char_offset,
      offset
  );
  return retval;
}

NaClSrpcError PpbFullscreenRpcClient::PPB_Fullscreen_SetFullscreen(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t fullscreen,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Fullscreen_SetFullscreen:ii:i",
      instance,
      fullscreen,
      success
  );
  return retval;
}

NaClSrpcError PpbFullscreenRpcClient::PPB_Fullscreen_GetScreenSize(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t* size_bytes, char* size,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Fullscreen_GetScreenSize:i:Ci",
      instance,
      size_bytes, size,
      success
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t size_bytes, char* size,
    int32_t is_always_opaque,
    PP_Resource* resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Create:iCi:i",
      instance,
      size_bytes, size,
      is_always_opaque,
      resource
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_IsGraphics2D(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_IsGraphics2D:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Describe(
    NaClSrpcChannel* channel,
    PP_Resource graphics_2d,
    nacl_abi_size_t* size_bytes, char* size,
    int32_t* is_always_opaque,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Describe:i:Cii",
      graphics_2d,
      size_bytes, size,
      is_always_opaque,
      success
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_PaintImageData(
    NaClSrpcChannel* channel,
    PP_Resource graphics_2d,
    PP_Resource image,
    nacl_abi_size_t top_left_bytes, char* top_left,
    nacl_abi_size_t src_rect_bytes, char* src_rect)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_PaintImageData:iiCC:",
      graphics_2d,
      image,
      top_left_bytes, top_left,
      src_rect_bytes, src_rect
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Scroll(
    NaClSrpcChannel* channel,
    PP_Resource graphics_2d,
    nacl_abi_size_t clip_rect_bytes, char* clip_rect,
    nacl_abi_size_t amount_bytes, char* amount)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Scroll:iCC:",
      graphics_2d,
      clip_rect_bytes, clip_rect,
      amount_bytes, amount
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_ReplaceContents(
    NaClSrpcChannel* channel,
    PP_Resource graphics_2d,
    PP_Resource image)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_ReplaceContents:ii:",
      graphics_2d,
      image
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Flush(
    NaClSrpcChannel* channel,
    PP_Resource graphics_2d,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Flush:ii:i",
      graphics_2d,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3D_GetAttribMaxValue(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t attribute,
    int32_t* value,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3D_GetAttribMaxValue:ii:ii",
      instance,
      attribute,
      value,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3D_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource share_context,
    nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
    PP_Resource* resource_id)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3D_Create:iiI:i",
      instance,
      share_context,
      attrib_list_bytes, attrib_list,
      resource_id
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3D_GetAttribs(
    NaClSrpcChannel* channel,
    PP_Resource context,
    nacl_abi_size_t input_attrib_list_bytes, int32_t* input_attrib_list,
    nacl_abi_size_t* output_attrib_list_bytes, int32_t* output_attrib_list,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3D_GetAttribs:iI:Ii",
      context,
      input_attrib_list_bytes, input_attrib_list,
      output_attrib_list_bytes, output_attrib_list,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3D_SetAttribs(
    NaClSrpcChannel* channel,
    PP_Resource context,
    nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3D_SetAttribs:iI:i",
      context,
      attrib_list_bytes, attrib_list,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3D_GetError(
    NaClSrpcChannel* channel,
    PP_Resource context,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3D_GetError:i:i",
      context,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3D_SwapBuffers(
    NaClSrpcChannel* channel,
    PP_Resource context,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3D_SwapBuffers:ii:i",
      context,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_CreateRaw(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource share_context,
    nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
    PP_Resource* resource_id)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3DTrusted_CreateRaw:iiI:i",
      instance,
      share_context,
      attrib_list_bytes, attrib_list,
      resource_id
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_InitCommandBuffer(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t size,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3DTrusted_InitCommandBuffer:ii:i",
      resource_id,
      size,
      success
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_GetRingBuffer(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    NaClSrpcImcDescType* shm_desc,
    int32_t* shm_size)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3DTrusted_GetRingBuffer:i:hi",
      resource_id,
      shm_desc,
      shm_size
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_GetState(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    nacl_abi_size_t* state_bytes, char* state)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3DTrusted_GetState:i:C",
      resource_id,
      state_bytes, state
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_Flush(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t put_offset)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3DTrusted_Flush:ii:",
      resource_id,
      put_offset
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_FlushSync(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t put_offset,
    nacl_abi_size_t* state_bytes, char* state)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3DTrusted_FlushSync:ii:C",
      resource_id,
      put_offset,
      state_bytes, state
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_FlushSyncFast(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t put_offset,
    int32_t last_known_offset,
    nacl_abi_size_t* state_bytes, char* state)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3DTrusted_FlushSyncFast:iii:C",
      resource_id,
      put_offset,
      last_known_offset,
      state_bytes, state
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_CreateTransferBuffer(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t size,
    int32_t request_id,
    int32_t* id)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3DTrusted_CreateTransferBuffer:iii:i",
      resource_id,
      size,
      request_id,
      id
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_DestroyTransferBuffer(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t id)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3DTrusted_DestroyTransferBuffer:ii:",
      resource_id,
      id
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_GetTransferBuffer(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t id,
    NaClSrpcImcDescType* shm_desc,
    int32_t* shm_size)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics3DTrusted_GetTransferBuffer:ii:hi",
      resource_id,
      id,
      shm_desc,
      shm_size
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_GetNativeImageDataFormat(
    NaClSrpcChannel* channel,
    int32_t* format)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_GetNativeImageDataFormat::i",
      format
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_IsImageDataFormatSupported(
    NaClSrpcChannel* channel,
    int32_t format,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_IsImageDataFormatSupported:i:i",
      format,
      success
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t format,
    nacl_abi_size_t size_bytes, char* size,
    int32_t init_to_zero,
    PP_Resource* resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_Create:iiCi:i",
      instance,
      format,
      size_bytes, size,
      init_to_zero,
      resource
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_IsImageData(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_IsImageData:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_Describe(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    nacl_abi_size_t* desc_bytes, char* desc,
    NaClSrpcImcDescType* shm,
    int32_t* shm_size,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_Describe:i:Chii",
      resource,
      desc_bytes, desc,
      shm,
      shm_size,
      success
  );
  return retval;
}

NaClSrpcError PpbInputEventRpcClient::PPB_InputEvent_RequestInputEvents(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t event_classes,
    int32_t filtered,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_InputEvent_RequestInputEvents:iii:i",
      instance,
      event_classes,
      filtered,
      success
  );
  return retval;
}

NaClSrpcError PpbInputEventRpcClient::PPB_InputEvent_ClearInputEventRequest(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t event_classes)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_InputEvent_ClearInputEventRequest:ii:",
      instance,
      event_classes
  );
  return retval;
}

NaClSrpcError PpbInputEventRpcClient::PPB_InputEvent_CreateMouseInputEvent(
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
    PP_Resource* resource_id)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_InputEvent_CreateMouseInputEvent:iidiiiiiii:i",
      instance,
      type,
      time_stamp,
      modifiers,
      mouse_button,
      mouse_position_x,
      mouse_position_y,
      click_count,
      mouse_movement_x,
      mouse_movement_y,
      resource_id
  );
  return retval;
}

NaClSrpcError PpbInputEventRpcClient::PPB_InputEvent_CreateWheelInputEvent(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    double time_stamp,
    int32_t modifiers,
    double wheel_delta_x,
    double wheel_delta_y,
    double wheel_ticks_x,
    double wheel_ticks_y,
    int32_t scroll_by_page,
    PP_Resource* resource_id)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_InputEvent_CreateWheelInputEvent:ididdddi:i",
      instance,
      time_stamp,
      modifiers,
      wheel_delta_x,
      wheel_delta_y,
      wheel_ticks_x,
      wheel_ticks_y,
      scroll_by_page,
      resource_id
  );
  return retval;
}

NaClSrpcError PpbInputEventRpcClient::PPB_InputEvent_CreateKeyboardInputEvent(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t type,
    double time_stamp,
    int32_t modifiers,
    int32_t key_code,
    nacl_abi_size_t character_text_bytes, char* character_text,
    PP_Resource* resource_id)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_InputEvent_CreateKeyboardInputEvent:iidiiC:i",
      instance,
      type,
      time_stamp,
      modifiers,
      key_code,
      character_text_bytes, character_text,
      resource_id
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_BindGraphics(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource graphics_device,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_BindGraphics:ii:i",
      instance,
      graphics_device,
      success
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_IsFullFrame(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t* is_full_frame)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_IsFullFrame:i:i",
      instance,
      is_full_frame
  );
  return retval;
}

NaClSrpcError PpbMessagingRpcClient::PPB_Messaging_PostMessage(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t message_bytes, char* message)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Messaging_PostMessage:iC:",
      instance,
      message_bytes, message
  );
  return retval;
}

NaClSrpcError PpbMouseLockRpcClient::PPB_MouseLock_LockMouse(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_MouseLock_LockMouse:ii:i",
      instance,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbMouseLockRpcClient::PPB_MouseLock_UnlockMouse(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_MouseLock_UnlockMouse:i:",
      instance
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_GetLocalizedString(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t string_id,
    nacl_abi_size_t* string_bytes, char* string)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_GetLocalizedString:ii:C",
      instance,
      string_id,
      string_bytes, string
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_GetResourceImage(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t image_id,
    PP_Resource* image)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_GetResourceImage:ii:i",
      instance,
      image_id,
      image
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_GetFontFileWithFallback(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t description_bytes, char* description,
    nacl_abi_size_t face_bytes, char* face,
    int32_t charset,
    PP_Resource* font)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_GetFontFileWithFallback:iCCi:i",
      instance,
      description_bytes, description,
      face_bytes, face,
      charset,
      font
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_GetFontTableForPrivateFontFile(
    NaClSrpcChannel* channel,
    PP_Resource font_file,
    int32_t table,
    nacl_abi_size_t* output_bytes, char* output,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_GetFontTableForPrivateFontFile:ii:Ci",
      font_file,
      table,
      output_bytes, output,
      success
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_SearchString(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t string_bytes, char* string,
    nacl_abi_size_t term_bytes, char* term,
    int32_t case_sensitive,
    nacl_abi_size_t* results_bytes, char* results,
    int32_t* count)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_SearchString:iCCi:Ci",
      instance,
      string_bytes, string,
      term_bytes, term,
      case_sensitive,
      results_bytes, results,
      count
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_DidStartLoading(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_DidStartLoading:i:",
      instance
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_DidStopLoading(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_DidStopLoading:i:",
      instance
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_SetContentRestriction(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t restrictions)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_SetContentRestriction:ii:",
      instance,
      restrictions
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_HistogramPDFPageCount(
    NaClSrpcChannel* channel,
    int32_t count)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_HistogramPDFPageCount:i:",
      count
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_UserMetricsRecordAction(
    NaClSrpcChannel* channel,
    nacl_abi_size_t action_bytes, char* action)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_UserMetricsRecordAction:C:",
      action_bytes, action
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_HasUnsupportedFeature(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_HasUnsupportedFeature:i:",
      instance
  );
  return retval;
}

NaClSrpcError PpbPdfRpcClient::PPB_PDF_SaveAs(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_PDF_SaveAs:i:",
      instance
  );
  return retval;
}

NaClSrpcError PpbScrollbarRpcClient::PPB_Scrollbar_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t vertical,
    PP_Resource* scrollbar)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Scrollbar_Create:ii:i",
      instance,
      vertical,
      scrollbar
  );
  return retval;
}

NaClSrpcError PpbScrollbarRpcClient::PPB_Scrollbar_IsScrollbar(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* is_scrollbar)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Scrollbar_IsScrollbar:i:i",
      resource,
      is_scrollbar
  );
  return retval;
}

NaClSrpcError PpbScrollbarRpcClient::PPB_Scrollbar_IsOverlay(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* is_overlay)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Scrollbar_IsOverlay:i:i",
      resource,
      is_overlay
  );
  return retval;
}

NaClSrpcError PpbScrollbarRpcClient::PPB_Scrollbar_GetThickness(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* thickness)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Scrollbar_GetThickness:i:i",
      resource,
      thickness
  );
  return retval;
}

NaClSrpcError PpbScrollbarRpcClient::PPB_Scrollbar_GetValue(
    NaClSrpcChannel* channel,
    PP_Resource scrollbar,
    int32_t* value)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Scrollbar_GetValue:i:i",
      scrollbar,
      value
  );
  return retval;
}

NaClSrpcError PpbScrollbarRpcClient::PPB_Scrollbar_SetValue(
    NaClSrpcChannel* channel,
    PP_Resource scrollbar,
    int32_t value)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Scrollbar_SetValue:ii:",
      scrollbar,
      value
  );
  return retval;
}

NaClSrpcError PpbScrollbarRpcClient::PPB_Scrollbar_SetDocumentSize(
    NaClSrpcChannel* channel,
    PP_Resource scrollbar,
    int32_t size)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Scrollbar_SetDocumentSize:ii:",
      scrollbar,
      size
  );
  return retval;
}

NaClSrpcError PpbScrollbarRpcClient::PPB_Scrollbar_SetTickMarks(
    NaClSrpcChannel* channel,
    PP_Resource scrollbar,
    nacl_abi_size_t tick_marks_bytes, char* tick_marks,
    int32_t count)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Scrollbar_SetTickMarks:iCi:",
      scrollbar,
      tick_marks_bytes, tick_marks,
      count
  );
  return retval;
}

NaClSrpcError PpbScrollbarRpcClient::PPB_Scrollbar_ScrollBy(
    NaClSrpcChannel* channel,
    PP_Resource scrollbar,
    int32_t unit,
    int32_t multiplier)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Scrollbar_ScrollBy:iii:",
      scrollbar,
      unit,
      multiplier
  );
  return retval;
}

NaClSrpcError PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource* resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_TCPSocket_Private_Create:i:i",
      instance,
      resource
  );
  return retval;
}

NaClSrpcError PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_IsTCPSocket(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* is_tcp_socket)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_TCPSocket_Private_IsTCPSocket:i:i",
      resource,
      is_tcp_socket
  );
  return retval;
}

NaClSrpcError PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_Connect(
    NaClSrpcChannel* channel,
    PP_Resource tcp_socket,
    char* host,
    int32_t port,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_TCPSocket_Private_Connect:isii:i",
      tcp_socket,
      host,
      port,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_ConnectWithNetAddress(
    NaClSrpcChannel* channel,
    PP_Resource tcp_socket,
    nacl_abi_size_t addr_bytes, char* addr,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_TCPSocket_Private_ConnectWithNetAddress:iCi:i",
      tcp_socket,
      addr_bytes, addr,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_GetLocalAddress(
    NaClSrpcChannel* channel,
    PP_Resource tcp_socket,
    nacl_abi_size_t* local_addr_bytes, char* local_addr,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_TCPSocket_Private_GetLocalAddress:i:Ci",
      tcp_socket,
      local_addr_bytes, local_addr,
      success
  );
  return retval;
}

NaClSrpcError PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_GetRemoteAddress(
    NaClSrpcChannel* channel,
    PP_Resource tcp_socket,
    nacl_abi_size_t* remote_addr_bytes, char* remote_addr,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_TCPSocket_Private_GetRemoteAddress:i:Ci",
      tcp_socket,
      remote_addr_bytes, remote_addr,
      success
  );
  return retval;
}

NaClSrpcError PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_SSLHandshake(
    NaClSrpcChannel* channel,
    PP_Resource tcp_socket,
    char* server_name,
    int32_t server_port,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_TCPSocket_Private_SSLHandshake:isii:i",
      tcp_socket,
      server_name,
      server_port,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_Read(
    NaClSrpcChannel* channel,
    PP_Resource tcp_socket,
    int32_t bytes_to_read,
    int32_t callback_id,
    nacl_abi_size_t* buffer_bytes, char* buffer,
    int32_t* pp_error_or_bytes)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_TCPSocket_Private_Read:iii:Ci",
      tcp_socket,
      bytes_to_read,
      callback_id,
      buffer_bytes, buffer,
      pp_error_or_bytes
  );
  return retval;
}

NaClSrpcError PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_Write(
    NaClSrpcChannel* channel,
    PP_Resource tcp_socket,
    nacl_abi_size_t buffer_bytes, char* buffer,
    int32_t bytes_to_write,
    int32_t callback_id,
    int32_t* pp_error_or_bytes)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_TCPSocket_Private_Write:iCii:i",
      tcp_socket,
      buffer_bytes, buffer,
      bytes_to_write,
      callback_id,
      pp_error_or_bytes
  );
  return retval;
}

NaClSrpcError PpbTCPSocketPrivateRpcClient::PPB_TCPSocket_Private_Disconnect(
    NaClSrpcChannel* channel,
    PP_Resource tcp_socket)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_TCPSocket_Private_Disconnect:i:",
      tcp_socket
  );
  return retval;
}

NaClSrpcError PpbTestingRpcClient::PPB_Testing_ReadImageData(
    NaClSrpcChannel* channel,
    PP_Resource device_context_2d,
    PP_Resource image,
    nacl_abi_size_t top_left_bytes, char* top_left,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Testing_ReadImageData:iiC:i",
      device_context_2d,
      image,
      top_left_bytes, top_left,
      success
  );
  return retval;
}

NaClSrpcError PpbTestingRpcClient::PPB_Testing_RunMessageLoop(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Testing_RunMessageLoop:i:",
      instance
  );
  return retval;
}

NaClSrpcError PpbTestingRpcClient::PPB_Testing_QuitMessageLoop(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Testing_QuitMessageLoop:i:",
      instance
  );
  return retval;
}

NaClSrpcError PpbTestingRpcClient::PPB_Testing_GetLiveObjectsForInstance(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t* live_object_count)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Testing_GetLiveObjectsForInstance:i:i",
      instance,
      live_object_count
  );
  return retval;
}

NaClSrpcError PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance_id,
    PP_Resource* resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_UDPSocket_Private_Create:i:i",
      instance_id,
      resource
  );
  return retval;
}

NaClSrpcError PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_IsUDPSocket(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t* is_udp_socket_private)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_UDPSocket_Private_IsUDPSocket:i:i",
      resource_id,
      is_udp_socket_private
  );
  return retval;
}

NaClSrpcError PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_Bind(
    NaClSrpcChannel* channel,
    PP_Resource udp_socket,
    nacl_abi_size_t addr_bytes, char* addr,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_UDPSocket_Private_Bind:iCi:i",
      udp_socket,
      addr_bytes, addr,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_RecvFrom(
    NaClSrpcChannel* channel,
    PP_Resource udp_socket,
    int32_t num_bytes,
    int32_t callback_id,
    nacl_abi_size_t* buffer_bytes, char* buffer,
    int32_t* pp_error_or_bytes)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_UDPSocket_Private_RecvFrom:iii:Ci",
      udp_socket,
      num_bytes,
      callback_id,
      buffer_bytes, buffer,
      pp_error_or_bytes
  );
  return retval;
}

NaClSrpcError PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_GetRecvFromAddress(
    NaClSrpcChannel* channel,
    PP_Resource udp_socket,
    nacl_abi_size_t* addr_bytes, char* addr,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_UDPSocket_Private_GetRecvFromAddress:i:Ci",
      udp_socket,
      addr_bytes, addr,
      success
  );
  return retval;
}

NaClSrpcError PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_SendTo(
    NaClSrpcChannel* channel,
    PP_Resource udp_socket,
    nacl_abi_size_t buffer_bytes, char* buffer,
    int32_t num_bytes,
    nacl_abi_size_t addr_bytes, char* addr,
    int32_t callback_id,
    int32_t* pp_error_or_bytes)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_UDPSocket_Private_SendTo:iCiCi:i",
      udp_socket,
      buffer_bytes, buffer,
      num_bytes,
      addr_bytes, addr,
      callback_id,
      pp_error_or_bytes
  );
  return retval;
}

NaClSrpcError PpbUDPSocketPrivateRpcClient::PPB_UDPSocket_Private_Close(
    NaClSrpcChannel* channel,
    PP_Resource udp_socket)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_UDPSocket_Private_Close:i:",
      udp_socket
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource* resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_Create:i:i",
      instance,
      resource
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_IsURLLoader(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* is_url_loader)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_IsURLLoader:i:i",
      resource,
      is_url_loader
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_Open(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    PP_Resource request,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_Open:iii:i",
      loader,
      request,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_FollowRedirect(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_FollowRedirect:ii:i",
      loader,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_GetUploadProgress(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    int64_t* bytes_sent,
    int64_t* total_bytes_to_be_sent,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_GetUploadProgress:i:lli",
      loader,
      bytes_sent,
      total_bytes_to_be_sent,
      success
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_GetDownloadProgress(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_GetDownloadProgress:i:lli",
      loader,
      bytes_received,
      total_bytes_to_be_received,
      success
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_GetResponseInfo(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    PP_Resource* response)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_GetResponseInfo:i:i",
      loader,
      response
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_ReadResponseBody(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    int32_t bytes_to_read,
    int32_t callback_id,
    nacl_abi_size_t* buffer_bytes, char* buffer,
    int32_t* pp_error_or_bytes)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_ReadResponseBody:iii:Ci",
      loader,
      bytes_to_read,
      callback_id,
      buffer_bytes, buffer,
      pp_error_or_bytes
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_FinishStreamingToFile(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    int32_t callback_id,
    int32_t* pp_error)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_FinishStreamingToFile:ii:i",
      loader,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_Close(
    NaClSrpcChannel* channel,
    PP_Resource loader)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_Close:i:",
      loader
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource* resource)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_Create:i:i",
      instance,
      resource
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_IsURLRequestInfo(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_IsURLRequestInfo:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_SetProperty(
    NaClSrpcChannel* channel,
    PP_Resource request,
    int32_t property,
    nacl_abi_size_t value_bytes, char* value,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_SetProperty:iiC:i",
      request,
      property,
      value_bytes, value,
      success
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_AppendDataToBody(
    NaClSrpcChannel* channel,
    PP_Resource request,
    nacl_abi_size_t data_bytes, char* data,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_AppendDataToBody:iC:i",
      request,
      data_bytes, data,
      success
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_AppendFileToBody(
    NaClSrpcChannel* channel,
    PP_Resource request,
    PP_Resource file_ref,
    int64_t start_offset,
    int64_t number_of_bytes,
    double expected_last_modified_time,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_AppendFileToBody:iilld:i",
      request,
      file_ref,
      start_offset,
      number_of_bytes,
      expected_last_modified_time,
      success
  );
  return retval;
}

NaClSrpcError PpbURLResponseInfoRpcClient::PPB_URLResponseInfo_IsURLResponseInfo(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLResponseInfo_IsURLResponseInfo:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbURLResponseInfoRpcClient::PPB_URLResponseInfo_GetProperty(
    NaClSrpcChannel* channel,
    PP_Resource response,
    int32_t property,
    nacl_abi_size_t* value_bytes, char* value)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLResponseInfo_GetProperty:ii:C",
      response,
      property,
      value_bytes, value
  );
  return retval;
}

NaClSrpcError PpbURLResponseInfoRpcClient::PPB_URLResponseInfo_GetBodyAsFileRef(
    NaClSrpcChannel* channel,
    PP_Resource response,
    PP_Resource* file_ref)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLResponseInfo_GetBodyAsFileRef:i:i",
      response,
      file_ref
  );
  return retval;
}

NaClSrpcError PpbWidgetRpcClient::PPB_Widget_IsWidget(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* is_widget)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Widget_IsWidget:i:i",
      resource,
      is_widget
  );
  return retval;
}

NaClSrpcError PpbWidgetRpcClient::PPB_Widget_Paint(
    NaClSrpcChannel* channel,
    PP_Resource widget,
    nacl_abi_size_t rect_bytes, char* rect,
    PP_Resource image,
    int32_t* success)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Widget_Paint:iCi:i",
      widget,
      rect_bytes, rect,
      image,
      success
  );
  return retval;
}

NaClSrpcError PpbWidgetRpcClient::PPB_Widget_HandleEvent(
    NaClSrpcChannel* channel,
    PP_Resource widget,
    PP_Resource event,
    int32_t* handled)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Widget_HandleEvent:ii:i",
      widget,
      event,
      handled
  );
  return retval;
}

NaClSrpcError PpbWidgetRpcClient::PPB_Widget_GetLocation(
    NaClSrpcChannel* channel,
    PP_Resource widget,
    nacl_abi_size_t* location_bytes, char* location,
    int32_t* visible)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Widget_GetLocation:i:Ci",
      widget,
      location_bytes, location,
      visible
  );
  return retval;
}

NaClSrpcError PpbWidgetRpcClient::PPB_Widget_SetLocation(
    NaClSrpcChannel* channel,
    PP_Resource widget,
    nacl_abi_size_t location_bytes, char* location)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Widget_SetLocation:iC:",
      widget,
      location_bytes, location
  );
  return retval;
}

NaClSrpcError PpbZoomRpcClient::PPB_Zoom_ZoomChanged(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    double factor)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Zoom_ZoomChanged:id:",
      instance,
      factor
  );
  return retval;
}

NaClSrpcError PpbZoomRpcClient::PPB_Zoom_ZoomLimitsChanged(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    double minimum_factor,
    double maximum_factor)  {
  VCHECK(ppapi_proxy::PPBCoreInterface()->IsMainThread(),
         ("%s: PPAPI calls are not supported off the main thread\n",
          __FUNCTION__));
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Zoom_ZoomLimitsChanged:idd:",
      instance,
      minimum_factor,
      maximum_factor
  );
  return retval;
}


