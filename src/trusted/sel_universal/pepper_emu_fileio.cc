/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file exports a single function used to setup the
// file-io sub-system for use with sel_universal

// NOTE: this is for experimentation and testing. We are not concerned
//       about descriptor and memory leaks

#include <stdio.h>
#include <string>
#include <map>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/sel_universal/pepper_emu_helper.h"
#include "native_client/src/trusted/sel_universal/pepper_emu.h"
#include "native_client/src/trusted/sel_universal/primitives.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/srpc_helper.h"
namespace {

// This will be used to invoke the call backs
IMultimedia* GlobalMultiMediaInterface = 0;

using std::map;
using std::string;

struct DataLoader {
  int dummy;
  FILE* fp;
  int request_info;
};

struct DataRequestInfo {
  string url;
  string method;
  int stream_to_file;
};

struct DataResponseInfo {
  int dummy;
};

Resource<DataLoader> GlobalLoaderResources(500, "loader");
Resource<DataResponseInfo> GlobalResponseInfoResources(500, "response_info");
Resource<DataRequestInfo> GlobalRequestInfoResources(500, "request_info");


map<string, string> GlobalUrlToFilenameMap;


static void PPB_URLRequestInfo_Create(SRPC_PARAMS) {
  int instance = ins[0]->u.ival;
  NaClLog(1, "PPB_URLRequestInfo_Create(%d)\n", instance);

  outs[0]->u.ival = GlobalRequestInfoResources.Alloc();
  if (outs[0]->u.ival < 0) {
    NaClLog(LOG_FATAL, "PPB_URLRequestInfo_Create: failed\n");
  }
  NaClLog(1, "PPB_URLRequestInfo_Create -> %d\n", outs[0]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


static void PPB_URLLoader_Create(SRPC_PARAMS) {
  int instance = ins[0]->u.ival;
  NaClLog(1, "PPB_URLLoader_Create(%d)\n", instance);

  outs[0]->u.ival = GlobalLoaderResources.Alloc();
  if (outs[0]->u.ival < 0) {
    NaClLog(LOG_FATAL, "PPB_URLLoader_Create: failed\n");
  }
  NaClLog(1, "PPB_URLLoader_Create -> %d\n", outs[0]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


static void PPB_URLLoader_GetResponseInfo(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_URLLoader_GetResponseInfo(%d)\n", handle);

  outs[0]->u.ival = GlobalResponseInfoResources.Alloc();
  if (outs[0]->u.ival < 0) {
    NaClLog(LOG_FATAL, "PPB_URLLoader_GetResponseInfo: failed\n");
  }
  NaClLog(1, "PPB_URLLoader_GetResponseInfo -> %d\n", outs[0]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


static void PPB_URLRequestInfo_SetProperty(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  int property = ins[1]->u.ival;
  NaClLog(1, "PPB_URLRequestInfo_SetProperty(%d, %d)\n", handle, property);
  DataRequestInfo* request_info =
    GlobalRequestInfoResources.GetDataForHandle(handle);
  if (request_info == NULL) {
    NaClLog(LOG_FATAL, "PPB_URLRequestInfo_SetProperty: unknown resource\n");
  }
  switch (property) {
    default:
      NaClLog(LOG_FATAL, "PPB_URLRequestInfo_SetProperty: unknown property\n");
      break;
    case PP_URLREQUESTPROPERTY_URL:
      request_info->url = GetMarshalledJSString(ins[2]);
      break;
    case PP_URLREQUESTPROPERTY_METHOD:
      request_info->method = GetMarshalledJSString(ins[2]);
      break;
    case PP_URLREQUESTPROPERTY_STREAMTOFILE:
      request_info->stream_to_file = GetMarshalledJSBool(ins[2]);
      break;
  }

  outs[0]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


static void PPB_URLLoader_Open(SRPC_PARAMS) {
  int handle_loader = ins[0]->u.ival;
  int request_info_handle = ins[1]->u.ival;
  int callback = ins[2]->u.ival;
  NaClLog(1, "PPB_URLLoader_Open(%d, %d, %d)\n",
          handle_loader, request_info_handle, callback);
  DataRequestInfo* request_info =
    GlobalRequestInfoResources.GetDataForHandle(request_info_handle);
  DataLoader* loader = GlobalLoaderResources.GetDataForHandle(handle_loader);
  loader->request_info = request_info_handle;
  string filename = request_info->url;
  NaClLog(1, "PPB_URLLoader_Open opening url: [%s]\n", filename.c_str());
  if (GlobalUrlToFilenameMap.find(filename) != GlobalUrlToFilenameMap.end()) {
    filename = GlobalUrlToFilenameMap[filename];
    NaClLog(1, "Using Alias: [%s]\n", filename.c_str());
  }
  loader->fp = fopen(filename.c_str(), "rb");
  if (loader->fp == NULL) {
    NaClLog(LOG_WARNING, "PPB_URLLoader_Open could not open file\n");
  }
  int result = (loader->fp == NULL) ? PP_ERROR_FAILED : PP_OK;
  UserEvent* event =
    MakeUserEvent(EVENT_TYPE_OPEN_CALLBACK, callback, result, 0, 0);
  GlobalMultiMediaInterface->PushUserEvent(event);

  outs[0]->u.ival = PP_OK_COMPLETIONPENDING;
  NaClLog(1, "PPB_URLLoader_Open -> %d\n", outs[0]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


static void PPB_URLResponseInfo_GetProperty(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  int property = ins[1]->u.ival;
  NaClLog(1, "PPB_URLResponseInfo_GetProperty(%d, %d)\n", handle, property);

  switch (property) {
    default:
      NaClLog(LOG_FATAL, "PPB_URLResponseInfo_GetProperty: unknown property\n");
      break;
    case PP_URLRESPONSEPROPERTY_STATUSCODE:
      // we are rather optimistic
      SetMarshalledJSInt(outs[0], 200);
      break;
  }

  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_URLLoader_ReadResponseBody:iii:Ci
static void PPB_URLLoader_ReadResponseBody(SRPC_PARAMS) {
  int handle_loader = ins[0]->u.ival;
  int size = ins[1]->u.ival;
  int callback = ins[2]->u.ival;
  DataLoader* loader = GlobalLoaderResources.GetDataForHandle(handle_loader);
  DataRequestInfo* request_info = GlobalRequestInfoResources.
                                  GetDataForHandle(loader->request_info);
  NaClLog(1, "PPB_URLLoader_ReadResponseBody(%d (%s), %d, %d)\n",
          handle_loader, request_info->url.c_str(), size, callback);
  char* buffer = static_cast<char*>(malloc(size));
  const int n = (int) fread(buffer, 1, size, loader->fp);
  UserEvent* event =
    MakeUserEvent(EVENT_TYPE_READ_CALLBACK, callback, n, buffer, n);
  if (n < size) {
    NaClLog(1, "PPB_URLLoader_ReadResponseBody reached eof\n");
  }
  GlobalMultiMediaInterface->PushUserEvent(event);
  outs[0]->u.count = 0;
  outs[1]->u.ival = PP_OK_COMPLETIONPENDING;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

UNIMPLEMENTED(PPB_URLLoader_IsURLLoader)

UNIMPLEMENTED(PPB_URLLoader_FollowRedirect)
UNIMPLEMENTED(PPB_URLLoader_GetUploadProgress)
UNIMPLEMENTED(PPB_URLLoader_GetDownloadProgress)
UNIMPLEMENTED(PPB_URLLoader_FinishStreamingToFile)
UNIMPLEMENTED(PPB_URLLoader_Close)

UNIMPLEMENTED(PPB_URLRequestInfo_IsURLRequestInfo)
UNIMPLEMENTED(PPB_URLRequestInfo_AppendDataToBody)
UNIMPLEMENTED(PPB_URLRequestInfo_AppendFileToBody)

UNIMPLEMENTED(PPB_URLResponseInfo_IsURLResponseInfo)
UNIMPLEMENTED(PPB_URLResponseInfo_GetBodyAsFileRef)

}  // end namespace

void RegisterFileAliasForUrl(string url, string filename) {
  CHECK(GlobalUrlToFilenameMap.find(url) == GlobalUrlToFilenameMap.end());
  GlobalUrlToFilenameMap[url] = filename;
}


#define TUPLE(a, b) #a #b, a
void PepperEmuInitFileIO(NaClCommandLoop* ncl, IMultimedia* im) {
  GlobalMultiMediaInterface = im;

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
}
