// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Note: this file defines hooks for all pepper related srpc calls
// it would be nice to keep this synchronized with
// src/shared/ppapi_proxy/ppb_rpc_server.cc
// which is a generated file
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/sel_universal/pepper_handler.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"

#include <stdio.h>
#include <string>

using nacl::DescWrapperFactory;
using nacl::DescWrapper;


namespace {

#define SRPC_STD_ARGS NaClSrpcRpc* rpc, \
                      NaClSrpcArg** inputs, \
                      NaClSrpcArg** outputs, \
                      NaClSrpcClosure* done


void Unimplemented(SRPC_STD_ARGS) {
  UNREFERENCED_PARAMETER(inputs);
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);

  const char* rpc_name;
  const char* arg_types;
  const char* ret_types;

  if (NaClSrpcServiceMethodNameAndTypes(rpc->channel->server,
                                        rpc->rpc_number,
                                        &rpc_name,
                                        &arg_types,
                                        &ret_types)) {
    NaClLog(LOG_ERROR, "cannot find signature for rpc %d\n", rpc->rpc_number);
  }

  // TODO(robertm): add full argument printing
  printf("invoking: %s (%s) -> %s\n", rpc_name, arg_types, ret_types);
}


NaClSrpcHandlerDesc srpc_methods[] = {
  {"Dummy::", &Unimplemented},
  { NULL, NULL }
};


void WINAPI PepperHandlerThread(void* desc_void) {
  DescWrapper* desc = reinterpret_cast<DescWrapper*>(desc_void);

  NaClLog(1, "pepper secondary service started %p\n", desc_void);
  NaClSrpcServerLoop(desc->desc(), srpc_methods, NULL);
  NaClLog(1, "pepper secondary service stopped\n");
  NaClThreadExit();
}

}  // end namespace


