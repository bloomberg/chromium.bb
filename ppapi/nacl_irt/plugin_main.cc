// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/nacl_irt/plugin_main.h"

#include "build/build_config.h"
// Need to include this before most other files because it defines
// IPC_MESSAGE_LOG_ENABLED. We need to use it to define
// IPC_MESSAGE_MACROS_LOG_ENABLED so ppapi_messages.h will generate the
// ViewMsgLog et al. functions.

#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "ipc/ipc_logging.h"
#include "ppapi/nacl_irt/plugin_startup.h"
#include "ppapi/nacl_irt/ppapi_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"

#if defined(__native_client__)
#include "native_client/src/shared/srpc/nacl_srpc_ppapi_plugin_internal.h"
#endif

void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* thread_functions) {
  // Initialize all classes that need to create threads that call back into
  // user code.
  ppapi::PPB_Audio_Shared::SetThreadFunctions(thread_functions);
}

int PpapiPluginMain() {
  base::MessageLoop loop;
  ppapi::proxy::PluginGlobals plugin_globals;

#if defined(OS_NACL_SFI)
  // This is currently needed so that the NaCl reverse service calls the
  // StartupInitializationComplete() SRPC method, which unblocks plugin
  // startup on the Chromium side.
  // TODO(mseaborn): Remove the need to call this.
  NaClPluginLowLevelInitializationComplete();
#endif

  ppapi::PpapiDispatcher ppapi_dispatcher(
      ppapi::GetIOThread()->message_loop_proxy(),
      ppapi::GetShutdownEvent(),
      ppapi::GetBrowserIPCFileDescriptor(),
      ppapi::GetRendererIPCFileDescriptor());
  plugin_globals.SetPluginProxyDelegate(&ppapi_dispatcher);

  loop.Run();

  return 0;
}
