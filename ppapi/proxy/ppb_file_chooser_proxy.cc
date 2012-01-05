// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_file_chooser_proxy.h"

#include <queue>

#include "base/bind.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::PPB_FileChooser_API;

namespace ppapi {
namespace proxy {

namespace {
InterfaceProxy* CreateFileChooserProxy(Dispatcher* dispatcher) {
  return new PPB_FileChooser_Proxy(dispatcher);
}

class FileChooser : public Resource,
                    public PPB_FileChooser_API {
 public:
  FileChooser(const HostResource& resource);
  virtual ~FileChooser();

  // Resource overrides.
  virtual PPB_FileChooser_API* AsPPB_FileChooser_API() OVERRIDE;

  // PPB_FileChooser_API implementation.
  virtual int32_t Show(const PP_CompletionCallback& callback) OVERRIDE;
  virtual PP_Resource GetNextChosenFile() OVERRIDE;
  virtual int32_t ShowWithoutUserGesture(
      bool save_as,
      const char* suggested_file_name,
      const PP_CompletionCallback& callback) OVERRIDE;

  // Handles the choose complete notification from the host.
  void ChooseComplete(
    int32_t result_code,
    const std::vector<PPB_FileRef_CreateInfo>& chosen_files);

 private:
  int32_t Show(bool require_user_gesture,
               bool save_as,
               const char* suggested_file_name,
               const PP_CompletionCallback& callback);

  scoped_refptr<TrackedCallback> current_show_callback_;

  // All files returned by the current show callback that haven't yet been
  // given to the plugin. The plugin will repeatedly call us to get the next
  // file, and we'll vend those out of this queue, removing them when ownership
  // has transferred to the plugin.
  std::queue<PP_Resource> file_queue_;

  DISALLOW_COPY_AND_ASSIGN(FileChooser);
};

FileChooser::FileChooser(const HostResource& resource) : Resource(resource) {
}

FileChooser::~FileChooser() {
  // Any existing files we haven't transferred ownership to the plugin need
  // to be freed.
  ResourceTracker* tracker = PpapiGlobals::Get()->GetResourceTracker();
  while (!file_queue_.empty()) {
    tracker->ReleaseResource(file_queue_.front());
    file_queue_.pop();
  }
}

PPB_FileChooser_API* FileChooser::AsPPB_FileChooser_API() {
  return this;
}

int32_t FileChooser::Show(const PP_CompletionCallback& callback) {
  return Show(true, false, NULL, callback);
}

int32_t FileChooser::ShowWithoutUserGesture(
    bool save_as,
    const char* suggested_file_name,
    const PP_CompletionCallback& callback) {
  return Show(false, save_as, suggested_file_name, callback);
}

int32_t FileChooser::Show(bool require_user_gesture,
                          bool save_as,
                          const char* suggested_file_name,
                          const PP_CompletionCallback& callback) {
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (TrackedCallback::IsPending(current_show_callback_))
    return PP_ERROR_INPROGRESS;  // Can't show more than once.

  current_show_callback_ = new TrackedCallback(this, callback);
  PluginDispatcher::GetForResource(this)->Send(
      new PpapiHostMsg_PPBFileChooser_Show(
          API_ID_PPB_FILE_CHOOSER,
          host_resource(),
          save_as,
          suggested_file_name ? suggested_file_name : "",
          require_user_gesture));
  return PP_OK_COMPLETIONPENDING;
}

PP_Resource FileChooser::GetNextChosenFile() {
  if (file_queue_.empty())
    return 0;

  // Return the next resource in the queue. These resource have already been
  // addrefed (they're currently owned by the FileChooser) and returning them
  // transfers ownership of that reference to the plugin.
  PP_Resource next = file_queue_.front();
  file_queue_.pop();
  return next;
}

void FileChooser::ChooseComplete(
    int32_t result_code,
    const std::vector<PPB_FileRef_CreateInfo>& chosen_files) {
  // Convert each of the passed in file infos to resources. These will be owned
  // by the FileChooser object until they're passed to the plugin.
  DCHECK(file_queue_.empty());
  for (size_t i = 0; i < chosen_files.size(); i++)
    file_queue_.push(PPB_FileRef_Proxy::DeserializeFileRef(chosen_files[i]));

  // Notify the plugin of the new data.
  TrackedCallback::ClearAndRun(&current_show_callback_, result_code);
  // DANGER: May delete |this|!
}

}  // namespace

PPB_FileChooser_Proxy::PPB_FileChooser_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_FileChooser_Proxy::~PPB_FileChooser_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_FileChooser_Proxy::GetTrustedInfo() {
  static const Info info = {
    thunk::GetPPB_FileChooser_Trusted_0_5_Thunk(),
    PPB_FILECHOOSER_TRUSTED_INTERFACE_0_5,
    API_ID_NONE,  // FILE_CHOOSER is the canonical one.
    false,
    &CreateFileChooserProxy
  };
  return &info;
}

// static
PP_Resource PPB_FileChooser_Proxy::CreateProxyResource(
    PP_Instance instance,
    PP_FileChooserMode_Dev mode,
    const char* accept_mime_types) {
  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBFileChooser_Create(
      API_ID_PPB_FILE_CHOOSER, instance,
      mode,
      accept_mime_types ? accept_mime_types : "",
      &result));

