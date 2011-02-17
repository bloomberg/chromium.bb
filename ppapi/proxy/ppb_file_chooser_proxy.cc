// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_file_chooser_proxy.h"

#include <queue>

#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

class FileChooser : public PluginResource {
 public:
  FileChooser(const HostResource& resource);
  virtual ~FileChooser();

  virtual FileChooser* AsFileChooser();

  PP_CompletionCallback current_show_callback_;

  // All files returned by the current show callback that haven't yet been
  // given to the plugin. The plugin will repeatedly call us to get the next
  // file, and we'll vend those out of this queue, removing them when ownership
  // has transferred to the plugin.
  std::queue<PP_Resource> file_queue_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileChooser);
};

FileChooser::FileChooser(const HostResource& resource)
    : PluginResource(resource),
      current_show_callback_(PP_MakeCompletionCallback(NULL, NULL)) {
}

FileChooser::~FileChooser() {
  // Always need to fire completion callbacks to prevent a leak in the plugin.
  if (current_show_callback_.func)
    PP_RunCompletionCallback(&current_show_callback_, PP_ERROR_ABORTED);

  // Any existing files we haven't transferred ownership to the plugin need
  // to be freed.
  PluginResourceTracker* tracker = PluginResourceTracker::GetInstance();
  while (!file_queue_.empty()) {
    tracker->ReleaseResource(file_queue_.front());
    file_queue_.pop();
  }
}

FileChooser* FileChooser::AsFileChooser() {
  return this;
}

namespace {

PP_Resource Create(PP_Instance instance,
                   const PP_FileChooserOptions_Dev* options) {
  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBFileChooser_Create(
      INTERFACE_ID_PPB_FILE_CHOOSER, instance,
      options->mode, options->accept_mime_types, &result));

  if (result.is_null())
    return 0;
  linked_ptr<FileChooser> object(new FileChooser(result));
  return PluginResourceTracker::GetInstance()->AddResource(object);
}

PP_Bool IsFileChooser(PP_Resource resource) {
  FileChooser* object = PluginResource::GetAs<FileChooser>(resource);
  return BoolToPPBool(!!object);
}

int32_t Show(PP_Resource chooser, struct PP_CompletionCallback callback) {
  FileChooser* object = PluginResource::GetAs<FileChooser>(chooser);
  if (!object)
    return PP_ERROR_BADRESOURCE;
  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(object->instance());
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  if (object->current_show_callback_.func)
    return PP_ERROR_INPROGRESS;  // Can't show more than once.

  object->current_show_callback_ = callback;
  dispatcher->Send(new PpapiHostMsg_PPBFileChooser_Show(
      INTERFACE_ID_PPB_FILE_CHOOSER,
      object->host_resource()));
  return PP_ERROR_WOULDBLOCK;
}

PP_Resource GetNextChosenFile(PP_Resource chooser) {
  FileChooser* object = PluginResource::GetAs<FileChooser>(chooser);
  if (!object || object->file_queue_.empty())
    return 0;

  // Return the next resource in the queue. These resource have already been
  // addrefed (they're currently owned by the FileChooser) and returning them
  // transfers ownership of that reference to the plugin.
  PP_Resource next = object->file_queue_.front();
  object->file_queue_.pop();
  return next;
}

const PPB_FileChooser_Dev file_chooser_interface = {
  &Create,
  &IsFileChooser,
  &Show,
  &GetNextChosenFile
};

