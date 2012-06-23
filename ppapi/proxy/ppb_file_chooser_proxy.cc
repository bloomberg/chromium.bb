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
#include "ppapi/shared_impl/array_writer.h"
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
  virtual int32_t Show(const PP_ArrayOutput& output,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t ShowWithoutUserGesture(
      PP_Bool save_as,
      PP_Var suggested_file_name,
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback);
  virtual int32_t Show0_5(scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Resource GetNextChosenFile() OVERRIDE;
  virtual int32_t ShowWithoutUserGesture0_5(
      PP_Bool save_as,
      PP_Var suggested_file_name,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;

  // Handles the choose complete notification from the host.
  void ChooseComplete(
    int32_t result_code,
    const std::vector<PPB_FileRef_CreateInfo>& chosen_files);

 private:
  int32_t Show(bool require_user_gesture,
               PP_Bool save_as,
               PP_Var suggested_file_name,
               scoped_refptr<TrackedCallback> callback);

  // When using v0.6 of the API, contains the array output info.
  ArrayWriter output_;

  scoped_refptr<TrackedCallback> current_show_callback_;

  // When using v0.5 of the API, contains all files returned by the current
  // show callback that haven't yet been given to the plugin. The plugin will
  // repeatedly call us to get the next file, and we'll vend those out of this
  // queue, removing them when ownership has transferred to the plugin.
  std::queue<PP_Resource> file_queue_;

  DISALLOW_COPY_AND_ASSIGN(FileChooser);
};

FileChooser::FileChooser(const HostResource& resource)
    : Resource(OBJECT_IS_PROXY, resource) {
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

int32_t FileChooser::Show(const PP_ArrayOutput& output,
                          scoped_refptr<TrackedCallback> callback) {
  int32_t result = Show(true, PP_FALSE, PP_MakeUndefined(), callback);
  if (result == PP_OK_COMPLETIONPENDING)
    output_.set_pp_array_output(output);
  return result;
}

int32_t FileChooser::ShowWithoutUserGesture(
    PP_Bool save_as,
    PP_Var suggested_file_name,
    const PP_ArrayOutput& output,
    scoped_refptr<TrackedCallback> callback) {
  int32_t result = Show(false, save_as, suggested_file_name, callback);
  if (result == PP_OK_COMPLETIONPENDING)
    output_.set_pp_array_output(output);
  return result;
}

int32_t FileChooser::Show0_5(scoped_refptr<TrackedCallback> callback) {
  return Show(true, PP_FALSE, PP_MakeUndefined(), callback);
}

int32_t FileChooser::ShowWithoutUserGesture0_5(
    PP_Bool save_as,
    PP_Var suggested_file_name,
    scoped_refptr<TrackedCallback> callback) {
  return Show(false, save_as, suggested_file_name, callback);
}

int32_t FileChooser::Show(bool require_user_gesture,
                          PP_Bool save_as,
                          PP_Var suggested_file_name,
                          scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(current_show_callback_))
    return PP_ERROR_INPROGRESS;  // Can't show more than once.

  current_show_callback_ = callback;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForResource(this);
  dispatcher->Send(
      new PpapiHostMsg_PPBFileChooser_Show(
          API_ID_PPB_FILE_CHOOSER,
          host_resource(),
          save_as,
          SerializedVarSendInput(dispatcher, suggested_file_name),
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
  if (output_.is_valid()) {
    // Using v0.6 of the API with the output array.
    std::vector<PP_Resource> files;
    for (size_t i = 0; i < chosen_files.size(); i++)
      files.push_back(PPB_FileRef_Proxy::DeserializeFileRef(chosen_files[i]));
    output_.StoreResourceVector(files);
  } else {
    // Convert each of the passed in file infos to resources. These will be
    // owned by the FileChooser object until they're passed to the plugin.
    DCHECK(file_queue_.empty());
    for (size_t i = 0; i < chosen_files.size(); i++) {
      file_queue_.push(PPB_FileRef_Proxy::DeserializeFileRef(
          chosen_files[i]));
    }
  }

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
PP_Resource PPB_FileChooser_Proxy::CreateProxyResource(
    PP_Instance instance,
    PP_FileChooserMode_Dev mode,
    const char* accept_types) {
  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBFileChooser_Create(
      API_ID_PPB_FILE_CHOOSER, instance,
      mode,
      accept_types ? accept_types : "",
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
    std::string accept_types,
    HostResource* result) {
  thunk::EnterResourceCreation enter(instance);
  if (enter.succeeded()) {
    result->SetHostResource(instance, enter.functions()->CreateFileChooser(
        instance,
        static_cast<PP_FileChooserMode_Dev>(mode),
        accept_types.c_str()));
  }
}

void PPB_FileChooser_Proxy::OnMsgShow(
    const HostResource& chooser,
    PP_Bool save_as,
    SerializedVarReceiveInput suggested_file_name,
    bool require_user_gesture) {
  scoped_refptr<RefCountedArrayOutputAdapter<PP_Resource> > output(
      new RefCountedArrayOutputAdapter<PP_Resource>);
  EnterHostFromHostResourceForceCallback<PPB_FileChooser_API> enter(
      chooser,
      callback_factory_.NewOptionalCallback(
          &PPB_FileChooser_Proxy::OnShowCallback, output, chooser));
  if (enter.succeeded()) {
    if (require_user_gesture) {
      enter.SetResult(enter.object()->Show(output->pp_array_output(),
                                           enter.callback()));
    } else {
      enter.SetResult(enter.object()->ShowWithoutUserGesture(
          save_as,
          suggested_file_name.Get(dispatcher()),
          output->pp_array_output(),
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

void PPB_FileChooser_Proxy::OnShowCallback(
    int32_t result,
    scoped_refptr<RefCountedArrayOutputAdapter<PP_Resource> >
        output,
    HostResource chooser) {
  EnterHostFromHostResource<PPB_FileChooser_API> enter(chooser);

  std::vector<PPB_FileRef_CreateInfo> files;
  if (enter.succeeded() && result == PP_OK) {
    PPB_FileRef_Proxy* file_ref_proxy = static_cast<PPB_FileRef_Proxy*>(
        dispatcher()->GetInterfaceProxy(API_ID_PPB_FILE_REF));

    // Convert the returned files to the serialized info.
    for (size_t i = 0; i < output->output().size(); i++) {
      PPB_FileRef_CreateInfo cur_create_info;
      file_ref_proxy->SerializeFileRef(output->output()[i], &cur_create_info);
      files.push_back(cur_create_info);
    }
  }

  dispatcher()->Send(new PpapiMsg_PPBFileChooser_ChooseComplete(
      API_ID_PPB_FILE_CHOOSER, chooser, result, files));
}

}  // namespace proxy
}  // namespace ppapi