  if (result.is_null())
    return 0;
  return (new FileChooser(result))->GetReference();
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

void PPB_FileChooser_Proxy::OnMsgCreate(
    PP_Instance instance,
    int mode,
    std::string accept_mime_types,
    HostResource* result) {
  thunk::EnterResourceCreation enter(instance);
  if (enter.succeeded()) {
    result->SetHostResource(instance, enter.functions()->CreateFileChooser(
        instance,
        static_cast<PP_FileChooserMode_Dev>(mode),
        accept_mime_types.c_str()));
  }
}

void PPB_FileChooser_Proxy::OnMsgShow(
    const HostResource& chooser,
    bool save_as,
    std::string suggested_file_name,
    bool require_user_gesture) {
  EnterHostFromHostResourceForceCallback<PPB_FileChooser_API> enter(
      chooser, callback_factory_, &PPB_FileChooser_Proxy::OnShowCallback,
      chooser);
  if (enter.succeeded()) {
    if (require_user_gesture) {
      enter.SetResult(enter.object()->Show(enter.callback()));
    } else {
      enter.SetResult(enter.object()->ShowWithoutUserGesture(
          save_as,
          suggested_file_name.c_str(),
          enter.callback()));
    }
  }
}

void PPB_FileChooser_Proxy::OnMsgChooseComplete(
    const HostResource& chooser,
    int32_t result_code,
    const std::vector<PPB_FileRef_CreateInfo>& chosen_files) {
  EnterPluginFromHostResource<PPB_FileChooser_API> enter(chooser);
  if (enter.succeeded()) {
    static_cast<FileChooser*>(enter.object())->ChooseComplete(
        result_code, chosen_files);
  }
}

void PPB_FileChooser_Proxy::OnShowCallback(int32_t result,
                                           const HostResource& chooser) {
  EnterHostFromHostResource<PPB_FileChooser_API> enter(chooser);

  std::vector<PPB_FileRef_CreateInfo> files;
  if (enter.succeeded() && result == PP_OK) {
    PPB_FileRef_Proxy* file_ref_proxy = static_cast<PPB_FileRef_Proxy*>(
        dispatcher()->GetInterfaceProxy(API_ID_PPB_FILE_REF));

    // Convert the returned files to the serialized info.
    while (PP_Resource cur_file_resource =
           enter.object()->GetNextChosenFile()) {
      PPB_FileRef_CreateInfo cur_create_info;
      file_ref_proxy->SerializeFileRef(cur_file_resource, &cur_create_info);
      files.push_back(cur_create_info);
    }
  }

  dispatcher()->Send(new PpapiMsg_PPBFileChooser_ChooseComplete(
      API_ID_PPB_FILE_CHOOSER, chooser, result, files));
}

}  // namespace proxy
}  // namespace ppapi
