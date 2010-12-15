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

namespace {

static void HasPropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
  PpbCoreRpcServer::PPB_Core_ReleaseResource(
      rpc,
      done,
      inputs[0]->u.lval
  );
}

static void PPB_Core_GetTimeDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(inputs);
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
  PpbGraphics2DRpcServer::PPB_Graphics2D_Create(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.count, inputs[1]->arrays.iarr,
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
  PpbGraphics2DRpcServer::PPB_Graphics2D_Describe(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.count), outputs[0]->arrays.iarr,
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
  UNREFERENCED_PARAMETER(done);
  PpbGraphics2DRpcServer::PPB_Graphics2D_PaintImageData(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.lval,
      inputs[2]->u.count, inputs[2]->arrays.iarr,
      inputs[3]->u.count, inputs[3]->arrays.iarr
  );
}

static void PPB_Graphics2D_ScrollDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);
  PpbGraphics2DRpcServer::PPB_Graphics2D_Scroll(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.count, inputs[1]->arrays.iarr,
      inputs[2]->u.count, inputs[2]->arrays.iarr
  );
}

static void PPB_Graphics2D_ReplaceContentsDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);
  PpbGraphics2DRpcServer::PPB_Graphics2D_ReplaceContents(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.lval
  );
}

static void PPB_ImageData_GetNativeImageDataFormatDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(inputs);
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
  PpbImageDataRpcServer::PPB_ImageData_Create(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.iarr,
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
  PpbImageDataRpcServer::PPB_ImageData_Describe(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.count), outputs[0]->arrays.iarr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_Instance_GetWindowObjectDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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
  UNREFERENCED_PARAMETER(done);
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

}  // namespace

NACL_SRPC_METHOD_ARRAY(PpbRpcs::srpc_methods) = {
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
  { "PPB_Core_GetTime::d", PPB_Core_GetTimeDispatcher },
  { "PPB_Graphics2D_Create:lIi:l", PPB_Graphics2D_CreateDispatcher },
  { "PPB_Graphics2D_IsGraphics2D:l:i", PPB_Graphics2D_IsGraphics2DDispatcher },
  { "PPB_Graphics2D_Describe:l:Iii", PPB_Graphics2D_DescribeDispatcher },
  { "PPB_Graphics2D_PaintImageData:llII:", PPB_Graphics2D_PaintImageDataDispatcher },
  { "PPB_Graphics2D_Scroll:lII:", PPB_Graphics2D_ScrollDispatcher },
  { "PPB_Graphics2D_ReplaceContents:ll:", PPB_Graphics2D_ReplaceContentsDispatcher },
  { "PPB_ImageData_GetNativeImageDataFormat::i", PPB_ImageData_GetNativeImageDataFormatDispatcher },
  { "PPB_ImageData_IsImageDataFormatSupported:i:i", PPB_ImageData_IsImageDataFormatSupportedDispatcher },
  { "PPB_ImageData_Create:liIi:l", PPB_ImageData_CreateDispatcher },
  { "PPB_ImageData_IsImageData:l:i", PPB_ImageData_IsImageDataDispatcher },
  { "PPB_ImageData_Describe:l:Ii", PPB_ImageData_DescribeDispatcher },
  { "PPB_Instance_GetWindowObject:l:C", PPB_Instance_GetWindowObjectDispatcher },
  { "PPB_Instance_GetOwnerElementObject:l:C", PPB_Instance_GetOwnerElementObjectDispatcher },
  { "PPB_Instance_BindGraphics:ll:i", PPB_Instance_BindGraphicsDispatcher },
  { "PPB_Instance_IsFullFrame:l:i", PPB_Instance_IsFullFrameDispatcher },
  { "PPB_Instance_ExecuteScript:lCC:CC", PPB_Instance_ExecuteScriptDispatcher },
  { NULL, NULL }
};  // NACL_SRPC_METHOD_ARRAY

