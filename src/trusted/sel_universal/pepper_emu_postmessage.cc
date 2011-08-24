/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// initialize post-message sub-system for use with sel_universal
// for now we just print the post-message content on the screen for debugging


#include <string>
#include <iostream>

#include "ppapi/c/pp_input_event.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/sel_universal/pepper_emu.h"
#include "native_client/src/trusted/sel_universal/pepper_emu_helper.h"
#include "native_client/src/trusted/sel_universal/primitives.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/srpc_helper.h"

namespace {

IMultimedia* GlobalMultiMediaInterface = 0;
std::string GlobalQuitMessage;

// PPB_Messaging_PostMessage:iC:
void PPB_Messaging_PostMessage(SRPC_PARAMS) {
  UNREFERENCED_PARAMETER(outs);
  // NOTE: only string supported for now
  std::string message = GetMarshalledJSString(ins[1]);
  std::cout << "POST_MESSAGE: [" << message << "]\n";
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);

  // automatic termination mechanism used for testing
  if (GlobalQuitMessage.size() > 0 && GlobalQuitMessage == message) {
    NaClLog(LOG_INFO, "'quit message' triggered termination\n");
    UserEvent* event = MakeTerminationEvent();
    GlobalMultiMediaInterface->PushUserEvent(event);
  }
}

}  // end namespace

#define TUPLE(a, b) #a #b, a
void PepperEmuInitPostMessage(NaClCommandLoop* ncl, IMultimedia* im) {
  GlobalMultiMediaInterface = im;
  NaClLog(LOG_INFO, "HandlerPostMessageInitialize\n");

  ncl->AddUpcallRpc(TUPLE(PPB_Messaging_PostMessage, :iC:));
}


bool HandlerPepperEmuSetQuitMessage(NaClCommandLoop* ncl,
                                    const std::vector<std::string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 2) {
    NaClLog(LOG_ERROR, "Insufficient arguments to 'rpc' command.\n");
    return false;
  }
  // drop quotes - no escaping yet
  GlobalQuitMessage = args[1].substr(1, args[1].size() - 2);
  NaClLog(LOG_INFO, "Setting 'quit message' to  [%s]\n",
          GlobalQuitMessage.c_str());


  return true;
}
