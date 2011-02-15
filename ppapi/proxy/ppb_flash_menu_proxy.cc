// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_menu_proxy.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

class FlashMenu : public PluginResource {
 public:
  explicit FlashMenu(const HostResource& resource)
      : PluginResource(resource),
        callback_(PP_BlockUntilComplete()),
        selected_id_ptr_(NULL) {
  }

  virtual ~FlashMenu() {}

  // Resource overrides.
  virtual FlashMenu* AsFlashMenu() { return this; }

  int32_t* selected_id_ptr() const { return selected_id_ptr_; }
  void set_selected_id_ptr(int32_t* ptr) { selected_id_ptr_ = ptr; }

  PP_CompletionCallback callback() const { return callback_; }
  void set_callback(PP_CompletionCallback cb) { callback_ = cb; }

 private:
  PP_CompletionCallback callback_;
  int32_t* selected_id_ptr_;

  DISALLOW_COPY_AND_ASSIGN(FlashMenu);
};

namespace {

PP_Resource Create(PP_Instance instance_id, const PP_Flash_Menu* menu_data) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return 0;

  HostResource result;
  pp::proxy::SerializedFlashMenu serialized_menu;
  if (!serialized_menu.SetPPMenu(menu_data))
    return 0;

  dispatcher->Send(new PpapiHostMsg_PPBFlashMenu_Create(
      INTERFACE_ID_PPB_FLASH_MENU, instance_id, serialized_menu, &result));
  if (result.is_null())
    return 0;

  linked_ptr<FlashMenu> menu(new FlashMenu(result));
  return PluginResourceTracker::GetInstance()->AddResource(menu);
}

PP_Bool IsFlashMenu(PP_Resource resource) {
  return BoolToPPBool(!!PluginResource::GetAs<FlashMenu>(resource));
}

int32_t Show(PP_Resource menu_id,
             const PP_Point* location,
             int32_t* selected_id,
             PP_CompletionCallback callback) {
  FlashMenu* object = PluginResource::GetAs<FlashMenu>(menu_id);
  if (!object)
    return PP_ERROR_BADRESOURCE;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      object->instance());
  if (!dispatcher)
    return PP_ERROR_FAILED;

  if (object->callback().func)
    return PP_ERROR_INPROGRESS;

  object->set_selected_id_ptr(selected_id);
  object->set_callback(callback);

  dispatcher->Send(new PpapiHostMsg_PPBFlashMenu_Show(
      INTERFACE_ID_PPB_FLASH_MENU, object->host_resource(), *location));

  return PP_ERROR_WOULDBLOCK;
}

const PPB_Flash_Menu flash_menu_interface = {
  &Create,
  &IsFlashMenu,
  &Show,
};

InterfaceProxy* CreateFlashMenuProxy(Dispatcher* dispatcher,
                                     const void* target_interface) {
  return new PPB_Flash_Menu_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Flash_Menu_Proxy::PPB_Flash_Menu_Proxy(Dispatcher* dispatcher,
                                           const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Flash_Menu_Proxy::~PPB_Flash_Menu_Proxy() {
}

const InterfaceProxy::Info* PPB_Flash_Menu_Proxy::GetInfo() {
  static const Info info = {
    &flash_menu_interface,
    PPB_FLASH_MENU_INTERFACE,
    INTERFACE_ID_PPB_FLASH_MENU,
    true,
    &CreateFlashMenuProxy,
  };
  return &info;
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

void PPB_Flash_Menu_Proxy::OnMsgCreate(PP_Instance instance_id,
                                       const SerializedFlashMenu& menu_data,
                                       HostResource* result) {
  PP_Resource resource = ppb_flash_menu_target()->Create(instance_id,
                                                         menu_data.pp_menu());
  result->SetHostResource(instance_id, resource);
}

struct PPB_Flash_Menu_Proxy::ShowRequest {
  HostResource menu;
  int32_t selected_id;
};

void PPB_Flash_Menu_Proxy::OnMsgShow(const HostResource& menu,
                                     const PP_Point& location) {
  ShowRequest* request = new ShowRequest;
  request->menu = menu;
  CompletionCallback callback = callback_factory_.NewCallback(
      &PPB_Flash_Menu_Proxy::SendShowACKToPlugin, request);
  int32_t result = ppb_flash_menu_target()->Show(
      menu.host_resource(),
      &location,
      &request->selected_id,
      callback.pp_completion_callback());
  if (result != PP_ERROR_WOULDBLOCK) {
    // There was some error, so we won't get a callback. We need to now issue
    // the ACK to the plugin so that it hears about the error. This will also
    // clean up the data associated with the callback.
    callback.Run(result);
  }
}

void PPB_Flash_Menu_Proxy::OnMsgShowACK(const HostResource& menu,
                                        int32_t selected_id,
                                        int32_t result) {
  PP_Resource plugin_resource =
      PluginResourceTracker::GetInstance()->PluginResourceForHostResource(menu);
  if (!plugin_resource)
    return;
  FlashMenu* object = PluginResource::GetAs<FlashMenu>(plugin_resource);
  if (!object) {
    // The plugin has released the FlashMenu object so don't issue the
    // callback.
    return;
  }

  // Be careful to make the callback NULL before issuing the callback since the
  // plugin might want to show the menu again from within the callback.
  PP_CompletionCallback callback = object->callback();
  object->set_callback(PP_BlockUntilComplete());
  *(object->selected_id_ptr()) = selected_id;
  PP_RunCompletionCallback(&callback, result);
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
}  // namespace pp
