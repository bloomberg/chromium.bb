// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

static void StreamAsFileDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  NaClFileRpcServer::StreamAsFile(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->arrays.str,
      inputs[2]->u.ival
  );
}

static void GetFileDescDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  NaClFileRpcServer::GetFileDesc(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->arrays.str,
      &(outputs[0]->u.hval)
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

static void PPB_Audio_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioRpcServer::PPB_Audio_Create(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Audio_IsAudioDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioRpcServer::PPB_Audio_IsAudio(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Audio_GetCurrentConfigDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioRpcServer::PPB_Audio_GetCurrentConfig(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Audio_StopPlaybackDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioRpcServer::PPB_Audio_StopPlayback(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Audio_StartPlaybackDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioRpcServer::PPB_Audio_StartPlayback(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_AudioConfig_CreateStereo16BitDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioConfigRpcServer::PPB_AudioConfig_CreateStereo16Bit(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_AudioConfig_IsAudioConfigDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioConfigRpcServer::PPB_AudioConfig_IsAudioConfig(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_AudioConfig_RecommendSampleFrameCountDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioConfigRpcServer::PPB_AudioConfig_RecommendSampleFrameCount(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_AudioConfig_GetSampleRateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioConfigRpcServer::PPB_AudioConfig_GetSampleRate(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_AudioConfig_GetSampleFrameCountDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbAudioConfigRpcServer::PPB_AudioConfig_GetSampleFrameCount(
      rpc,
      done,
      inputs[0]->u.ival,
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
      inputs[0]->u.ival
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
      inputs[0]->u.ival
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
      inputs[0]->u.ival,
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

static void PPB_Core_GetTimeTicksDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(inputs);
  PpbCoreRpcServer::PPB_Core_GetTimeTicks(
      rpc,
      done,
      &(outputs[0]->u.dval)
  );
}

static void PPB_Core_CallOnMainThreadDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbCoreRpcServer::PPB_Core_CallOnMainThread(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival
  );
}

static void PPB_CursorControl_SetCursorDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbCursorControlRpcServer::PPB_CursorControl_SetCursor(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPB_CursorControl_LockCursorDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbCursorControlRpcServer::PPB_CursorControl_LockCursor(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_CursorControl_UnlockCursorDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbCursorControlRpcServer::PPB_CursorControl_UnlockCursor(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_CursorControl_HasCursorLockDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbCursorControlRpcServer::PPB_CursorControl_HasCursorLock(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_CursorControl_CanLockCursorDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbCursorControlRpcServer::PPB_CursorControl_CanLockCursor(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileIO_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileIORpcServer::PPB_FileIO_Create(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileIO_IsFileIODispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileIORpcServer::PPB_FileIO_IsFileIO(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileIO_OpenDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileIORpcServer::PPB_FileIO_Open(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      inputs[3]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileIO_QueryDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileIORpcServer::PPB_FileIO_Query(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_FileIO_TouchDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileIORpcServer::PPB_FileIO_Touch(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.dval,
      inputs[2]->u.dval,
      inputs[3]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileIO_ReadDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileIORpcServer::PPB_FileIO_Read(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.lval,
      inputs[2]->u.ival,
      inputs[3]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_FileIO_WriteDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileIORpcServer::PPB_FileIO_Write(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.lval,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.ival,
      inputs[4]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileIO_SetLengthDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileIORpcServer::PPB_FileIO_SetLength(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.lval,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileIO_FlushDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileIORpcServer::PPB_FileIO_Flush(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileIO_CloseDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbFileIORpcServer::PPB_FileIO_Close(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_FileRef_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileRefRpcServer::PPB_FileRef_Create(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileRef_IsFileRefDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileRefRpcServer::PPB_FileRef_IsFileRef(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileRef_GetFileSystemTypeDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileRefRpcServer::PPB_FileRef_GetFileSystemType(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileRef_GetNameDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileRefRpcServer::PPB_FileRef_GetName(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPB_FileRef_GetPathDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileRefRpcServer::PPB_FileRef_GetPath(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPB_FileRef_GetParentDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileRefRpcServer::PPB_FileRef_GetParent(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileRef_MakeDirectoryDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileRefRpcServer::PPB_FileRef_MakeDirectory(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileRef_TouchDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileRefRpcServer::PPB_FileRef_Touch(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.dval,
      inputs[2]->u.dval,
      inputs[3]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileRef_DeleteDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileRefRpcServer::PPB_FileRef_Delete(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileRef_RenameDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileRefRpcServer::PPB_FileRef_Rename(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileSystem_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileSystemRpcServer::PPB_FileSystem_Create(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileSystem_IsFileSystemDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileSystemRpcServer::PPB_FileSystem_IsFileSystem(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileSystem_OpenDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileSystemRpcServer::PPB_FileSystem_Open(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.lval,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_FileSystem_GetTypeDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFileSystemRpcServer::PPB_FileSystem_GetType(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Find_NumberOfFindResultsChangedDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbFindRpcServer::PPB_Find_NumberOfFindResultsChanged(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival
  );
}

static void PPB_Find_SelectedFindResultChangedDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbFindRpcServer::PPB_Find_SelectedFindResultChanged(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival
  );
}

static void PPB_Font_GetFontFamiliesDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFontRpcServer::PPB_Font_GetFontFamilies(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPB_Font_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFontRpcServer::PPB_Font_Create(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Font_IsFontDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFontRpcServer::PPB_Font_IsFont(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Font_DescribeDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFontRpcServer::PPB_Font_Describe(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.count), outputs[1]->arrays.carr,
      &(outputs[2]->u.count), outputs[2]->arrays.carr,
      &(outputs[3]->u.ival)
  );
}

static void PPB_Font_DrawTextAtDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFontRpcServer::PPB_Font_DrawTextAt(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      inputs[4]->u.count, inputs[4]->arrays.carr,
      inputs[5]->u.ival,
      inputs[6]->u.count, inputs[6]->arrays.carr,
      inputs[7]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Font_MeasureTextDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFontRpcServer::PPB_Font_MeasureText(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Font_CharacterOffsetForPixelDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFontRpcServer::PPB_Font_CharacterOffsetForPixel(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Font_PixelOffsetForCharacterDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFontRpcServer::PPB_Font_PixelOffsetForCharacter(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Fullscreen_SetFullscreenDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFullscreenRpcServer::PPB_Fullscreen_SetFullscreen(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Fullscreen_GetScreenSizeDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbFullscreenRpcServer::PPB_Fullscreen_GetScreenSize(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
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
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
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
      inputs[0]->u.ival,
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
      inputs[0]->u.ival,
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
      inputs[0]->u.ival,
      inputs[1]->u.ival,
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
      inputs[0]->u.ival,
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
      inputs[0]->u.ival,
      inputs[1]->u.ival
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
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Graphics3D_GetAttribMaxValueDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3D_GetAttribMaxValue(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival),
      &(outputs[1]->u.ival)
  );
}

static void PPB_Graphics3D_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3D_Create(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.iarr,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Graphics3D_GetAttribsDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3D_GetAttribs(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.iarr,
      &(outputs[0]->u.count), outputs[0]->arrays.iarr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_Graphics3D_SetAttribsDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3D_SetAttribs(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.iarr,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Graphics3D_GetErrorDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3D_GetError(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Graphics3D_SwapBuffersDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3D_SwapBuffers(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Graphics3DTrusted_CreateRawDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_CreateRaw(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.iarr,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Graphics3DTrusted_InitCommandBufferDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_InitCommandBuffer(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Graphics3DTrusted_GetRingBufferDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_GetRingBuffer(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.hval),
      &(outputs[1]->u.ival)
  );
}

static void PPB_Graphics3DTrusted_GetStateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_GetState(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPB_Graphics3DTrusted_FlushDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_Flush(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival
  );
}

static void PPB_Graphics3DTrusted_FlushSyncDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_FlushSync(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPB_Graphics3DTrusted_FlushSyncFastDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_FlushSyncFast(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPB_Graphics3DTrusted_CreateTransferBufferDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_CreateTransferBuffer(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Graphics3DTrusted_DestroyTransferBufferDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_DestroyTransferBuffer(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival
  );
}

static void PPB_Graphics3DTrusted_GetTransferBufferDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbGraphics3DRpcServer::PPB_Graphics3DTrusted_GetTransferBuffer(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.hval),
      &(outputs[1]->u.ival)
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
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.ival,
      &(outputs[0]->u.ival)
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
      inputs[0]->u.ival,
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
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.hval),
      &(outputs[2]->u.ival),
      &(outputs[3]->u.ival)
  );
}

static void PPB_InputEvent_RequestInputEventsDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbInputEventRpcServer::PPB_InputEvent_RequestInputEvents(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_InputEvent_ClearInputEventRequestDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbInputEventRpcServer::PPB_InputEvent_ClearInputEventRequest(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival
  );
}

static void PPB_InputEvent_CreateMouseInputEventDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbInputEventRpcServer::PPB_InputEvent_CreateMouseInputEvent(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.dval,
      inputs[3]->u.ival,
      inputs[4]->u.ival,
      inputs[5]->u.ival,
      inputs[6]->u.ival,
      inputs[7]->u.ival,
      inputs[8]->u.ival,
      inputs[9]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_InputEvent_CreateWheelInputEventDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbInputEventRpcServer::PPB_InputEvent_CreateWheelInputEvent(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.dval,
      inputs[2]->u.ival,
      inputs[3]->u.dval,
      inputs[4]->u.dval,
      inputs[5]->u.dval,
      inputs[6]->u.dval,
      inputs[7]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_InputEvent_CreateKeyboardInputEventDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbInputEventRpcServer::PPB_InputEvent_CreateKeyboardInputEvent(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.dval,
      inputs[3]->u.ival,
      inputs[4]->u.ival,
      inputs[5]->u.count, inputs[5]->arrays.carr,
      &(outputs[0]->u.ival)
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
      inputs[0]->u.ival,
      inputs[1]->u.ival,
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
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Messaging_PostMessageDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbMessagingRpcServer::PPB_Messaging_PostMessage(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr
  );
}

static void PPB_MouseLock_LockMouseDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbMouseLockRpcServer::PPB_MouseLock_LockMouse(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_MouseLock_UnlockMouseDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbMouseLockRpcServer::PPB_MouseLock_UnlockMouse(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_PDF_GetLocalizedStringDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbPdfRpcServer::PPB_PDF_GetLocalizedString(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void PPB_PDF_GetResourceImageDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbPdfRpcServer::PPB_PDF_GetResourceImage(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_PDF_GetFontFileWithFallbackDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbPdfRpcServer::PPB_PDF_GetFontFileWithFallback(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_PDF_GetFontTableForPrivateFontFileDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbPdfRpcServer::PPB_PDF_GetFontTableForPrivateFontFile(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_PDF_SearchStringDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbPdfRpcServer::PPB_PDF_SearchString(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_PDF_DidStartLoadingDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbPdfRpcServer::PPB_PDF_DidStartLoading(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_PDF_DidStopLoadingDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbPdfRpcServer::PPB_PDF_DidStopLoading(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_PDF_SetContentRestrictionDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbPdfRpcServer::PPB_PDF_SetContentRestriction(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival
  );
}

static void PPB_PDF_HistogramPDFPageCountDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbPdfRpcServer::PPB_PDF_HistogramPDFPageCount(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_PDF_UserMetricsRecordActionDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbPdfRpcServer::PPB_PDF_UserMetricsRecordAction(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr
  );
}

static void PPB_PDF_HasUnsupportedFeatureDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbPdfRpcServer::PPB_PDF_HasUnsupportedFeature(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_PDF_SaveAsDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbPdfRpcServer::PPB_PDF_SaveAs(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_Scrollbar_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbScrollbarRpcServer::PPB_Scrollbar_Create(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Scrollbar_IsScrollbarDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbScrollbarRpcServer::PPB_Scrollbar_IsScrollbar(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Scrollbar_IsOverlayDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbScrollbarRpcServer::PPB_Scrollbar_IsOverlay(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Scrollbar_GetThicknessDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbScrollbarRpcServer::PPB_Scrollbar_GetThickness(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Scrollbar_GetValueDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbScrollbarRpcServer::PPB_Scrollbar_GetValue(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Scrollbar_SetValueDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbScrollbarRpcServer::PPB_Scrollbar_SetValue(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival
  );
}

static void PPB_Scrollbar_SetDocumentSizeDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbScrollbarRpcServer::PPB_Scrollbar_SetDocumentSize(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival
  );
}

static void PPB_Scrollbar_SetTickMarksDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbScrollbarRpcServer::PPB_Scrollbar_SetTickMarks(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival
  );
}

static void PPB_Scrollbar_ScrollByDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbScrollbarRpcServer::PPB_Scrollbar_ScrollBy(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival
  );
}

static void PPB_TCPSocket_Private_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_Create(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_TCPSocket_Private_IsTCPSocketDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_IsTCPSocket(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_TCPSocket_Private_ConnectDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_Connect(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->arrays.str,
      inputs[2]->u.ival,
      inputs[3]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_TCPSocket_Private_ConnectWithNetAddressDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_ConnectWithNetAddress(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_TCPSocket_Private_GetLocalAddressDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_GetLocalAddress(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_TCPSocket_Private_GetRemoteAddressDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_GetRemoteAddress(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_TCPSocket_Private_SSLHandshakeDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_SSLHandshake(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->arrays.str,
      inputs[2]->u.ival,
      inputs[3]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_TCPSocket_Private_ReadDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_Read(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_TCPSocket_Private_WriteDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_Write(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      inputs[3]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_TCPSocket_Private_DisconnectDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbTCPSocketPrivateRpcServer::PPB_TCPSocket_Private_Disconnect(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_Testing_ReadImageDataDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTestingRpcServer::PPB_Testing_ReadImageData(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Testing_RunMessageLoopDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbTestingRpcServer::PPB_Testing_RunMessageLoop(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_Testing_QuitMessageLoopDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbTestingRpcServer::PPB_Testing_QuitMessageLoop(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_Testing_GetLiveObjectsForInstanceDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbTestingRpcServer::PPB_Testing_GetLiveObjectsForInstance(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_UDPSocket_Private_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_Create(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_UDPSocket_Private_IsUDPSocketDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_IsUDPSocket(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_UDPSocket_Private_BindDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_Bind(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_UDPSocket_Private_RecvFromDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_RecvFrom(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_UDPSocket_Private_GetRecvFromAddressDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_GetRecvFromAddress(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_UDPSocket_Private_SendToDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_SendTo(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      inputs[4]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_UDPSocket_Private_CloseDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbUDPSocketPrivateRpcServer::PPB_UDPSocket_Private_Close(
      rpc,
      done,
      inputs[0]->u.ival
  );
}

static void PPB_URLLoader_CreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLLoaderRpcServer::PPB_URLLoader_Create(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLLoader_IsURLLoaderDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLLoaderRpcServer::PPB_URLLoader_IsURLLoader(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLLoader_OpenDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLLoaderRpcServer::PPB_URLLoader_Open(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLLoader_FollowRedirectDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLLoaderRpcServer::PPB_URLLoader_FollowRedirect(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLLoader_GetUploadProgressDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLLoaderRpcServer::PPB_URLLoader_GetUploadProgress(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.lval),
      &(outputs[1]->u.lval),
      &(outputs[2]->u.ival)
  );
}

static void PPB_URLLoader_GetDownloadProgressDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLLoaderRpcServer::PPB_URLLoader_GetDownloadProgress(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.lval),
      &(outputs[1]->u.lval),
      &(outputs[2]->u.ival)
  );
}

static void PPB_URLLoader_GetResponseInfoDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLLoaderRpcServer::PPB_URLLoader_GetResponseInfo(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLLoader_ReadResponseBodyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLLoaderRpcServer::PPB_URLLoader_ReadResponseBody(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      inputs[2]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_URLLoader_FinishStreamingToFileDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbURLLoaderRpcServer::PPB_URLLoader_FinishStreamingToFile(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_URLLoader_CloseDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbURLLoaderRpcServer::PPB_URLLoader_Close(
      rpc,
      done,
      inputs[0]->u.ival
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
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
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
      inputs[0]->u.ival,
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
      inputs[0]->u.ival,
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
      inputs[0]->u.ival,
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
      inputs[0]->u.ival,
      inputs[1]->u.ival,
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
      inputs[0]->u.ival,
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
      inputs[0]->u.ival,
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
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Widget_IsWidgetDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbWidgetRpcServer::PPB_Widget_IsWidget(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Widget_PaintDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbWidgetRpcServer::PPB_Widget_Paint(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Widget_HandleEventDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbWidgetRpcServer::PPB_Widget_HandleEvent(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival,
      &(outputs[0]->u.ival)
  );
}

static void PPB_Widget_GetLocationDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  PpbWidgetRpcServer::PPB_Widget_GetLocation(
      rpc,
      done,
      inputs[0]->u.ival,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.ival)
  );
}

static void PPB_Widget_SetLocationDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbWidgetRpcServer::PPB_Widget_SetLocation(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.count, inputs[1]->arrays.carr
  );
}

static void PPB_Zoom_ZoomChangedDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbZoomRpcServer::PPB_Zoom_ZoomChanged(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.dval
  );
}

static void PPB_Zoom_ZoomLimitsChangedDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  PpbZoomRpcServer::PPB_Zoom_ZoomLimitsChanged(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.dval,
      inputs[2]->u.dval
  );
}

}  // namespace

NaClSrpcHandlerDesc PpbRpcs::srpc_methods[] = {
  { "StreamAsFile:isi:", StreamAsFileDispatcher },
  { "GetFileDesc:is:h", GetFileDescDispatcher },
  { "PPB_GetInterface:s:i", PPB_GetInterfaceDispatcher },
  { "PPB_Audio_Create:ii:i", PPB_Audio_CreateDispatcher },
  { "PPB_Audio_IsAudio:i:i", PPB_Audio_IsAudioDispatcher },
  { "PPB_Audio_GetCurrentConfig:i:i", PPB_Audio_GetCurrentConfigDispatcher },
  { "PPB_Audio_StopPlayback:i:i", PPB_Audio_StopPlaybackDispatcher },
  { "PPB_Audio_StartPlayback:i:i", PPB_Audio_StartPlaybackDispatcher },
  { "PPB_AudioConfig_CreateStereo16Bit:iii:i", PPB_AudioConfig_CreateStereo16BitDispatcher },
  { "PPB_AudioConfig_IsAudioConfig:i:i", PPB_AudioConfig_IsAudioConfigDispatcher },
  { "PPB_AudioConfig_RecommendSampleFrameCount:ii:i", PPB_AudioConfig_RecommendSampleFrameCountDispatcher },
  { "PPB_AudioConfig_GetSampleRate:i:i", PPB_AudioConfig_GetSampleRateDispatcher },
  { "PPB_AudioConfig_GetSampleFrameCount:i:i", PPB_AudioConfig_GetSampleFrameCountDispatcher },
  { "PPB_Core_AddRefResource:i:", PPB_Core_AddRefResourceDispatcher },
  { "PPB_Core_ReleaseResource:i:", PPB_Core_ReleaseResourceDispatcher },
  { "ReleaseResourceMultipleTimes:ii:", ReleaseResourceMultipleTimesDispatcher },
  { "PPB_Core_GetTime::d", PPB_Core_GetTimeDispatcher },
  { "PPB_Core_GetTimeTicks::d", PPB_Core_GetTimeTicksDispatcher },
  { "PPB_Core_CallOnMainThread:iii:", PPB_Core_CallOnMainThreadDispatcher },
  { "PPB_CursorControl_SetCursor:iiiC:i", PPB_CursorControl_SetCursorDispatcher },
  { "PPB_CursorControl_LockCursor:i:i", PPB_CursorControl_LockCursorDispatcher },
  { "PPB_CursorControl_UnlockCursor:i:i", PPB_CursorControl_UnlockCursorDispatcher },
  { "PPB_CursorControl_HasCursorLock:i:i", PPB_CursorControl_HasCursorLockDispatcher },
  { "PPB_CursorControl_CanLockCursor:i:i", PPB_CursorControl_CanLockCursorDispatcher },
  { "PPB_FileIO_Create:i:i", PPB_FileIO_CreateDispatcher },
  { "PPB_FileIO_IsFileIO:i:i", PPB_FileIO_IsFileIODispatcher },
  { "PPB_FileIO_Open:iiii:i", PPB_FileIO_OpenDispatcher },
  { "PPB_FileIO_Query:iii:Ci", PPB_FileIO_QueryDispatcher },
  { "PPB_FileIO_Touch:iddi:i", PPB_FileIO_TouchDispatcher },
  { "PPB_FileIO_Read:ilii:Ci", PPB_FileIO_ReadDispatcher },
  { "PPB_FileIO_Write:ilCii:i", PPB_FileIO_WriteDispatcher },
  { "PPB_FileIO_SetLength:ili:i", PPB_FileIO_SetLengthDispatcher },
  { "PPB_FileIO_Flush:ii:i", PPB_FileIO_FlushDispatcher },
  { "PPB_FileIO_Close:i:", PPB_FileIO_CloseDispatcher },
  { "PPB_FileRef_Create:iC:i", PPB_FileRef_CreateDispatcher },
  { "PPB_FileRef_IsFileRef:i:i", PPB_FileRef_IsFileRefDispatcher },
  { "PPB_FileRef_GetFileSystemType:i:i", PPB_FileRef_GetFileSystemTypeDispatcher },
  { "PPB_FileRef_GetName:i:C", PPB_FileRef_GetNameDispatcher },
  { "PPB_FileRef_GetPath:i:C", PPB_FileRef_GetPathDispatcher },
  { "PPB_FileRef_GetParent:i:i", PPB_FileRef_GetParentDispatcher },
  { "PPB_FileRef_MakeDirectory:iii:i", PPB_FileRef_MakeDirectoryDispatcher },
  { "PPB_FileRef_Touch:iddi:i", PPB_FileRef_TouchDispatcher },
  { "PPB_FileRef_Delete:ii:i", PPB_FileRef_DeleteDispatcher },
  { "PPB_FileRef_Rename:iii:i", PPB_FileRef_RenameDispatcher },
  { "PPB_FileSystem_Create:ii:i", PPB_FileSystem_CreateDispatcher },
  { "PPB_FileSystem_IsFileSystem:i:i", PPB_FileSystem_IsFileSystemDispatcher },
  { "PPB_FileSystem_Open:ili:i", PPB_FileSystem_OpenDispatcher },
  { "PPB_FileSystem_GetType:i:i", PPB_FileSystem_GetTypeDispatcher },
  { "PPB_Find_NumberOfFindResultsChanged:iii:", PPB_Find_NumberOfFindResultsChangedDispatcher },
  { "PPB_Find_SelectedFindResultChanged:ii:", PPB_Find_SelectedFindResultChangedDispatcher },
  { "PPB_Font_GetFontFamilies:i:C", PPB_Font_GetFontFamiliesDispatcher },
  { "PPB_Font_Create:iCC:i", PPB_Font_CreateDispatcher },
  { "PPB_Font_IsFont:i:i", PPB_Font_IsFontDispatcher },
  { "PPB_Font_Describe:i:CCCi", PPB_Font_DescribeDispatcher },
  { "PPB_Font_DrawTextAt:iiCCCiCi:i", PPB_Font_DrawTextAtDispatcher },
  { "PPB_Font_MeasureText:iCC:i", PPB_Font_MeasureTextDispatcher },
  { "PPB_Font_CharacterOffsetForPixel:iCCi:i", PPB_Font_CharacterOffsetForPixelDispatcher },
  { "PPB_Font_PixelOffsetForCharacter:iCCi:i", PPB_Font_PixelOffsetForCharacterDispatcher },
  { "PPB_Fullscreen_SetFullscreen:ii:i", PPB_Fullscreen_SetFullscreenDispatcher },
  { "PPB_Fullscreen_GetScreenSize:i:Ci", PPB_Fullscreen_GetScreenSizeDispatcher },
  { "PPB_Graphics2D_Create:iCi:i", PPB_Graphics2D_CreateDispatcher },
  { "PPB_Graphics2D_IsGraphics2D:i:i", PPB_Graphics2D_IsGraphics2DDispatcher },
  { "PPB_Graphics2D_Describe:i:Cii", PPB_Graphics2D_DescribeDispatcher },
  { "PPB_Graphics2D_PaintImageData:iiCC:", PPB_Graphics2D_PaintImageDataDispatcher },
  { "PPB_Graphics2D_Scroll:iCC:", PPB_Graphics2D_ScrollDispatcher },
  { "PPB_Graphics2D_ReplaceContents:ii:", PPB_Graphics2D_ReplaceContentsDispatcher },
  { "PPB_Graphics2D_Flush:ii:i", PPB_Graphics2D_FlushDispatcher },
  { "PPB_Graphics3D_GetAttribMaxValue:ii:ii", PPB_Graphics3D_GetAttribMaxValueDispatcher },
  { "PPB_Graphics3D_Create:iiI:i", PPB_Graphics3D_CreateDispatcher },
  { "PPB_Graphics3D_GetAttribs:iI:Ii", PPB_Graphics3D_GetAttribsDispatcher },
  { "PPB_Graphics3D_SetAttribs:iI:i", PPB_Graphics3D_SetAttribsDispatcher },
  { "PPB_Graphics3D_GetError:i:i", PPB_Graphics3D_GetErrorDispatcher },
  { "PPB_Graphics3D_SwapBuffers:ii:i", PPB_Graphics3D_SwapBuffersDispatcher },
  { "PPB_Graphics3DTrusted_CreateRaw:iiI:i", PPB_Graphics3DTrusted_CreateRawDispatcher },
  { "PPB_Graphics3DTrusted_InitCommandBuffer:ii:i", PPB_Graphics3DTrusted_InitCommandBufferDispatcher },
  { "PPB_Graphics3DTrusted_GetRingBuffer:i:hi", PPB_Graphics3DTrusted_GetRingBufferDispatcher },
  { "PPB_Graphics3DTrusted_GetState:i:C", PPB_Graphics3DTrusted_GetStateDispatcher },
  { "PPB_Graphics3DTrusted_Flush:ii:", PPB_Graphics3DTrusted_FlushDispatcher },
  { "PPB_Graphics3DTrusted_FlushSync:ii:C", PPB_Graphics3DTrusted_FlushSyncDispatcher },
  { "PPB_Graphics3DTrusted_FlushSyncFast:iii:C", PPB_Graphics3DTrusted_FlushSyncFastDispatcher },
  { "PPB_Graphics3DTrusted_CreateTransferBuffer:iii:i", PPB_Graphics3DTrusted_CreateTransferBufferDispatcher },
  { "PPB_Graphics3DTrusted_DestroyTransferBuffer:ii:", PPB_Graphics3DTrusted_DestroyTransferBufferDispatcher },
  { "PPB_Graphics3DTrusted_GetTransferBuffer:ii:hi", PPB_Graphics3DTrusted_GetTransferBufferDispatcher },
  { "PPB_ImageData_GetNativeImageDataFormat::i", PPB_ImageData_GetNativeImageDataFormatDispatcher },
  { "PPB_ImageData_IsImageDataFormatSupported:i:i", PPB_ImageData_IsImageDataFormatSupportedDispatcher },
  { "PPB_ImageData_Create:iiCi:i", PPB_ImageData_CreateDispatcher },
  { "PPB_ImageData_IsImageData:i:i", PPB_ImageData_IsImageDataDispatcher },
  { "PPB_ImageData_Describe:i:Chii", PPB_ImageData_DescribeDispatcher },
  { "PPB_InputEvent_RequestInputEvents:iii:i", PPB_InputEvent_RequestInputEventsDispatcher },
  { "PPB_InputEvent_ClearInputEventRequest:ii:", PPB_InputEvent_ClearInputEventRequestDispatcher },
  { "PPB_InputEvent_CreateMouseInputEvent:iidiiiiiii:i", PPB_InputEvent_CreateMouseInputEventDispatcher },
  { "PPB_InputEvent_CreateWheelInputEvent:ididdddi:i", PPB_InputEvent_CreateWheelInputEventDispatcher },
  { "PPB_InputEvent_CreateKeyboardInputEvent:iidiiC:i", PPB_InputEvent_CreateKeyboardInputEventDispatcher },
  { "PPB_Instance_BindGraphics:ii:i", PPB_Instance_BindGraphicsDispatcher },
  { "PPB_Instance_IsFullFrame:i:i", PPB_Instance_IsFullFrameDispatcher },
  { "PPB_Messaging_PostMessage:iC:", PPB_Messaging_PostMessageDispatcher },
  { "PPB_MouseLock_LockMouse:ii:i", PPB_MouseLock_LockMouseDispatcher },
  { "PPB_MouseLock_UnlockMouse:i:", PPB_MouseLock_UnlockMouseDispatcher },
  { "PPB_PDF_GetLocalizedString:ii:C", PPB_PDF_GetLocalizedStringDispatcher },
  { "PPB_PDF_GetResourceImage:ii:i", PPB_PDF_GetResourceImageDispatcher },
  { "PPB_PDF_GetFontFileWithFallback:iCCi:i", PPB_PDF_GetFontFileWithFallbackDispatcher },
  { "PPB_PDF_GetFontTableForPrivateFontFile:ii:Ci", PPB_PDF_GetFontTableForPrivateFontFileDispatcher },
  { "PPB_PDF_SearchString:iCCi:Ci", PPB_PDF_SearchStringDispatcher },
  { "PPB_PDF_DidStartLoading:i:", PPB_PDF_DidStartLoadingDispatcher },
  { "PPB_PDF_DidStopLoading:i:", PPB_PDF_DidStopLoadingDispatcher },
  { "PPB_PDF_SetContentRestriction:ii:", PPB_PDF_SetContentRestrictionDispatcher },
  { "PPB_PDF_HistogramPDFPageCount:i:", PPB_PDF_HistogramPDFPageCountDispatcher },
  { "PPB_PDF_UserMetricsRecordAction:C:", PPB_PDF_UserMetricsRecordActionDispatcher },
  { "PPB_PDF_HasUnsupportedFeature:i:", PPB_PDF_HasUnsupportedFeatureDispatcher },
  { "PPB_PDF_SaveAs:i:", PPB_PDF_SaveAsDispatcher },
  { "PPB_Scrollbar_Create:ii:i", PPB_Scrollbar_CreateDispatcher },
  { "PPB_Scrollbar_IsScrollbar:i:i", PPB_Scrollbar_IsScrollbarDispatcher },
  { "PPB_Scrollbar_IsOverlay:i:i", PPB_Scrollbar_IsOverlayDispatcher },
  { "PPB_Scrollbar_GetThickness:i:i", PPB_Scrollbar_GetThicknessDispatcher },
  { "PPB_Scrollbar_GetValue:i:i", PPB_Scrollbar_GetValueDispatcher },
  { "PPB_Scrollbar_SetValue:ii:", PPB_Scrollbar_SetValueDispatcher },
  { "PPB_Scrollbar_SetDocumentSize:ii:", PPB_Scrollbar_SetDocumentSizeDispatcher },
  { "PPB_Scrollbar_SetTickMarks:iCi:", PPB_Scrollbar_SetTickMarksDispatcher },
  { "PPB_Scrollbar_ScrollBy:iii:", PPB_Scrollbar_ScrollByDispatcher },
  { "PPB_TCPSocket_Private_Create:i:i", PPB_TCPSocket_Private_CreateDispatcher },
  { "PPB_TCPSocket_Private_IsTCPSocket:i:i", PPB_TCPSocket_Private_IsTCPSocketDispatcher },
  { "PPB_TCPSocket_Private_Connect:isii:i", PPB_TCPSocket_Private_ConnectDispatcher },
  { "PPB_TCPSocket_Private_ConnectWithNetAddress:iCi:i", PPB_TCPSocket_Private_ConnectWithNetAddressDispatcher },
  { "PPB_TCPSocket_Private_GetLocalAddress:i:Ci", PPB_TCPSocket_Private_GetLocalAddressDispatcher },
  { "PPB_TCPSocket_Private_GetRemoteAddress:i:Ci", PPB_TCPSocket_Private_GetRemoteAddressDispatcher },
  { "PPB_TCPSocket_Private_SSLHandshake:isii:i", PPB_TCPSocket_Private_SSLHandshakeDispatcher },
  { "PPB_TCPSocket_Private_Read:iii:Ci", PPB_TCPSocket_Private_ReadDispatcher },
  { "PPB_TCPSocket_Private_Write:iCii:i", PPB_TCPSocket_Private_WriteDispatcher },
  { "PPB_TCPSocket_Private_Disconnect:i:", PPB_TCPSocket_Private_DisconnectDispatcher },
  { "PPB_Testing_ReadImageData:iiC:i", PPB_Testing_ReadImageDataDispatcher },
  { "PPB_Testing_RunMessageLoop:i:", PPB_Testing_RunMessageLoopDispatcher },
  { "PPB_Testing_QuitMessageLoop:i:", PPB_Testing_QuitMessageLoopDispatcher },
  { "PPB_Testing_GetLiveObjectsForInstance:i:i", PPB_Testing_GetLiveObjectsForInstanceDispatcher },
  { "PPB_UDPSocket_Private_Create:i:i", PPB_UDPSocket_Private_CreateDispatcher },
  { "PPB_UDPSocket_Private_IsUDPSocket:i:i", PPB_UDPSocket_Private_IsUDPSocketDispatcher },
  { "PPB_UDPSocket_Private_Bind:iCi:i", PPB_UDPSocket_Private_BindDispatcher },
  { "PPB_UDPSocket_Private_RecvFrom:iii:Ci", PPB_UDPSocket_Private_RecvFromDispatcher },
  { "PPB_UDPSocket_Private_GetRecvFromAddress:i:Ci", PPB_UDPSocket_Private_GetRecvFromAddressDispatcher },
  { "PPB_UDPSocket_Private_SendTo:iCiCi:i", PPB_UDPSocket_Private_SendToDispatcher },
  { "PPB_UDPSocket_Private_Close:i:", PPB_UDPSocket_Private_CloseDispatcher },
  { "PPB_URLLoader_Create:i:i", PPB_URLLoader_CreateDispatcher },
  { "PPB_URLLoader_IsURLLoader:i:i", PPB_URLLoader_IsURLLoaderDispatcher },
  { "PPB_URLLoader_Open:iii:i", PPB_URLLoader_OpenDispatcher },
  { "PPB_URLLoader_FollowRedirect:ii:i", PPB_URLLoader_FollowRedirectDispatcher },
  { "PPB_URLLoader_GetUploadProgress:i:lli", PPB_URLLoader_GetUploadProgressDispatcher },
  { "PPB_URLLoader_GetDownloadProgress:i:lli", PPB_URLLoader_GetDownloadProgressDispatcher },
  { "PPB_URLLoader_GetResponseInfo:i:i", PPB_URLLoader_GetResponseInfoDispatcher },
  { "PPB_URLLoader_ReadResponseBody:iii:Ci", PPB_URLLoader_ReadResponseBodyDispatcher },
  { "PPB_URLLoader_FinishStreamingToFile:ii:i", PPB_URLLoader_FinishStreamingToFileDispatcher },
  { "PPB_URLLoader_Close:i:", PPB_URLLoader_CloseDispatcher },
  { "PPB_URLRequestInfo_Create:i:i", PPB_URLRequestInfo_CreateDispatcher },
  { "PPB_URLRequestInfo_IsURLRequestInfo:i:i", PPB_URLRequestInfo_IsURLRequestInfoDispatcher },
  { "PPB_URLRequestInfo_SetProperty:iiC:i", PPB_URLRequestInfo_SetPropertyDispatcher },
  { "PPB_URLRequestInfo_AppendDataToBody:iC:i", PPB_URLRequestInfo_AppendDataToBodyDispatcher },
  { "PPB_URLRequestInfo_AppendFileToBody:iilld:i", PPB_URLRequestInfo_AppendFileToBodyDispatcher },
  { "PPB_URLResponseInfo_IsURLResponseInfo:i:i", PPB_URLResponseInfo_IsURLResponseInfoDispatcher },
  { "PPB_URLResponseInfo_GetProperty:ii:C", PPB_URLResponseInfo_GetPropertyDispatcher },
  { "PPB_URLResponseInfo_GetBodyAsFileRef:i:i", PPB_URLResponseInfo_GetBodyAsFileRefDispatcher },
  { "PPB_Widget_IsWidget:i:i", PPB_Widget_IsWidgetDispatcher },
  { "PPB_Widget_Paint:iCi:i", PPB_Widget_PaintDispatcher },
  { "PPB_Widget_HandleEvent:ii:i", PPB_Widget_HandleEventDispatcher },
  { "PPB_Widget_GetLocation:i:Ci", PPB_Widget_GetLocationDispatcher },
  { "PPB_Widget_SetLocation:iC:", PPB_Widget_SetLocationDispatcher },
  { "PPB_Zoom_ZoomChanged:id:", PPB_Zoom_ZoomChangedDispatcher },
  { "PPB_Zoom_ZoomLimitsChanged:idd:", PPB_Zoom_ZoomLimitsChangedDispatcher },
  { NULL, NULL }
};

