/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// initialize post-message sub-system for use with sel_universal
// for now we just print the post-message content on the screen for debugging


#include <string>
#include <iostream>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/sel_universal/pepper_emu.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/srpc_helper.h"


// Marshalling Conventions Used By NaCl
#define JAVA_SCRIPT_TYPE_STRING 5
#define JAVA_SCRIPT_TYPE_INT 2

struct JavaScriptData {
  uint32_t js_type;
  uint32_t size;
  char     payload[1];
};

namespace {

// Currently not use
IMultimedia* GlobalMultiMediaInterface = 0;


// PPB_Messaging_PostMessage:iC:
void PPB_Messaging_PostMessage(SRPC_PARAMS) {
  UNREFERENCED_PARAMETER(outs);
  JavaScriptData* data = reinterpret_cast<JavaScriptData*>(ins[1]->arrays.carr);
  // NOTE: only string supported for now
  CHECK(data->js_type == JAVA_SCRIPT_TYPE_STRING);
  std::cout << "POST_MESSAGE: " << std::string(data->payload, data->size);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

}  // end namespace

#define TUPLE(a, b) #a #b, a
void PepperEmuInitPostMessage(NaClCommandLoop* ncl, IMultimedia* im) {
  GlobalMultiMediaInterface = im;
  NaClLog(LOG_INFO, "HandlerPostMessageInitialize\n");

  ncl->AddUpcallRpc(TUPLE(PPB_Messaging_PostMessage, :iC:));
}
