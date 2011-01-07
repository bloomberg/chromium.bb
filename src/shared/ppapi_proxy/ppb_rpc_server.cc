// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "trusted/srpcgen/ppb_rpc.h"
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

namespace {

static void HasPropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  ObjectStubRpcServer::HasProperty(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.ival),
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void HasMethodDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  ObjectStubRpcServer::HasMethod(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.ival),
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void GetPropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  ObjectStubRpcServer::GetProperty(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void GetAllPropertyNamesDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  ObjectStubRpcServer::GetAllPropertyNames(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      &(outputs[0]->u.ival),
      &(outputs[1]->u.count), outputs[1]->arrays.carr,
      &(outputs[2]->u.count), outputs[2]->arrays.carr
  );
}

static void SetPropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  ObjectStubRpcServer::SetProperty(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void RemovePropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  ObjectStubRpcServer::RemoveProperty(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void CallDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  ObjectStubRpcServer::Call(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      inputs[4]->u.count, inputs[4]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void ConstructDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  ObjectStubRpcServer::Construct(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void DeallocateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  ObjectStubRpcServer::Deallocate(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr
  );
}

static void PPB_GetInterfaceDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbRpcServer::PPB_GetInterface(
      rpc,
      done,
      inputs[0]->arrays.str,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Audio_Dev_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioDevRpcServer::PPB_Audio_Dev_Create(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.lval,
      &(outputs[0]->u.lval)
  );
}

static void PPB_Audio_Dev_IsAudioDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioDevRpcServer::PPB_Audio_Dev_IsAudio(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Audio_Dev_GetCurrentConfigDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioDevRpcServer::PPB_Audio_Dev_GetCurrentConfig(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.lval)
  );
}

static void PPB_Audio_Dev_StopPlaybackDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioDevRpcServer::PPB_Audio_Dev_StopPlayback(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Audio_Dev_StartPlaybackDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioDevRpcServer::PPB_Audio_Dev_StartPlayback(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_AudioConfig_Dev_CreateStereo16BitDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioConfigDevRpcServer::PPB_AudioConfig_Dev_CreateStereo16Bit(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.lval)
  );
}

static void PPB_AudioConfig_Dev_IsAudioConfigDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioConfigDevRpcServer::PPB_AudioConfig_Dev_IsAudioConfig(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_AudioConfig_Dev_RecommendSampleFrameCountDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioConfigDevRpcServer::PPB_AudioConfig_Dev_RecommendSampleFrameCount(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_AudioConfig_Dev_GetSampleRateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioConfigDevRpcServer::PPB_AudioConfig_Dev_GetSampleRate(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_AudioConfig_Dev_GetSampleFrameCountDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioConfigDevRpcServer::PPB_AudioConfig_Dev_GetSampleFrameCount(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Core_AddRefResourceDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbCoreRpcServer::PPB_Core_AddRefResource(
      rpc,
      done,
      inputs[0]->u.lval
  );
}

static void PPB_Core_ReleaseResourceDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbCoreRpcServer::PPB_Core_ReleaseResource(
      rpc,
      done,
      inputs[0]->u.lval
  );
}

static void ReleaseResourceMultipleTimesDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbCoreRpcServer::ReleaseResourceMultipleTimes(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.ival
  );
}

static void PPB_Core_GetTimeDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(inputs);
  PpbCoreRpcServer::PPB_Core_GetTime(
      rpc,
      done,
      &(outputs[0]->u.dval)
  );
}

static void PPB_Graphics2D_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics2DRpcServer::PPB_Graphics2D_Create(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      &(outputs[0]->u.lval)
  );
}

static void PPB_Graphics2D_IsGraphics2DDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics2DRpcServer::PPB_Graphics2D_IsGraphics2D(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Graphics2D_DescribeDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics2DRpcServer::PPB_Graphics2D_Describe(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival),
      &(outputs[2]->u.ival)
  );
}

static void PPB_Graphics2D_PaintImageDataDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbGraphics2DRpcServer::PPB_Graphics2D_PaintImageData(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.lval,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.count, inputs[3]->arrays.carr
  );
}

static void PPB_Graphics2D_ScrollDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbGraphics2DRpcServer::PPB_Graphics2D_Scroll(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr
  );
}

