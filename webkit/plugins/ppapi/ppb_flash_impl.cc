// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_flash_impl.h"

#include <string.h>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/private/ppb_flash.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/error_util.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/var.h"

namespace webkit {
namespace ppapi {

// PPB_Flash_Impl --------------------------------------------------------------

namespace {

void SetInstanceAlwaysOnTop(PP_Instance pp_instance, PP_Bool on_top) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return;
  instance->set_always_on_top(PPBoolToBool(on_top));
}

PP_Var GetProxyForURL(PP_Instance pp_instance, const char* url) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_MakeUndefined();

  GURL gurl(url);
  if (!gurl.is_valid())
    return PP_MakeUndefined();

  std::string proxy_host = instance->delegate()->ResolveProxy(gurl);
  if (proxy_host.empty())
    return PP_MakeUndefined();  // No proxy.
  return StringVar::StringToPPVar(instance->module(), proxy_host);
}

FilePath GetFilePathFromUTF8(const char* path) {
#if defined(OS_WIN)
  return FilePath(UTF8ToUTF16(path));
#else
  return FilePath(path);
#endif
}

int32_t OpenModuleLocalFile(PP_Instance pp_instance,
                            const char* path,
                            int32_t mode,
                            PP_FileHandle* file) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  int flags = 0;
  if (mode & PP_FILEOPENFLAG_READ)
    flags |= base::PLATFORM_FILE_READ;
  if (mode & PP_FILEOPENFLAG_WRITE) {
    flags |= base::PLATFORM_FILE_WRITE;
    flags |= base::PLATFORM_FILE_WRITE_ATTRIBUTES;
  }
  if (mode & PP_FILEOPENFLAG_TRUNCATE) {
    DCHECK(mode & PP_FILEOPENFLAG_WRITE);
    flags |= base::PLATFORM_FILE_TRUNCATE;
  }

  if (mode & PP_FILEOPENFLAG_CREATE) {
    if (mode & PP_FILEOPENFLAG_EXCLUSIVE)
      flags |= base::PLATFORM_FILE_CREATE;
    else
      flags |= base::PLATFORM_FILE_OPEN_ALWAYS;
  } else {
    flags |= base::PLATFORM_FILE_OPEN;
  }

  base::PlatformFile base_file;
  base::PlatformFileError result = instance->delegate()->OpenModuleLocalFile(
      instance->module()->name(),
      GetFilePathFromUTF8(path),
      flags,
      &base_file);
  *file = base_file;
  return PlatformFileErrorToPepperError(result);
}


int32_t RenameModuleLocalFile(PP_Instance pp_instance,
                              const char* path_from,
                              const char* path_to) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  base::PlatformFileError result = instance->delegate()->RenameModuleLocalFile(
      instance->module()->name(),
      GetFilePathFromUTF8(path_from),
      GetFilePathFromUTF8(path_to));
  return PlatformFileErrorToPepperError(result);
}

int32_t DeleteModuleLocalFileOrDir(PP_Instance pp_instance,
                                   const char* path,
                                   PP_Bool recursive) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  base::PlatformFileError result =
      instance->delegate()->DeleteModuleLocalFileOrDir(
          instance->module()->name(), GetFilePathFromUTF8(path),
          PPBoolToBool(recursive));
  return PlatformFileErrorToPepperError(result);
}

int32_t CreateModuleLocalDir(PP_Instance pp_instance, const char* path) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  base::PlatformFileError result = instance->delegate()->CreateModuleLocalDir(
      instance->module()->name(), GetFilePathFromUTF8(path));
  return PlatformFileErrorToPepperError(result);
}

int32_t QueryModuleLocalFile(PP_Instance pp_instance,
                             const char* path,
                             PP_FileInfo_Dev* info) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  base::PlatformFileInfo file_info;
  base::PlatformFileError result = instance->delegate()->QueryModuleLocalFile(
      instance->module()->name(), GetFilePathFromUTF8(path), &file_info);
  if (result == base::PLATFORM_FILE_OK) {
    info->size = file_info.size;
    info->creation_time = file_info.creation_time.ToDoubleT();
    info->last_access_time = file_info.last_accessed.ToDoubleT();
    info->last_modified_time = file_info.last_modified.ToDoubleT();
    info->system_type = PP_FILESYSTEMTYPE_EXTERNAL;
    if (file_info.is_directory)
      info->type = PP_FILETYPE_DIRECTORY;
    else
      info->type = PP_FILETYPE_REGULAR;
  }
  return PlatformFileErrorToPepperError(result);
}

