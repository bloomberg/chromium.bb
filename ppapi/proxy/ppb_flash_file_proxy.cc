// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_file_proxy.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

// Given an error code and a handle result from a Pepper API call, converts to a
// PlatformFileForTransit by sharing with the other side, closing the original
// handle, possibly also updating the error value if an error occurred.
IPC::PlatformFileForTransit PlatformFileToPlatformFileForTransit(
    Dispatcher* dispatcher,
    int32_t* error,
    base::PlatformFile file) {
  if (*error != PP_OK)
    return IPC::InvalidPlatformFileForTransit();
  IPC::PlatformFileForTransit out_handle =
      dispatcher->ShareHandleWithRemote(file, true);
  if (out_handle == IPC::InvalidPlatformFileForTransit())
    *error = PP_ERROR_NOACCESS;
  return out_handle;
}

void FreeDirContents(PP_Instance /* instance */,
                     PP_DirContents_Dev* contents) {
  for (int32_t i = 0; i < contents->count; ++i)
    delete[] contents->entries[i].name;
  delete[] contents->entries;
  delete contents;
}

}  // namespace

// PPB_Flash_File_ModuleLocal --------------------------------------------------

namespace {

int32_t OpenModuleLocalFile(PP_Instance instance,
                            const char* path,
                            int32_t mode,
                            PP_FileHandle* file) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  IPC::PlatformFileForTransit transit;
  dispatcher->Send(new PpapiHostMsg_PPBFlashFile_ModuleLocal_OpenFile(
      INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL,
      instance, path, mode, &transit, &result));
  *file = IPC::PlatformFileForTransitToPlatformFile(transit);
  return result;
}

int32_t RenameModuleLocalFile(PP_Instance instance,
                              const char* from_path,
                              const char* to_path) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  dispatcher->Send(new PpapiHostMsg_PPBFlashFile_ModuleLocal_RenameFile(
      INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL,
      instance, from_path, to_path, &result));
  return result;
}

int32_t DeleteModuleLocalFileOrDir(PP_Instance instance,
                                   const char* path,
                                   PP_Bool recursive) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  dispatcher->Send(new PpapiHostMsg_PPBFlashFile_ModuleLocal_DeleteFileOrDir(
      INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL,
      instance, path, recursive, &result));
  return result;
}

int32_t CreateModuleLocalDir(PP_Instance instance, const char* path) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  dispatcher->Send(new PpapiHostMsg_PPBFlashFile_ModuleLocal_CreateDir(
      INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL, instance, path, &result));
  return result;
}

int32_t QueryModuleLocalFile(PP_Instance instance,
                             const char* path,
                             PP_FileInfo_Dev* info) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  dispatcher->Send(new PpapiHostMsg_PPBFlashFile_ModuleLocal_QueryFile(
      INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL, instance, path, info, &result));
  return result;
}

int32_t GetModuleLocalDirContents(PP_Instance instance,
                                  const char* path,
                                  PP_DirContents_Dev** contents) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  std::vector<SerializedDirEntry> entries;
  dispatcher->Send(new PpapiHostMsg_PPBFlashFile_ModuleLocal_GetDirContents(
      INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL,
      instance, path, &entries, &result));

  if (result != PP_OK)
    return result;

  // Copy the serialized dir entries to the output struct.
  *contents = new PP_DirContents_Dev;
  (*contents)->count = static_cast<int32_t>(entries.size());
  (*contents)->entries = new PP_DirEntry_Dev[entries.size()];
  for (size_t i = 0; i < entries.size(); i++) {
    const SerializedDirEntry& source = entries[i];
    PP_DirEntry_Dev* dest = &(*contents)->entries[i];

    char* name_copy = new char[source.name.size() + 1];
    memcpy(name_copy, source.name.c_str(), source.name.size() + 1);
    dest->name = name_copy;
    dest->is_dir = BoolToPPBool(source.is_dir);
  }

  return result;
}

const PPB_Flash_File_ModuleLocal flash_file_modulelocal_interface = {
  &OpenModuleLocalFile,
  &RenameModuleLocalFile,
  &DeleteModuleLocalFileOrDir,
  &CreateModuleLocalDir,
  &QueryModuleLocalFile,
  &GetModuleLocalDirContents,
  &FreeDirContents,
};

