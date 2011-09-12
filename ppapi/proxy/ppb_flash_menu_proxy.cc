// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_menu_proxy.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_flash_menu_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::PPB_Flash_Menu_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

class FlashMenu : public PPB_Flash_Menu_API, public Resource {
 public:
  explicit FlashMenu(const HostResource& resource);
  virtual ~FlashMenu();

  // Resource overrides.
  virtual PPB_Flash_Menu_API* AsPPB_Flash_Menu_API() OVERRIDE;

  // PPB_Flash_Menu_API implementation.
  virtual int32_t Show(const PP_Point* location,
                       int32_t* selected_id,
                       PP_CompletionCallback callback) OVERRIDE;

  void ShowACK(int32_t selected_id, int32_t result);

 private:
  PP_CompletionCallback callback_;
  int32_t* selected_id_ptr_;

  DISALLOW_COPY_AND_ASSIGN(FlashMenu);
};

FlashMenu::FlashMenu(const HostResource& resource)
    : Resource(resource),
      callback_(PP_BlockUntilComplete()),
      selected_id_ptr_(NULL) {
}

FlashMenu::~FlashMenu() {
}

PPB_Flash_Menu_API* FlashMenu::AsPPB_Flash_Menu_API() {
  return this;
}

int32_t FlashMenu::Show(const struct PP_Point* location,
                        int32_t* selected_id,
                        struct PP_CompletionCallback callback) {
  if (callback_.func)
    return PP_ERROR_INPROGRESS;

  selected_id_ptr_ = selected_id;
  callback_ = callback;

  PluginDispatcher::GetForResource(this)->Send(
      new PpapiHostMsg_PPBFlashMenu_Show(
          INTERFACE_ID_PPB_FLASH_MENU, host_resource(), *location));
  return PP_OK_COMPLETIONPENDING;
}

void FlashMenu::ShowACK(int32_t selected_id, int32_t result) {
  *selected_id_ptr_ = selected_id;
  PP_RunAndClearCompletionCallback(&callback_, result);
}

namespace {

InterfaceProxy* CreateFlashMenuProxy(Dispatcher* dispatcher) {
  return new PPB_Flash_Menu_Proxy(dispatcher);
}

}  // namespace

PPB_Flash_Menu_Proxy::PPB_Flash_Menu_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Flash_Menu_Proxy::~PPB_Flash_Menu_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Flash_Menu_Proxy::GetInfo() {
  static const Info info = {
    ppapi::thunk::GetPPB_Flash_Menu_Thunk(),
    PPB_FLASH_MENU_INTERFACE,
    INTERFACE_ID_PPB_FLASH_MENU,
    true,
    &CreateFlashMenuProxy,
  };
  return &info;
}

// static
PP_Resource PPB_Flash_Menu_Proxy::CreateProxyResource(
    PP_Instance instance_id,
    const PP_Flash_Menu* menu_data) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return 0;

  HostResource result;
  SerializedFlashMenu serialized_menu;
  if (!serialized_menu.SetPPMenu(menu_data))
    return 0;

  dispatcher->Send(new PpapiHostMsg_PPBFlashMenu_Create(
      INTERFACE_ID_PPB_FLASH_MENU, instance_id, serialized_menu, &result));
  if (result.is_null())
    return 0;
  return (new FlashMenu(result))->GetReference();
}

bool PPB_Flash_Menu_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_Menu_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashMenu_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashMenu_Show,
                        OnMsgShow)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFlashMenu_ShowACK,
                        OnMsgShowACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
  return handled;
}

void PPB_Flash_Menu_Proxy::OnMsgCreate(PP_Instance instance,
                                       const SerializedFlashMenu& menu_data,
                                       HostResource* result) {
  thunk::EnterResourceCreation enter(instance);
  if (enter.succeeded()) {
    result->SetHostResource(
        instance,
        enter.functions()->CreateFlashMenu(instance, menu_data.pp_menu()));
  }
}

struct PPB_Flash_Menu_Proxy::ShowRequest {
  HostResource menu;
  int32_t selected_id;
};

void PPB_Flash_Menu_Proxy::OnMsgShow(const HostResource& menu,
                                     const PP_Point& location) {
  ShowRequest* request = new ShowRequest;
  request->menu = menu;

  EnterHostFromHostResourceForceCallback<PPB_Flash_Menu_API> enter(
      menu, callback_factory_, &PPB_Flash_Menu_Proxy::SendShowACKToPlugin,
      request);
  if (enter.succeeded()) {
    enter.SetResult(enter.object()->Show(&location, &request->selected_id,
                                         enter.callback()));
  }
}

void PPB_Flash_Menu_Proxy::OnMsgShowACK(const HostResource& menu,
                                        int32_t selected_id,
                                        int32_t result) {
  EnterPluginFromHostResource<PPB_Flash_Menu_API> enter(menu);
  if (enter.failed())
    return;
  static_cast<FlashMenu*>(enter.object())->ShowACK(selected_id, result);
}

void PPB_Flash_Menu_Proxy::SendShowACKToPlugin(
    int32_t result,
    ShowRequest* request) {
  dispatcher()->Send(new PpapiMsg_PPBFlashMenu_ShowACK(
      INTERFACE_ID_PPB_FLASH_MENU,
      request->menu,
      request->selected_id,
      result));
  delete request;
}

}  // namespace proxy
}  // namespace ppapi