bool HandlerAddPepperRpcs(NaClCommandLoop* l, const vector<string>& args) {
  if (args.size() != 1) {
    NaClLog(LOG_ERROR, "not the right number of args for this command\n");
    return false;
  }

  NaClLog(1, "adding dummy pepper rpcs upcalls\n");
  l->AddUpcallRpc("HasProperty:CCC:iC", &Unimplemented);
  l->AddUpcallRpc("HasMethod:CCC:iC", &Unimplemented);
  l->AddUpcallRpc("GetProperty:CCC:CC", &Unimplemented);
  l->AddUpcallRpc("GetAllPropertyNames:CC:iCC", &Unimplemented);
  l->AddUpcallRpc("SetProperty:CCCC:C", &Unimplemented);
  l->AddUpcallRpc("RemoveProperty:CCC:C", &Unimplemented);
  l->AddUpcallRpc("Call:CCiCC:CC", &Unimplemented);
  l->AddUpcallRpc("Construct:CiCC:CC", &Unimplemented);
  l->AddUpcallRpc("Deallocate:C:", &Unimplemented);
  l->AddUpcallRpc("PPB_GetInterface:s:i", &Unimplemented);
  l->AddUpcallRpc("PPB_Audio_Dev_Create:ll:l", &Unimplemented);
  l->AddUpcallRpc("PPB_Audio_Dev_IsAudio:l:i", &Unimplemented);
  l->AddUpcallRpc("PPB_Audio_Dev_GetCurrentConfig:l:l", &Unimplemented);
  l->AddUpcallRpc("PPB_Audio_Dev_StopPlayback:l:i", &Unimplemented);
  l->AddUpcallRpc("PPB_Audio_Dev_StartPlayback:l:i", &Unimplemented);
  l->AddUpcallRpc("PPB_AudioConfig_Dev_CreateStereo16Bit:lii:l",
                      &Unimplemented);
  l->AddUpcallRpc("PPB_AudioConfig_Dev_IsAudioConfig:l:i", &Unimplemented);
  l->AddUpcallRpc("PPB_AudioConfig_Dev_RecommendSampleFrameCount:i:i",
                      &Unimplemented);
  l->AddUpcallRpc("PPB_AudioConfig_Dev_GetSampleRate:l:i", &Unimplemented);
  l->AddUpcallRpc("PPB_AudioConfig_Dev_GetSampleFrameCount:l:i",
                      &Unimplemented);
  l->AddUpcallRpc("PPB_Core_AddRefResource:l:", &Unimplemented);
  l->AddUpcallRpc("PPB_Core_ReleaseResource:l:", &Unimplemented);
  l->AddUpcallRpc("ReleaseResourceMultipleTimes:li:", &Unimplemented);
  l->AddUpcallRpc("PPB_Core_GetTime::d", &Unimplemented);
  l->AddUpcallRpc("PPB_Graphics2D_Create:lCi:l", &Unimplemented);
  l->AddUpcallRpc("PPB_Graphics2D_IsGraphics2D:l:i", &Unimplemented);
  l->AddUpcallRpc("PPB_Graphics2D_Describe:l:Cii", &Unimplemented);
  l->AddUpcallRpc("PPB_Graphics2D_PaintImageData:llCC:", &Unimplemented);
  l->AddUpcallRpc("PPB_Graphics2D_Scroll:lCC:", &Unimplemented);
  l->AddUpcallRpc("PPB_Graphics2D_ReplaceContents:ll:", &Unimplemented);
  l->AddUpcallRpc("PPB_ImageData_GetNativeImageDataFormat::i", &Unimplemented);
  l->AddUpcallRpc("PPB_ImageData_IsImageDataFormatSupported:i:i",
                      &Unimplemented);
  l->AddUpcallRpc("PPB_ImageData_Create:liCi:l", &Unimplemented);
  l->AddUpcallRpc("PPB_ImageData_IsImageData:l:i", &Unimplemented);
  l->AddUpcallRpc("PPB_ImageData_Describe:l:Chii", &Unimplemented);
  l->AddUpcallRpc("PPB_Instance_GetWindowObject:l:C", &Unimplemented);
  l->AddUpcallRpc("PPB_Instance_GetOwnerElementObject:l:C", &Unimplemented);
  l->AddUpcallRpc("PPB_Instance_BindGraphics:ll:i", &Unimplemented);
  l->AddUpcallRpc("PPB_Instance_IsFullFrame:l:i", &Unimplemented);
  l->AddUpcallRpc("PPB_Instance_ExecuteScript:lCC:CC", &Unimplemented);
  l->AddUpcallRpc("PPB_URLRequestInfo_Create:l:l", &Unimplemented);
  l->AddUpcallRpc("PPB_URLRequestInfo_IsURLRequestInfo:l:i",
                      &Unimplemented);
  l->AddUpcallRpc("PPB_URLRequestInfo_SetProperty:liC:i", &Unimplemented);
  l->AddUpcallRpc("PPB_URLRequestInfo_AppendDataToBody:lC:i",
                      &Unimplemented);
  l->AddUpcallRpc("PPB_URLRequestInfo_AppendFileToBody:lllld:i",
                      &Unimplemented);
  l->AddUpcallRpc("PPB_URLResponseInfo_IsURLResponseInfo:l:i",
                      &Unimplemented);
  l->AddUpcallRpc("PPB_URLResponseInfo_GetProperty:li:C", &Unimplemented);
  l->AddUpcallRpc("PPB_URLResponseInfo_GetBodyAsFileRef:l:l",
                      &Unimplemented);
  return true;
}

bool HandlerPepperInit(NaClCommandLoop* ncl, const vector<string>& args) {
  if (args.size() != 2) {
    NaClLog(LOG_ERROR, "not the right number of args for this command\n");
    return false;
  }

  DescWrapperFactory* factory = new DescWrapperFactory();
  NaClLog(1, "InitializePepperRpc\n");

  // NOTE: these are really NaClDescXferableDataDesc. Code was mimicked after
  //       the exisiting plugin code
  NaClLog(1, "create socket pair so that client can make pepper upcalls\n");

  DescWrapper* descs[2] = { NULL, NULL };
  if (0 != factory->MakeSocketPair(descs)) {
    NaClLog(LOG_FATAL, "cannot create socket pair\n");
  }

  NaClLog(1, "spawning secondary service thread\n");

  NaClThread thread;
  if (!NaClThreadCtor(
        &thread,
        PepperHandlerThread,
        descs[0],
        128 << 10)) {
    NaClLog(LOG_FATAL, "cannot create service handler thread\n");
  }

  ncl->AddDesc(descs[1]->desc(), args[1]);
  return true;
}
