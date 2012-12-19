// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_proxy.h"

#include <limits>

#include "base/logging.h"
#include "base/message_loop.h"
#include "build/build_config.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_print.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/proxy_module.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "ppapi/thunk/resource_creation_api.h"

using ppapi::thunk::EnterInstanceNoLock;
using ppapi::thunk::EnterResourceNoLock;

namespace ppapi {
namespace proxy {

namespace {

void InvokePrinting(PP_Instance instance) {
  ProxyAutoLock lock;

  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (dispatcher) {
    dispatcher->Send(new PpapiHostMsg_PPBFlash_InvokePrinting(
        API_ID_PPB_FLASH, instance));
  }
}

const PPB_Flash_Print_1_0 g_flash_print_interface = {
  &InvokePrinting
};

}  // namespace

// -----------------------------------------------------------------------------

PPB_Flash_Proxy::PPB_Flash_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Flash_Proxy::~PPB_Flash_Proxy() {
}

// static
const PPB_Flash_Print_1_0* PPB_Flash_Proxy::GetFlashPrintInterface() {
  return &g_flash_print_interface;
}

bool PPB_Flash_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_FLASH))
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_InvokePrinting,
                        OnHostMsgInvokePrinting)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Flash_Proxy::OnHostMsgInvokePrinting(PP_Instance instance) {
  // This function is actually implemented in the PPB_Flash_Print interface.
  // It's rarely used enough that we just request this interface when needed.
  const PPB_Flash_Print_1_0* print_interface =
      static_cast<const PPB_Flash_Print_1_0*>(
          dispatcher()->local_get_interface()(PPB_FLASH_PRINT_INTERFACE_1_0));
  if (print_interface)
    print_interface->InvokePrinting(instance);
}

}  // namespace proxy
}  // namespace ppapi
