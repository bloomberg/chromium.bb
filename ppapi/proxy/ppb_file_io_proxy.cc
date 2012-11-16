// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_file_io_proxy.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_file_io_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"

using ppapi::thunk::PPB_FileIO_API;
using ppapi::thunk::PPB_FileRef_API;

namespace ppapi {
namespace proxy {

namespace {

// The maximum size we'll support reading in one chunk. The renderer process
// must allocate a buffer sized according to the request of the plugin. To
// keep things from getting out of control, we cap the read size to this value.
// This should generally be OK since the API specifies that it may perform a
// partial read.
static const int32_t kMaxReadSize = 33554432;  // 32MB

#if !defined(OS_NACL)
typedef EnterHostFromHostResourceForceCallback<PPB_FileIO_API> EnterHostFileIO;
#endif
typedef EnterPluginFromHostResource<PPB_FileIO_API> EnterPluginFileIO;

class FileIO : public PPB_FileIO_Shared {
 public:
  explicit FileIO(const HostResource& host_resource);
  virtual ~FileIO();

  // PPB_FileIO_API implementation (not provided by FileIOImpl).
  virtual void Close() OVERRIDE;
  virtual int32_t GetOSFileDescriptor() OVERRIDE;
  virtual int32_t WillWrite(int64_t offset,
                            int32_t bytes_to_write,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t WillSetLength(
      int64_t length,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  // FileIOImpl overrides.
  virtual int32_t OpenValidated(
      PP_Resource file_ref_resource,
      PPB_FileRef_API* file_ref_api,
      int32_t open_flags,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t QueryValidated(
      PP_FileInfo* info,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t TouchValidated(
      PP_Time last_access_time,
      PP_Time last_modified_time,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t ReadValidated(
      int64_t offset,
      const PP_ArrayOutput& output_array_buffer,
      int32_t max_read_length,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t WriteValidated(
      int64_t offset,
      const char* buffer,
      int32_t bytes_to_write,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t SetLengthValidated(
      int64_t length,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t FlushValidated(
      scoped_refptr<TrackedCallback> callback) OVERRIDE;

  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  static const ApiID kApiID = API_ID_PPB_FILE_IO;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileIO);
};

FileIO::FileIO(const HostResource& host_resource)
    : PPB_FileIO_Shared(host_resource) {
}

FileIO::~FileIO() {
  Close();
}

void FileIO::Close() {
  if (file_open_) {
    GetDispatcher()->Send(new PpapiHostMsg_PPBFileIO_Close(kApiID,
                                                           host_resource()));
  }
}

int32_t FileIO::GetOSFileDescriptor() {
  return -1;
}

int32_t FileIO::WillWrite(int64_t offset,
                          int32_t bytes_to_write,
                          scoped_refptr<TrackedCallback> callback) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileIO_WillWrite(
      kApiID, host_resource(), offset, bytes_to_write));
  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIO::WillSetLength(int64_t length,
                              scoped_refptr<TrackedCallback> callback) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileIO_WillSetLength(
      kApiID, host_resource(), length));
  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIO::OpenValidated(PP_Resource file_ref_resource,
                              PPB_FileRef_API* file_ref_api,
                              int32_t open_flags,
                              scoped_refptr<TrackedCallback> callback) {
  Resource* file_ref_object =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(file_ref_resource);

  GetDispatcher()->Send(new PpapiHostMsg_PPBFileIO_Open(
      kApiID, host_resource(), file_ref_object->host_resource(), open_flags));
  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIO::QueryValidated(PP_FileInfo* info,
                               scoped_refptr<TrackedCallback> callback) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileIO_Query(
      kApiID, host_resource()));
  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, info);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIO::TouchValidated(PP_Time last_access_time,
                               PP_Time last_modified_time,
                               scoped_refptr<TrackedCallback> callback) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileIO_Touch(
      kApiID, host_resource(), last_access_time, last_modified_time));
  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIO::ReadValidated(int64_t offset,
                              const PP_ArrayOutput& output_array_buffer,
                              int32_t max_read_length,
                              scoped_refptr<TrackedCallback> callback) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileIO_Read(
      kApiID, host_resource(), offset, max_read_length));
  RegisterCallback(OPERATION_READ, callback, &output_array_buffer, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIO::WriteValidated(int64_t offset,
                               const char* buffer,
                               int32_t bytes_to_write,
                               scoped_refptr<TrackedCallback> callback) {
  // TODO(brettw) it would be nice to use a shared memory buffer for large
  // writes rather than having to copy to a string (which will involve a number
  // of extra copies to serialize over IPC).
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileIO_Write(
      kApiID, host_resource(), offset, std::string(buffer, bytes_to_write)));
  RegisterCallback(OPERATION_WRITE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIO::SetLengthValidated(int64_t length,
                                   scoped_refptr<TrackedCallback> callback) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileIO_SetLength(
      kApiID, host_resource(), length));
  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIO::FlushValidated(scoped_refptr<TrackedCallback> callback) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileIO_Flush(
      kApiID, host_resource()));
  RegisterCallback(OPERATION_EXCLUSIVE, callback, NULL, NULL);
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace

// -----------------------------------------------------------------------------

PPB_FileIO_Proxy::PPB_FileIO_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_FileIO_Proxy::~PPB_FileIO_Proxy() {
}

// static
PP_Resource PPB_FileIO_Proxy::CreateProxyResource(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBFileIO_Create(kApiID, instance,
                                                     &result));
  if (result.is_null())
    return 0;
  return (new FileIO(result))->GetReference();
}

bool PPB_FileIO_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_FileIO_Proxy, msg)
  #if !defined(OS_NACL)
    // Plugin -> host message.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_Create, OnHostMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_Open, OnHostMsgOpen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_Close, OnHostMsgClose)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_Query, OnHostMsgQuery)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_Touch, OnHostMsgTouch)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_Read, OnHostMsgRead)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_Write, OnHostMsgWrite)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_SetLength, OnHostMsgSetLength)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_Flush, OnHostMsgFlush)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_WillWrite, OnHostMsgWillWrite)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileIO_WillSetLength,
                        OnHostMsgWillSetLength)
#endif  // !defined(OS_NACL)
    // Host -> plugin messages.
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFileIO_GeneralComplete,
                        OnPluginMsgGeneralComplete)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFileIO_OpenFileComplete,
                        OnPluginMsgOpenFileComplete)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFileIO_QueryComplete,
                        OnPluginMsgQueryComplete)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFileIO_ReadComplete,
                        OnPluginMsgReadComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if !defined(OS_NACL)