static void PPB_Graphics2D_ReplaceContentsDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbGraphics2DRpcServer::PPB_Graphics2D_ReplaceContents(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.lval
  );
}

static void PPB_Graphics2D_FlushDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics2DRpcServer::PPB_Graphics2D_Flush(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_ImageData_GetNativeImageDataFormatDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(inputs);
  PpbImageDataRpcServer::PPB_ImageData_GetNativeImageDataFormat(
      rpc,
      done,
      &(outputs[0]->u.ival)
  );
}

static void PPB_ImageData_IsImageDataFormatSupportedDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbImageDataRpcServer::PPB_ImageData_IsImageDataFormatSupported(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_ImageData_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbImageDataRpcServer::PPB_ImageData_Create(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.ival,
      &(outputs[0]->u.lval)
  );
}

static void PPB_ImageData_IsImageDataDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbImageDataRpcServer::PPB_ImageData_IsImageData(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_ImageData_DescribeDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbImageDataRpcServer::PPB_ImageData_Describe(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.hval),
      &(outputs[2]->u.ival),
      &(outputs[3]->u.ival)
  );
}

static void PPB_Instance_GetWindowObjectDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbInstanceRpcServer::PPB_Instance_GetWindowObject(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPB_Instance_GetOwnerElementObjectDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbInstanceRpcServer::PPB_Instance_GetOwnerElementObject(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPB_Instance_BindGraphicsDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbInstanceRpcServer::PPB_Instance_BindGraphics(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Instance_IsFullFrameDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbInstanceRpcServer::PPB_Instance_IsFullFrame(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Instance_ExecuteScriptDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbInstanceRpcServer::PPB_Instance_ExecuteScript(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void PPB_URLRequestInfo_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_Create(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.lval)
  );
}

static void PPB_URLRequestInfo_IsURLRequestInfoDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_IsURLRequestInfo(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLRequestInfo_SetPropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_SetProperty(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLRequestInfo_AppendDataToBodyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_AppendDataToBody(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLRequestInfo_AppendFileToBodyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLRequestInfoRpcServer::PPB_URLRequestInfo_AppendFileToBody(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.lval,
      inputs[2]->u.lval,
      inputs[3]->u.lval,
      inputs[4]->u.dval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLResponseInfo_IsURLResponseInfoDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLResponseInfoRpcServer::PPB_URLResponseInfo_IsURLResponseInfo(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLResponseInfo_GetPropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLResponseInfoRpcServer::PPB_URLResponseInfo_GetProperty(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPB_URLResponseInfo_GetBodyAsFileRefDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLResponseInfoRpcServer::PPB_URLResponseInfo_GetBodyAsFileRef(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.lval)
  );
}

}  // namespace

NaClSrpcHandlerDesc PpbRpcs::srpc_methods[] = {
  { "HasProperty:CCC:iC", HasPropertyDispatcher },
  { "HasMethod:CCC:iC", HasMethodDispatcher },
  { "GetProperty:CCC:CC", GetPropertyDispatcher },
  { "GetAllPropertyNames:CC:iCC", GetAllPropertyNamesDispatcher },
  { "SetProperty:CCCC:C", SetPropertyDispatcher },
  { "RemoveProperty:CCC:C", RemovePropertyDispatcher },
  { "Call:CCiCC:CC", CallDispatcher },
  { "Construct:CiCC:CC", ConstructDispatcher },
  { "Deallocate:C:", DeallocateDispatcher },
  { "PPB_GetInterface:s:i", PPB_GetInterfaceDispatcher },
  { "PPB_Audio_Dev_Create:ll:l", PPB_Audio_Dev_CreateDispatcher },
  { "PPB_Audio_Dev_IsAudio:l:i", PPB_Audio_Dev_IsAudioDispatcher },
  { "PPB_Audio_Dev_GetCurrentConfig:l:l", PPB_Audio_Dev_GetCurrentConfigDispatcher },
  { "PPB_Audio_Dev_StopPlayback:l:i", PPB_Audio_Dev_StopPlaybackDispatcher },
  { "PPB_Audio_Dev_StartPlayback:l:i", PPB_Audio_Dev_StartPlaybackDispatcher },
  { "PPB_AudioConfig_Dev_CreateStereo16Bit:lii:l", PPB_AudioConfig_Dev_CreateStereo16BitDispatcher },
  { "PPB_AudioConfig_Dev_IsAudioConfig:l:i", PPB_AudioConfig_Dev_IsAudioConfigDispatcher },
  { "PPB_AudioConfig_Dev_RecommendSampleFrameCount:i:i", PPB_AudioConfig_Dev_RecommendSampleFrameCountDispatcher },
  { "PPB_AudioConfig_Dev_GetSampleRate:l:i", PPB_AudioConfig_Dev_GetSampleRateDispatcher },
  { "PPB_AudioConfig_Dev_GetSampleFrameCount:l:i", PPB_AudioConfig_Dev_GetSampleFrameCountDispatcher },
  { "PPB_Core_AddRefResource:l:", PPB_Core_AddRefResourceDispatcher },
  { "PPB_Core_ReleaseResource:l:", PPB_Core_ReleaseResourceDispatcher },
  { "ReleaseResourceMultipleTimes:li:", ReleaseResourceMultipleTimesDispatcher },
  { "PPB_Core_GetTime::d", PPB_Core_GetTimeDispatcher },
  { "PPB_Graphics2D_Create:lCi:l", PPB_Graphics2D_CreateDispatcher },
  { "PPB_Graphics2D_IsGraphics2D:l:i", PPB_Graphics2D_IsGraphics2DDispatcher },
  { "PPB_Graphics2D_Describe:l:Cii", PPB_Graphics2D_DescribeDispatcher },
  { "PPB_Graphics2D_PaintImageData:llCC:", PPB_Graphics2D_PaintImageDataDispatcher },
  { "PPB_Graphics2D_Scroll:lCC:", PPB_Graphics2D_ScrollDispatcher },
  { "PPB_Graphics2D_ReplaceContents:ll:", PPB_Graphics2D_ReplaceContentsDispatcher },
  { "PPB_Graphics2D_Flush:li:i", PPB_Graphics2D_FlushDispatcher },
  { "PPB_ImageData_GetNativeImageDataFormat::i", PPB_ImageData_GetNativeImageDataFormatDispatcher },
  { "PPB_ImageData_IsImageDataFormatSupported:i:i", PPB_ImageData_IsImageDataFormatSupportedDispatcher },
  { "PPB_ImageData_Create:liCi:l", PPB_ImageData_CreateDispatcher },
  { "PPB_ImageData_IsImageData:l:i", PPB_ImageData_IsImageDataDispatcher },
  { "PPB_ImageData_Describe:l:Chii", PPB_ImageData_DescribeDispatcher },
  { "PPB_Instance_GetWindowObject:l:C", PPB_Instance_GetWindowObjectDispatcher },
  { "PPB_Instance_GetOwnerElementObject:l:C", PPB_Instance_GetOwnerElementObjectDispatcher },
  { "PPB_Instance_BindGraphics:ll:i", PPB_Instance_BindGraphicsDispatcher },
  { "PPB_Instance_IsFullFrame:l:i", PPB_Instance_IsFullFrameDispatcher },
  { "PPB_Instance_ExecuteScript:lCC:CC", PPB_Instance_ExecuteScriptDispatcher },
  { "PPB_URLRequestInfo_Create:l:l", PPB_URLRequestInfo_CreateDispatcher },
  { "PPB_URLRequestInfo_IsURLRequestInfo:l:i", PPB_URLRequestInfo_IsURLRequestInfoDispatcher },
  { "PPB_URLRequestInfo_SetProperty:liC:i", PPB_URLRequestInfo_SetPropertyDispatcher },
  { "PPB_URLRequestInfo_AppendDataToBody:lC:i", PPB_URLRequestInfo_AppendDataToBodyDispatcher },
  { "PPB_URLRequestInfo_AppendFileToBody:lllld:i", PPB_URLRequestInfo_AppendFileToBodyDispatcher },
  { "PPB_URLResponseInfo_IsURLResponseInfo:l:i", PPB_URLResponseInfo_IsURLResponseInfoDispatcher },
  { "PPB_URLResponseInfo_GetProperty:li:C", PPB_URLResponseInfo_GetPropertyDispatcher },
  { "PPB_URLResponseInfo_GetBodyAsFileRef:l:l", PPB_URLResponseInfo_GetBodyAsFileRefDispatcher },
  { NULL, NULL }
};

