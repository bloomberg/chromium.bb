// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/dir_contents.h"
#include "ppapi/shared_impl/file_path.h"

// Singly-included section since need custom serialization.
#ifndef PPAPI_SHARED_IMPL_PEPPER_FILE_MESSAGES_H_
#define PPAPI_SHARED_IMPL_PEPPER_FILE_MESSAGES_H_

namespace IPC {

template <>
struct ParamTraits<ppapi::PepperFilePath> {
  typedef ppapi::PepperFilePath param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // PPAPI_SHARED_IMPL_PEPPER_FILE_MESSAGES_H_

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT PPAPI_PROXY_EXPORT

#define IPC_MESSAGE_START PepperFileMsgStart

IPC_STRUCT_TRAITS_BEGIN(ppapi::DirEntry)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(is_dir)
IPC_STRUCT_TRAITS_END()

// Trusted Pepper Filesystem messages from the renderer to the browser.

// Open the file.
IPC_SYNC_MESSAGE_CONTROL2_2(PepperFileMsg_OpenFile,
                            ppapi::PepperFilePath /* path */,
                            int /* flags */,
                            base::PlatformFileError /* error_code */,
                            IPC::PlatformFileForTransit /* result */)

// Rename the file.
IPC_SYNC_MESSAGE_CONTROL2_1(PepperFileMsg_RenameFile,
                            ppapi::PepperFilePath /* from_path */,
                            ppapi::PepperFilePath /* to_path */,
                            base::PlatformFileError /* error_code */)

// Delete the file.
IPC_SYNC_MESSAGE_CONTROL2_1(PepperFileMsg_DeleteFileOrDir,
                            ppapi::PepperFilePath /* path */,
                            bool /* recursive */,
                            base::PlatformFileError /* error_code */)

// Create the directory.
IPC_SYNC_MESSAGE_CONTROL1_1(PepperFileMsg_CreateDir,
                            ppapi::PepperFilePath /* path */,
                            base::PlatformFileError /* error_code */)

// Query the file's info.
IPC_SYNC_MESSAGE_CONTROL1_2(PepperFileMsg_QueryFile,
                            ppapi::PepperFilePath /* path */,
                            base::PlatformFileInfo, /* info */
                            base::PlatformFileError /* error_code */)

// Get the directory's contents.
IPC_SYNC_MESSAGE_CONTROL1_2(PepperFileMsg_GetDirContents,
                            ppapi::PepperFilePath /* path */,
                            ppapi::DirContents, /* contents */
                            base::PlatformFileError /* error_code */)

// Create a temporary file.
IPC_SYNC_MESSAGE_CONTROL0_2(PepperFileMsg_CreateTemporaryFile,
                            base::PlatformFileError /* error_code */,
                            IPC::PlatformFileForTransit /* file */)