InterfaceProxy* CreateFlashFileModuleLocalProxy(Dispatcher* dispatcher,
                                                const void* target_interface) {
  return new PPB_Flash_File_ModuleLocal_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Flash_File_ModuleLocal_Proxy::PPB_Flash_File_ModuleLocal_Proxy(
    Dispatcher* dispatcher,
    const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Flash_File_ModuleLocal_Proxy::~PPB_Flash_File_ModuleLocal_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Flash_File_ModuleLocal_Proxy::GetInfo() {
  static const Info info = {
    &flash_file_modulelocal_interface,
    PPB_FLASH_FILE_MODULELOCAL_INTERFACE,
    INTERFACE_ID_PPB_FLASH_FILE_MODULELOCAL,
    true,
    &CreateFlashFileModuleLocalProxy,
  };
  return &info;
}

bool PPB_Flash_File_ModuleLocal_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_File_ModuleLocal_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_OpenFile,
                        OnMsgOpenFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_RenameFile,
                        OnMsgRenameFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_DeleteFileOrDir,
                        OnMsgDeleteFileOrDir)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_CreateDir,
                        OnMsgCreateDir)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_QueryFile,
                        OnMsgQueryFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_ModuleLocal_GetDirContents,
                        OnMsgGetDirContents)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgOpenFile(
    PP_Instance instance,
    const std::string& path,
    int32_t mode,
    IPC::PlatformFileForTransit* file_handle,
    int32_t* result) {
  base::PlatformFile file;
  *result = ppb_flash_file_module_local_target()->
      OpenFile(instance, path.c_str(), mode, &file);
  *file_handle = PlatformFileToPlatformFileForTransit(
      dispatcher(), result, file);
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgRenameFile(
    PP_Instance instance,
    const std::string& from_path,
    const std::string& to_path,
    int32_t* result) {
  *result = ppb_flash_file_module_local_target()->
      RenameFile(instance, from_path.c_str(), to_path.c_str());
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgDeleteFileOrDir(
    PP_Instance instance,
    const std::string& path,
    PP_Bool recursive,
    int32_t* result) {
  *result = ppb_flash_file_module_local_target()->
      DeleteFileOrDir(instance, path.c_str(), recursive);
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgCreateDir(PP_Instance instance,
                                                      const std::string& path,
                                                      int32_t* result) {
  *result = ppb_flash_file_module_local_target()->
      CreateDir(instance, path.c_str());
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgQueryFile(PP_Instance instance,
                                                      const std::string& path,
                                                      PP_FileInfo_Dev* info,
                                                      int32_t* result) {
  *result = ppb_flash_file_module_local_target()->
      QueryFile(instance, path.c_str(), info);
}

void PPB_Flash_File_ModuleLocal_Proxy::OnMsgGetDirContents(
    PP_Instance instance,
    const std::string& path,
    std::vector<pp::proxy::SerializedDirEntry>* entries,
    int32_t* result) {
  PP_DirContents_Dev* contents = NULL;
  *result = ppb_flash_file_module_local_target()->
      GetDirContents(instance, path.c_str(), &contents);
  if (*result != PP_OK)
    return;

  // Convert the list of entries to the serialized version.
  entries->resize(contents->count);
  for (int32_t i = 0; i < contents->count; i++) {
    (*entries)[i].name.assign(contents->entries[i].name);
    (*entries)[i].is_dir = PPBoolToBool(contents->entries[i].is_dir);
  }
  ppb_flash_file_module_local_target()->FreeDirContents(instance, contents);
}

// PPB_Flash_File_FileRef ------------------------------------------------------

namespace {

int32_t OpenFileRefFile(PP_Resource file_ref_id,
                        int32_t mode,
                        PP_FileHandle* file) {
  PluginResource* file_ref =
      PluginResourceTracker::GetInstance()->GetResourceObject(file_ref_id);
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  PluginDispatcher* dispatcher =
      PluginDispatcher::GetForInstance(file_ref->instance());
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  IPC::PlatformFileForTransit transit;
  dispatcher->Send(new PpapiHostMsg_PPBFlashFile_FileRef_OpenFile(
      INTERFACE_ID_PPB_FLASH_FILE_FILEREF,
      file_ref->host_resource(), mode, &transit, &result));
  *file = IPC::PlatformFileForTransitToPlatformFile(transit);
  return result;
}

int32_t QueryFileRefFile(PP_Resource file_ref_id,
                         PP_FileInfo_Dev* info) {
  PluginResource* file_ref =
      PluginResourceTracker::GetInstance()->GetResourceObject(file_ref_id);
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  PluginDispatcher* dispatcher =
      PluginDispatcher::GetForInstance(file_ref->instance());
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  int32_t result = PP_ERROR_FAILED;
  dispatcher->Send(new PpapiHostMsg_PPBFlashFile_FileRef_QueryFile(
      INTERFACE_ID_PPB_FLASH_FILE_FILEREF,
      file_ref->host_resource(), info, &result));
  return result;
}

const PPB_Flash_File_FileRef flash_file_fileref_interface = {
  &OpenFileRefFile,
  &QueryFileRefFile,
};

InterfaceProxy* CreateFlashFileFileRefProxy(Dispatcher* dispatcher,
                                            const void* target_interface) {
  return new PPB_Flash_File_FileRef_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Flash_File_FileRef_Proxy::PPB_Flash_File_FileRef_Proxy(
    Dispatcher* dispatcher,
    const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Flash_File_FileRef_Proxy::~PPB_Flash_File_FileRef_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Flash_File_FileRef_Proxy::GetInfo() {
  static const Info info = {
    &flash_file_fileref_interface,
    PPB_FLASH_FILE_FILEREF_INTERFACE,
    INTERFACE_ID_PPB_FLASH_FILE_FILEREF,
    true,
    &CreateFlashFileFileRefProxy,
  };
  return &info;
}

bool PPB_Flash_File_FileRef_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_File_FileRef_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_FileRef_OpenFile,
                        OnMsgOpenFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlashFile_FileRef_QueryFile,
                        OnMsgQueryFile)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Flash_File_FileRef_Proxy::OnMsgOpenFile(
    const HostResource& host_resource,
    int32_t mode,
    IPC::PlatformFileForTransit* file_handle,
    int32_t* result) {
  base::PlatformFile file;
  *result = ppb_flash_file_module_local_target()->
      OpenFile(host_resource.host_resource(), mode, &file);
  *file_handle = PlatformFileToPlatformFileForTransit(
      dispatcher(), result, file);
}

void PPB_Flash_File_FileRef_Proxy::OnMsgQueryFile(
    const HostResource& host_resource,
    PP_FileInfo_Dev* info,
    int32_t* result) {
  *result = ppb_flash_file_module_local_target()->
      QueryFile(host_resource.host_resource(), info);
}

}  // namespace proxy
}  // namespace pp