void PPB_FileIO_Proxy::OnHostMsgCreate(PP_Instance instance,
                                       HostResource* result) {
  thunk::EnterResourceCreation enter(instance);
  if (enter.succeeded()) {
    result->SetHostResource(instance,
                            enter.functions()->CreateFileIO(instance));
  }
}

void PPB_FileIO_Proxy::OnHostMsgOpen(const HostResource& host_resource,
                                     const HostResource& file_ref_resource,
                                     int32_t open_flags) {
  EnterHostFileIO enter(host_resource, callback_factory_,
      &PPB_FileIO_Proxy::OpenFileCallbackCompleteInHost, host_resource);
  if (enter.succeeded()) {
    enter.SetResult(enter.object()->Open(
        file_ref_resource.host_resource(), open_flags, enter.callback()));
  }
}

void PPB_FileIO_Proxy::OnHostMsgClose(const HostResource& host_resource) {
  EnterHostFromHostResource<PPB_FileIO_API> enter(host_resource);
  if (enter.succeeded())
    enter.object()->Close();
}

void PPB_FileIO_Proxy::OnHostMsgQuery(const HostResource& host_resource) {
  // The callback will take charge of deleting the FileInfo. The contents must
  // be defined so we don't send garbage to the plugin in the failure case.
  PP_FileInfo* info = new PP_FileInfo;
  memset(info, 0, sizeof(PP_FileInfo));
  EnterHostFileIO enter(host_resource, callback_factory_,
                        &PPB_FileIO_Proxy::QueryCallbackCompleteInHost,
                        host_resource, info);
  if (enter.succeeded())
    enter.SetResult(enter.object()->Query(info, enter.callback()));
}

void PPB_FileIO_Proxy::OnHostMsgTouch(const HostResource& host_resource,
                                      PP_Time last_access_time,
                                      PP_Time last_modified_time) {
  EnterHostFileIO enter(host_resource, callback_factory_,
                        &PPB_FileIO_Proxy::GeneralCallbackCompleteInHost,
                        host_resource);
  if (enter.succeeded()) {
    enter.SetResult(enter.object()->Touch(last_access_time, last_modified_time,
                                          enter.callback()));
  }
}

void PPB_FileIO_Proxy::OnHostMsgRead(const HostResource& host_resource,
                                     int64_t offset,
                                     int32_t bytes_to_read) {
  // Validate bytes_to_read before allocating below. This value is coming from
  // the untrusted plugin.
  bytes_to_read = std::min(bytes_to_read, kMaxReadSize);
  if (bytes_to_read < 0) {
    ReadCallbackCompleteInHost(PP_ERROR_FAILED, host_resource,
                               new std::string());
    return;
  }

  // The callback will take charge of deleting the string.
  std::string* dest = new std::string;
  dest->resize(bytes_to_read);
  EnterHostFileIO enter(host_resource, callback_factory_,
                        &PPB_FileIO_Proxy::ReadCallbackCompleteInHost,
                        host_resource, dest);
  if (enter.succeeded()) {
    enter.SetResult(enter.object()->Read(offset,
                                         bytes_to_read > 0 ? &(*dest)[0] : NULL,
                                         bytes_to_read, enter.callback()));
  }
}