int32_t GetModuleLocalDirContents(PP_Instance pp_instance,
                                  const char* path,
                                  PP_DirContents_Dev** contents) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_ERROR_FAILED;

  *contents = NULL;
  DirContents pepper_contents;
  base::PlatformFileError result =
      instance->delegate()->GetModuleLocalDirContents(
          instance->module()->name(),
          GetFilePathFromUTF8(path),
          &pepper_contents);

  if (result != base::PLATFORM_FILE_OK)
    return PlatformFileErrorToPepperError(result);

  *contents = new PP_DirContents_Dev;
  size_t count = pepper_contents.size();
  (*contents)->count = count;
  (*contents)->entries = new PP_DirEntry_Dev[count];
  for (size_t i = 0; i < count; ++i) {
    PP_DirEntry_Dev& entry = (*contents)->entries[i];
#if defined(OS_WIN)
    const std::string& name = UTF16ToUTF8(pepper_contents[i].name.value());
#else
    const std::string& name = pepper_contents[i].name.value();
#endif
    size_t size = name.size() + 1;
    char* name_copy = new char[size];
    memcpy(name_copy, name.c_str(), size);
    entry.name = name_copy;
    entry.is_dir = BoolToPPBool(pepper_contents[i].is_dir);
  }
  return PP_OK;
}

void FreeModuleLocalDirContents(PP_Instance instance,
                                PP_DirContents_Dev* contents) {
  DCHECK(contents);
  for (int32_t i = 0; i < contents->count; ++i) {
    delete [] contents->entries[i].name;
  }
  delete [] contents->entries;
  delete contents;
}

PP_Bool NavigateToURL(PP_Instance pp_instance,
                      const char* url,
                      const char* target) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return PP_FALSE;
  return BoolToPPBool(instance->NavigateToURL(url, target));
}

void RunMessageLoop() {
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void QuitMessageLoop() {
  MessageLoop::current()->QuitNow();
}

const PPB_Flash ppb_flash = {
  &SetInstanceAlwaysOnTop,
  &PPB_Flash_Impl::DrawGlyphs,
  &GetProxyForURL,
  &OpenModuleLocalFile,
  &RenameModuleLocalFile,
  &DeleteModuleLocalFileOrDir,
  &CreateModuleLocalDir,
  &QueryModuleLocalFile,
  &GetModuleLocalDirContents,
  &FreeModuleLocalDirContents,
  &NavigateToURL,
  &RunMessageLoop,
  &QuitMessageLoop,
};

}  // namespace

// static
const PPB_Flash* PPB_Flash_Impl::GetInterface() {
  return &ppb_flash;
}

// PPB_Flash_NetConnector_Impl -------------------------------------------------

namespace {

PP_Resource Create(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Flash_NetConnector_Impl> connector(
      new PPB_Flash_NetConnector_Impl(instance));
  return connector->GetReference();
}

PP_Bool IsFlashNetConnector(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Flash_NetConnector_Impl>(resource));
}

int32_t ConnectTcp(PP_Resource connector_id,
                   const char* host,
                   uint16_t port,
                   PP_FileHandle* socket_out,
                   PP_Flash_NetAddress* local_addr_out,
                   PP_Flash_NetAddress* remote_addr_out,
                   PP_CompletionCallback callback) {
  scoped_refptr<PPB_Flash_NetConnector_Impl> connector(
      Resource::GetAs<PPB_Flash_NetConnector_Impl>(connector_id));
  if (!connector.get())
    return PP_ERROR_BADRESOURCE;

  return connector->ConnectTcp(
      host, port, socket_out, local_addr_out, remote_addr_out, callback);
}

int32_t ConnectTcpAddress(PP_Resource connector_id,
                          const PP_Flash_NetAddress* addr,
                          PP_FileHandle* socket_out,
                          PP_Flash_NetAddress* local_addr_out,
                          PP_Flash_NetAddress* remote_addr_out,
                          PP_CompletionCallback callback) {
  scoped_refptr<PPB_Flash_NetConnector_Impl> connector(
      Resource::GetAs<PPB_Flash_NetConnector_Impl>(connector_id));
  if (!connector.get())
    return PP_ERROR_BADRESOURCE;

  return connector->ConnectTcpAddress(
      addr, socket_out, local_addr_out, remote_addr_out, callback);
}

const PPB_Flash_NetConnector ppb_flash_netconnector = {
  &Create,
  &IsFlashNetConnector,
  &ConnectTcp,
  &ConnectTcpAddress,
};

}  // namespace

