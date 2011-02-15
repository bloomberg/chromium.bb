// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_FLASH_MENU_PROXY_H_
#define PPAPI_PPB_FLASH_MENU_PROXY_H_

#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

struct PP_Flash_Menu;
struct PP_Point;
struct PPB_Flash_Menu;

namespace pp {
namespace proxy {

class SerializedFlashMenu;

class PPB_Flash_Menu_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_Menu_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Flash_Menu_Proxy();

  static const Info* GetInfo();

  const PPB_Flash_Menu* ppb_flash_menu_target() const {
    return static_cast<const PPB_Flash_Menu*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  struct ShowRequest;

  void OnMsgCreate(PP_Instance instance_id,
                   const SerializedFlashMenu& menu_data,
                   HostResource* resource);
  void OnMsgShow(const HostResource& menu,
                 const PP_Point& location);
  void OnMsgShowACK(const HostResource& menu,
                    int32_t selected_id,
                    int32_t result);
  void SendShowACKToPlugin(int32_t result, ShowRequest* request);

  CompletionCallbackFactory<PPB_Flash_Menu_Proxy,
                            ProxyNonThreadSafeRefCount> callback_factory_;
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_FLASH_MENU_PROXY_H_