void PPB_FileIO_Proxy::OnHostMsgWrite(const HostResource& host_resource,
                                      int64_t offset,
                                      const std::string& data) {
  EnterHostFileIO enter(host_resource, callback_factory_,
                        &PPB_FileIO_Proxy::GeneralCallbackCompleteInHost,
                        host_resource);
  if (enter.succeeded()) {
    enter.SetResult(enter.object()->Write(offset, data.data(), data.size(),
                                          enter.callback()));
  }
}

void PPB_FileIO_Proxy::OnHostMsgSetLength(const HostResource& host_resource,
                                          int64_t length) {
  EnterHostFileIO enter(host_resource, callback_factory_,
                        &PPB_FileIO_Proxy::GeneralCallbackCompleteInHost,
                        host_resource);
  if (enter.succeeded())
    enter.SetResult(enter.object()->SetLength(length, enter.callback()));
}

void PPB_FileIO_Proxy::OnHostMsgFlush(const HostResource& host_resource) {
  EnterHostFileIO enter(host_resource, callback_factory_,
                        &PPB_FileIO_Proxy::GeneralCallbackCompleteInHost,
                        host_resource);
  if (enter.succeeded())
    enter.SetResult(enter.object()->Flush(enter.callback()));
}

void PPB_FileIO_Proxy::OnHostMsgWillWrite(const HostResource& host_resource,
                                          int64_t offset,
                                          int32_t bytes_to_write) {
  EnterHostFileIO enter(host_resource, callback_factory_,
                        &PPB_FileIO_Proxy::GeneralCallbackCompleteInHost,
                        host_resource);
  if (enter.succeeded()) {
    enter.SetResult(enter.object()->WillWrite(offset, bytes_to_write,
                                              enter.callback()));
  }
}

void PPB_FileIO_Proxy::OnHostMsgWillSetLength(const HostResource& host_resource,
                                              int64_t length) {
  EnterHostFileIO enter(host_resource, callback_factory_,
                        &PPB_FileIO_Proxy::GeneralCallbackCompleteInHost,
                        host_resource);
  if (enter.succeeded())
    enter.SetResult(enter.object()->WillSetLength(length, enter.callback()));
}
#endif  // !defined(OS_NACL)

void PPB_FileIO_Proxy::OnPluginMsgGeneralComplete(
    const HostResource& host_resource,
    int32_t result) {
  EnterPluginFileIO enter(host_resource);
  if (enter.succeeded())
    static_cast<FileIO*>(enter.object())->ExecuteGeneralCallback(result);
}

void PPB_FileIO_Proxy::OnPluginMsgOpenFileComplete(
    const HostResource& host_resource,
    int32_t result) {
  EnterPluginFileIO enter(host_resource);
  if (enter.succeeded())
    static_cast<FileIO*>(enter.object())->ExecuteOpenFileCallback(result);
}

void PPB_FileIO_Proxy::OnPluginMsgQueryComplete(
    const HostResource& host_resource,
    int32_t result,
    const PP_FileInfo& info) {
  EnterPluginFileIO enter(host_resource);
  if (enter.succeeded())
    static_cast<FileIO*>(enter.object())->ExecuteQueryCallback(result, info);
}

void PPB_FileIO_Proxy::OnPluginMsgReadComplete(
    const HostResource& host_resource,
    int32_t result,
    const std::string& data) {
  EnterPluginFileIO enter(host_resource);
  if (enter.succeeded()) {
    // The result code should contain the data size if it's positive.
    DCHECK((result < 0 && data.size() == 0) ||
           result == static_cast<int32_t>(data.size()));
    static_cast<FileIO*>(enter.object())->ExecuteReadCallback(result,
                                                              data.data());
  }
}

#if !defined(OS_NACL)
void PPB_FileIO_Proxy::GeneralCallbackCompleteInHost(
    int32_t pp_error,
    const HostResource& host_resource) {
  Send(new PpapiMsg_PPBFileIO_GeneralComplete(kApiID, host_resource, pp_error));
}

void PPB_FileIO_Proxy::OpenFileCallbackCompleteInHost(
    int32_t pp_error,
    const HostResource& host_resource) {
  Send(new PpapiMsg_PPBFileIO_OpenFileComplete(kApiID, host_resource,
                                               pp_error));
}

void PPB_FileIO_Proxy::QueryCallbackCompleteInHost(
    int32_t pp_error,
    const HostResource& host_resource,
    PP_FileInfo* info) {
  Send(new PpapiMsg_PPBFileIO_QueryComplete(kApiID, host_resource, pp_error,
                                            *info));
  delete info;
}

void PPB_FileIO_Proxy::ReadCallbackCompleteInHost(
    int32_t pp_error,
    const HostResource& host_resource,
    std::string* data) {
  // Only send the amount of data in the string that was actually read.
  if (pp_error >= 0) {
    DCHECK(pp_error <= static_cast<int32_t>(data->size()));
    data->resize(pp_error);
  }
  Send(new PpapiMsg_PPBFileIO_ReadComplete(kApiID, host_resource, pp_error,
                                           *data));
  delete data;
}
#endif  // !defined(OS_NACL)

}  // namespace proxy
}  // namespace ppapi
