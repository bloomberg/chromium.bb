/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file exports a single function used to setup the
// file-io sub-system for use with sel_universal

// NOTE: this is for experimentation and testing. We are not concerned
//       about descriptor and memory leaks

#include <string>
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"

#define SRPC_PARAMS NaClSrpcRpc* rpc,  \
                    NaClSrpcArg** ins, \
                    NaClSrpcArg** outs, \
                    NaClSrpcClosure* done

#define UNIMPLEMENTED(name) \
static void name(SRPC_PARAMS) { \
  UNREFERENCED_PARAMETER(rpc); \
  UNREFERENCED_PARAMETER(ins); \
  UNREFERENCED_PARAMETER(outs); \
  UNREFERENCED_PARAMETER(done); \
  NaClLog(LOG_FATAL, #name " not yet implemented\n"); \
}

UNIMPLEMENTED(PPB_URLLoader_Create)
UNIMPLEMENTED(PPB_URLLoader_IsURLLoader)
UNIMPLEMENTED(PPB_URLLoader_Open)
UNIMPLEMENTED(PPB_URLLoader_FollowRedirect)
UNIMPLEMENTED(PPB_URLLoader_GetUploadProgress)
UNIMPLEMENTED(PPB_URLLoader_GetDownloadProgress)
UNIMPLEMENTED(PPB_URLLoader_GetResponseInfo)
UNIMPLEMENTED(PPB_URLLoader_ReadResponseBody)
UNIMPLEMENTED(PPB_URLLoader_FinishStreamingToFile)
UNIMPLEMENTED(PPB_URLLoader_Close)
UNIMPLEMENTED(PPB_URLRequestInfo_Create)
UNIMPLEMENTED(PPB_URLRequestInfo_IsURLRequestInfo)
UNIMPLEMENTED(PPB_URLRequestInfo_SetProperty)
UNIMPLEMENTED(PPB_URLRequestInfo_AppendDataToBody)
UNIMPLEMENTED(PPB_URLRequestInfo_AppendFileToBody)
UNIMPLEMENTED(PPB_URLResponseInfo_IsURLResponseInfo)
UNIMPLEMENTED(PPB_URLResponseInfo_GetProperty)
UNIMPLEMENTED(PPB_URLResponseInfo_GetBodyAsFileRef)

#define TUPLE(a, b) #a #b, a
bool HandlerFileIOInitialize(NaClCommandLoop* ncl, const vector<string>& args) {
  NaClLog(LOG_INFO, "HandlerFileIOInitialize\n");
  if (args.size() < 5) {
    NaClLog(LOG_ERROR, "Insufficient arguments to 'rpc' command.\n");
    return false;
  }

  UNREFERENCED_PARAMETER(ncl);

  ncl->AddUpcallRpc(TUPLE(PPB_URLLoader_Create, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLLoader_IsURLLoader, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLLoader_Open, :iii:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLLoader_FollowRedirect, :ii:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLLoader_GetUploadProgress, :i:lli));
  ncl->AddUpcallRpc(TUPLE(PPB_URLLoader_GetDownloadProgress, :i:lli));
  ncl->AddUpcallRpc(TUPLE(PPB_URLLoader_GetResponseInfo, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLLoader_ReadResponseBody, :iii:Ci));
  ncl->AddUpcallRpc(TUPLE(PPB_URLLoader_FinishStreamingToFile, :ii:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLLoader_Close, :i:));
  ncl->AddUpcallRpc(TUPLE(PPB_URLRequestInfo_Create, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLRequestInfo_IsURLRequestInfo, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLRequestInfo_SetProperty, :iiC:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLRequestInfo_AppendDataToBody, :iC:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLRequestInfo_AppendFileToBody, :iilld:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLResponseInfo_IsURLResponseInfo, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_URLResponseInfo_GetProperty, :ii:C));
  ncl->AddUpcallRpc(TUPLE(PPB_URLResponseInfo_GetBodyAsFileRef, :i:i));
  return true;
}