InterfaceProxy* CreateFileChooserProxy(Dispatcher* dispatcher,
                                       const void* target_interface) {
  return new PPB_FileChooser_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_FileChooser_Proxy::PPB_FileChooser_Proxy(Dispatcher* dispatcher,
                                             const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_FileChooser_Proxy::~PPB_FileChooser_Proxy() {
}

const InterfaceProxy::Info* PPB_FileChooser_Proxy::GetInfo() {
  static const Info info = {
    &file_chooser_interface,
    PPB_FILECHOOSER_DEV_INTERFACE,
    INTERFACE_ID_PPB_FILE_CHOOSER,
    false,
    &CreateFileChooserProxy,
  };
  return &info;
}

bool PPB_FileChooser_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_FileChooser_Proxy, msg)
    // Plugin -> host messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileChooser_Create, OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileChooser_Show, OnMsgShow)

    // Host -> plugin messages.
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFileChooser_ChooseComplete,
                        OnMsgChooseComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_FileChooser_Proxy::OnMsgCreate(PP_Instance instance,
                                        int mode,
                                        const std::string& accept_mime_types,
                                        HostResource* result) {
  PP_FileChooserOptions_Dev options;
  options.mode = static_cast<PP_FileChooserMode_Dev>(mode);
  options.accept_mime_types = accept_mime_types.c_str();
  result->SetHostResource(
      instance, ppb_file_chooser_target()->Create(instance, &options));
}

void PPB_FileChooser_Proxy::OnMsgShow(const HostResource& chooser) {
  CompletionCallback callback = callback_factory_.NewCallback(
      &PPB_FileChooser_Proxy::OnShowCallback, chooser);

  int32_t result = ppb_file_chooser_target()->Show(
      chooser.host_resource(), callback.pp_completion_callback());
  if (result != PP_ERROR_WOULDBLOCK)
    callback.Run(result);
}

void PPB_FileChooser_Proxy::OnMsgChooseComplete(
    const HostResource& chooser,
    int32_t result_code,
    const std::vector<PPBFileRef_CreateInfo>& chosen_files) {
  PP_Resource plugin_resource =
      PluginResourceTracker::GetInstance()->PluginResourceForHostResource(
          chooser);
  if (!plugin_resource)
    return;
  FileChooser* object = PluginResource::GetAs<FileChooser>(plugin_resource);
  if (!object)
    return;

  // Convert each of the passed in file infos to resources. These will be owned
  // by the FileChooser object until they're passed to the plugin.
  DCHECK(object->file_queue_.empty());
  for (size_t i = 0; i < chosen_files.size(); i++) {
    object->file_queue_.push(
        PPB_FileRef_Proxy::DeserializeFileRef(chosen_files[i]));
  }

  // Notify the plugin of the new data. We have to swap out the callback
  // because the plugin may trigger deleting the object from the callback, and
  // the FileChooser object will attempt to call the callback in its destructor
  // with the ABORTED status.
  PP_CompletionCallback callback = object->current_show_callback_;
  object->current_show_callback_ = PP_MakeCompletionCallback(NULL, NULL);
  PP_RunCompletionCallback(&callback, result_code);
  // DANGER: May delete |object|!
}

void PPB_FileChooser_Proxy::OnShowCallback(int32_t result,
                                           const HostResource& chooser) {
  std::vector<PPBFileRef_CreateInfo> files;
  if (result == PP_OK) {
    // Jump through some hoops to get the FileRef proxy. Since we know we're
    // in the host at this point, we can ask the host dispatcher for it.
    DCHECK(!dispatcher()->IsPlugin());
    HostDispatcher* host_disp = static_cast<HostDispatcher*>(dispatcher());
    PPB_FileRef_Proxy* file_ref_proxy = static_cast<PPB_FileRef_Proxy*>(
        host_disp->GetOrCreatePPBInterfaceProxy(INTERFACE_ID_PPB_FILE_REF));

    // Convert the returned files to the serialized info.
    while (PP_Resource cur_file_resource =
               ppb_file_chooser_target()->GetNextChosenFile(
                   chooser.host_resource())) {
      PPBFileRef_CreateInfo cur_create_info;
      file_ref_proxy->SerializeFileRef(cur_file_resource, &cur_create_info);
      files.push_back(cur_create_info);
    }
  }

  dispatcher()->Send(new PpapiMsg_PPBFileChooser_ChooseComplete(
      INTERFACE_ID_PPB_FILE_CHOOSER, chooser, result, files));
}

}  // namespace proxy
}  // namespace pp