PPB_Flash_NetConnector_Impl::PPB_Flash_NetConnector_Impl(
    PluginInstance* instance)
    : Resource(instance) {
}

PPB_Flash_NetConnector_Impl::~PPB_Flash_NetConnector_Impl() {
}

// static
const PPB_Flash_NetConnector* PPB_Flash_NetConnector_Impl::GetInterface() {
  return &ppb_flash_netconnector;
}

PPB_Flash_NetConnector_Impl*
    PPB_Flash_NetConnector_Impl::AsPPB_Flash_NetConnector_Impl() {
  return this;
}

int32_t PPB_Flash_NetConnector_Impl::ConnectTcp(
    const char* host,
    uint16_t port,
    PP_FileHandle* socket_out,
    PP_Flash_NetAddress* local_addr_out,
    PP_Flash_NetAddress* remote_addr_out,
    PP_CompletionCallback callback) {
  // |socket_out| is not optional.
  if (!socket_out)
    return PP_ERROR_BADARGUMENT;

  if (!callback.func) {
    NOTIMPLEMENTED();
    return PP_ERROR_BADARGUMENT;
  }

  if (callback_.get() && !callback_->completed())
    return PP_ERROR_INPROGRESS;

  PP_Resource resource_id = GetReferenceNoAddRef();
  if (!resource_id) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }

  int32_t rv = instance()->delegate()->ConnectTcp(this, host, port);
  if (rv == PP_ERROR_WOULDBLOCK) {
    // Record callback and output buffers.
    callback_ = new TrackedCompletionCallback(
        instance()->module()->GetCallbackTracker(), resource_id, callback);
    socket_out_ = socket_out;
    local_addr_out_ = local_addr_out;
    remote_addr_out_ = remote_addr_out;
  } else {
    // This should never be completed synchronously successfully.
    DCHECK_NE(rv, PP_OK);
  }
  return rv;
}

int32_t PPB_Flash_NetConnector_Impl::ConnectTcpAddress(
    const PP_Flash_NetAddress* addr,
    PP_FileHandle* socket_out,
    PP_Flash_NetAddress* local_addr_out,
    PP_Flash_NetAddress* remote_addr_out,
    PP_CompletionCallback callback) {
  // |socket_out| is not optional.
  if (!socket_out)
    return PP_ERROR_BADARGUMENT;

  if (!callback.func) {
    NOTIMPLEMENTED();
    return PP_ERROR_BADARGUMENT;
  }

  if (callback_.get() && !callback_->completed())
    return PP_ERROR_INPROGRESS;

  PP_Resource resource_id = GetReferenceNoAddRef();
  if (!resource_id) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }

  int32_t rv = instance()->delegate()->ConnectTcpAddress(this, addr);
  if (rv == PP_ERROR_WOULDBLOCK) {
    // Record callback and output buffers.
    callback_ = new TrackedCompletionCallback(
        instance()->module()->GetCallbackTracker(), resource_id, callback);
    socket_out_ = socket_out;
    local_addr_out_ = local_addr_out;
    remote_addr_out_ = remote_addr_out;
  } else {
    // This should never be completed synchronously successfully.
    DCHECK_NE(rv, PP_OK);
  }
  return rv;
}

void PPB_Flash_NetConnector_Impl::CompleteConnectTcp(
    PP_FileHandle socket,
    const PP_Flash_NetAddress& local_addr,
    const PP_Flash_NetAddress& remote_addr) {
  int32_t rv = PP_ERROR_ABORTED;
  if (!callback_->aborted()) {
    CHECK(!callback_->completed());

    // Write output data.
    *socket_out_ = socket;
    if (socket != PP_kInvalidFileHandle) {
      if (local_addr_out_)
        *local_addr_out_ = local_addr;
      if (remote_addr_out_)
        *remote_addr_out_ = remote_addr;
      rv = PP_OK;
    } else {
      rv = PP_ERROR_FAILED;
    }
  }

  // Theoretically, the plugin should be allowed to try another |ConnectTcp()|
  // from the callback.
  scoped_refptr<TrackedCompletionCallback> callback;
  callback.swap(callback_);
  // Wipe everything else out for safety.
  socket_out_ = NULL;
  local_addr_out_ = NULL;
  remote_addr_out_ = NULL;

  callback->Run(rv);  // Will complete abortively if necessary.
}

}  // namespace ppapi
}  // namespace webkit
