// Copyright (c) 2010 The Native Client Authors. All rights reserved.
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
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

NaClSrpcError ObjectStubRpcClient::HasProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    int32_t* success,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "HasProperty:CCC:iC",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      success,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::HasMethod(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    int32_t* success,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "HasMethod:CCC:iC",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      success,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::GetProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* value_bytes, char* value,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "GetProperty:CCC:CC",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      value_bytes, value,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::GetAllPropertyNames(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    int32_t* property_count,
    nacl_abi_size_t* properties_bytes, char* properties,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "GetAllPropertyNames:CC:iCC",
      capability_bytes, capability,
      exception_in_bytes, exception_in,
      property_count,
      properties_bytes, properties,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::SetProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t value_bytes, char* value,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "SetProperty:CCCC:C",
      capability_bytes, capability,
      name_bytes, name,
      value_bytes, value,
      exception_in_bytes, exception_in,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::RemoveProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "RemoveProperty:CCC:C",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::Call(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    int32_t argc,
    nacl_abi_size_t argv_bytes, char* argv,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* ret_bytes, char* ret,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "Call:CCiCC:CC",
      capability_bytes, capability,
      name_bytes, name,
      argc,
      argv_bytes, argv,
      exception_in_bytes, exception_in,
      ret_bytes, ret,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::Construct(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    int32_t argc,
    nacl_abi_size_t argv_bytes, char* argv,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* ret_bytes, char* ret,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "Construct:CiCC:CC",
      capability_bytes, capability,
      argc,
      argv_bytes, argv,
      exception_in_bytes, exception_in,
      ret_bytes, ret,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::Deallocate(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "Deallocate:C:",
      capability_bytes, capability
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

NaClSrpcError PpbAudioDevRpcClient::PPB_Audio_Dev_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource config,
    PP_Resource* out_resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_Dev_Create:ll:l",
      instance,
      config,
      out_resource
  );
  return retval;
}

NaClSrpcError PpbAudioDevRpcClient::PPB_Audio_Dev_IsAudio(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_Dev_IsAudio:l:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioDevRpcClient::PPB_Audio_Dev_GetCurrentConfig(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    PP_Resource* out_resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_Dev_GetCurrentConfig:l:l",
      resource,
      out_resource
  );
  return retval;
}

NaClSrpcError PpbAudioDevRpcClient::PPB_Audio_Dev_StopPlayback(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_Dev_StopPlayback:l:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioDevRpcClient::PPB_Audio_Dev_StartPlayback(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_Dev_StartPlayback:l:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioConfigDevRpcClient::PPB_AudioConfig_Dev_CreateStereo16Bit(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t sample_rate,
    int32_t sample_frame_count,
    PP_Resource* resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_Dev_CreateStereo16Bit:lii:l",
      instance,
      sample_rate,
      sample_frame_count,
      resource
  );
  return retval;
}

NaClSrpcError PpbAudioConfigDevRpcClient::PPB_AudioConfig_Dev_IsAudioConfig(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_Dev_IsAudioConfig:l:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioConfigDevRpcClient::PPB_AudioConfig_Dev_RecommendSampleFrameCount(
    NaClSrpcChannel* channel,
    int32_t request,
    int32_t* sample_frame_count)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_Dev_RecommendSampleFrameCount:i:i",
      request,
      sample_frame_count
  );
  return retval;
}

NaClSrpcError PpbAudioConfigDevRpcClient::PPB_AudioConfig_Dev_GetSampleRate(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* sample_rate)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_Dev_GetSampleRate:l:i",
      resource,
      sample_rate
  );
  return retval;
}

NaClSrpcError PpbAudioConfigDevRpcClient::PPB_AudioConfig_Dev_GetSampleFrameCount(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* sample_frame_count)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_Dev_GetSampleFrameCount:l:i",
      resource,
      sample_frame_count
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_AddRefResource(
    NaClSrpcChannel* channel,
    PP_Resource resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_AddRefResource:l:",
      resource
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_ReleaseResource(
    NaClSrpcChannel* channel,
    PP_Resource resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_ReleaseResource:l:",
      resource
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::ReleaseResourceMultipleTimes(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t count)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "ReleaseResourceMultipleTimes:li:",
      resource,
      count
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_GetTime(
    NaClSrpcChannel* channel,
    double* time)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_GetTime::d",
      time
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t size_bytes, char* size,
    int32_t is_always_opaque,
    PP_Resource* resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Create:lCi:l",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_IsGraphics2D:l:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Describe:l:Cii",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_PaintImageData:llCC:",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Scroll:lCC:",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_ReplaceContents:ll:",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Flush:li:i",
      graphics_2d,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_GetNativeImageDataFormat(
    NaClSrpcChannel* channel,
    int32_t* format)  {
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_Create:liCi:l",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_IsImageData:l:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_Describe:l:Chii",
      resource,
      desc_bytes, desc,
      shm,
      shm_size,
      success
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_GetWindowObject(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t* window_bytes, char* window)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_GetWindowObject:l:C",
      instance,
      window_bytes, window
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_GetOwnerElementObject(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t* owner_bytes, char* owner)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_GetOwnerElementObject:l:C",
      instance,
      owner_bytes, owner
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_BindGraphics(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource graphics_device,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_BindGraphics:ll:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_IsFullFrame:l:i",
      instance,
      is_full_frame
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_ExecuteScript(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t script_bytes, char* script,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* result_bytes, char* result,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_ExecuteScript:lCC:CC",
      instance,
      script_bytes, script,
      exception_in_bytes, exception_in,
      result_bytes, result,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource* resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_Create:l:l",
      instance,
      resource
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_IsURLLoader(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* is_url_loader)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_IsURLLoader:l:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_Open:lli:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_FollowRedirect:li:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_GetUploadProgress:l:lli",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_GetDownloadProgress:l:lli",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_GetResponseInfo:l:l",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_ReadResponseBody:lii:Ci",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_FinishStreamingToFile:li:i",
      loader,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_Close(
    NaClSrpcChannel* channel,
    PP_Resource loader)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_Close:l:",
      loader
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_Create(
    NaClSrpcChannel* channel,
    PP_Module module,
    PP_Resource* resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_Create:l:l",
      module,
      resource
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_IsURLRequestInfo(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_IsURLRequestInfo:l:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_SetProperty:liC:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_AppendDataToBody:lC:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_AppendFileToBody:lllld:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLResponseInfo_IsURLResponseInfo:l:i",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLResponseInfo_GetProperty:li:C",
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
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLResponseInfo_GetBodyAsFileRef:l:l",
      response,
      file_ref
  );
  return retval;
}


