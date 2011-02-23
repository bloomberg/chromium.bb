// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_H_

#ifdef _WIN32
#include <windows.h>
#endif

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

// PPB_Flash -------------------------------------------------------------------

#define PPB_FLASH_INTERFACE "PPB_Flash;6"

#ifdef _WIN32
typedef HANDLE PP_FileHandle;
static const PP_FileHandle PP_kInvalidFileHandle = NULL;
#else
typedef int PP_FileHandle;
static const PP_FileHandle PP_kInvalidFileHandle = -1;
#endif

struct PP_CompletionCallback;
struct PP_FontDescription_Dev;
struct PP_FileInfo_Dev;

struct PP_DirEntry_Dev {
  const char* name;
  PP_Bool is_dir;
};

struct PP_DirContents_Dev {
  int32_t count;
  struct PP_DirEntry_Dev* entries;
};

struct PPB_Flash {
  // Sets or clears the rendering hint that the given plugin instance is always
  // on top of page content. Somewhat more optimized painting can be used in
  // this case.
  void (*SetInstanceAlwaysOnTop)(PP_Instance instance, PP_Bool on_top);

  PP_Bool (*DrawGlyphs)(PP_Instance instance,
                        PP_Resource pp_image_data,
                        const struct PP_FontDescription_Dev* font_desc,
                        uint32_t color,
                        struct PP_Point position,
                        struct PP_Rect clip,
                        const float transformation[3][3],
                        uint32_t glyph_count,
                        const uint16_t glyph_indices[],
                        const struct PP_Point glyph_advances[]);

  // Retrieves the proxy that will be used for the given URL. The result will
  // be a string in PAC format, or an undefined var on error.
  struct PP_Var (*GetProxyForURL)(PP_Instance instance, const char* url);

  // Opens a module-local file, returning a file descriptor (posix) or a HANDLE
  // (win32) into file. Module-local file paths (here and below) are
  // '/'-separated UTF-8 strings, relative to a module-specific root. The return
  // value is the ppapi error, PP_OK if success, one of the PP_ERROR_* in case
  // of failure.
  int32_t (*OpenModuleLocalFile)(PP_Instance instance,
                                 const char* path,
                                 int32_t mode,
                                 PP_FileHandle* file);

  // Renames a module-local file. The return value is the ppapi error, PP_OK if
  // success, one of the PP_ERROR_* in case of failure.
  int32_t (*RenameModuleLocalFile)(PP_Instance instance,
                                   const char* path_from,
                                   const char* path_to);

  // Deletes a module-local file or directory. If recursive is set and the path
  // points to a directory, deletes all the contents of the directory. The
  // return value is the ppapi error, PP_OK if success, one of the PP_ERROR_* in
  // case of failure.
  int32_t (*DeleteModuleLocalFileOrDir)(PP_Instance instance,
                                        const char* path,
                                        PP_Bool recursive);

  // Creates a module-local directory. The return value is the ppapi error,
  // PP_OK if success, one of the PP_ERROR_* in case of failure.
  int32_t (*CreateModuleLocalDir)(PP_Instance instance, const char* path);

  // Queries information about a module-local file. The return value is the
  // ppapi error, PP_OK if success, one of the PP_ERROR_* in case of failure.
  int32_t (*QueryModuleLocalFile)(PP_Instance instance,
                                  const char* path,
                                  struct PP_FileInfo_Dev* info);

  // Gets the list of files contained in a module-local directory. The return
  // value is the ppapi error, PP_OK if success, one of the PP_ERROR_* in case
  // of failure. If non-NULL, the returned contents should be freed with
  // FreeModuleLocalDirContents.
  int32_t (*GetModuleLocalDirContents)(PP_Instance instance,
                                       const char* path,
                                       struct PP_DirContents_Dev** contents);

  // Frees the data allocated by GetModuleLocalDirContents.
  void (*FreeModuleLocalDirContents)(PP_Instance instance,
                                     struct PP_DirContents_Dev* contents);

  // Navigate to URL. May open a new tab if target is not "_self". Return true
  // if success. This differs from javascript:window.open() in that it bypasses
  // the popup blocker, even when this is not called from an event handler.
  PP_Bool (*NavigateToURL)(PP_Instance instance,
                           const char* url,
                           const char* target);

  // Runs a nested message loop. The plugin will be reentered from this call.
  // This function is used in places where Flash would normally enter a nested
  // message loop (e.g., when displaying context menus), but Pepper provides
  // only an asynchronous call. After performing that asynchronous call, call
  // |RunMessageLoop()|. In the callback, call |QuitMessageLoop()|.
  void (*RunMessageLoop)(PP_Instance instance);

  // Posts a quit message for the outermost nested message loop. Use this to
  // exit and return back to the caller after you call RunMessageLoop.
  void (*QuitMessageLoop)(PP_Instance instance);
};

// PPB_Flash_NetConnector ------------------------------------------------------

#define PPB_FLASH_NETCONNECTOR_INTERFACE "PPB_Flash_NetConnector;1"

// This is an opaque type holding a network address.
struct PP_Flash_NetAddress {
  size_t size;
  char data[128];
};

struct PPB_Flash_NetConnector {
  PP_Resource (*Create)(PP_Instance instance_id);
  PP_Bool (*IsFlashNetConnector)(PP_Resource resource_id);

  // Connect to a TCP port given as a host-port pair. The local and remote
  // addresses of the connection (if successful) are returned in
  // |local_addr_out| and |remote_addr_out|, respectively, if non-null.
  int32_t (*ConnectTcp)(PP_Resource connector_id,
                        const char* host,
                        uint16_t port,
                        PP_FileHandle* socket_out,
                        struct PP_Flash_NetAddress* local_addr_out,
                        struct PP_Flash_NetAddress* remote_addr_out,
                        struct PP_CompletionCallback callback);

  // Same as |ConnectTcp()|, but connecting to the address given by |addr|. A
  // typical use-case would be for reconnections.
  int32_t (*ConnectTcpAddress)(PP_Resource connector_id,
                               const struct PP_Flash_NetAddress* addr,
                               PP_FileHandle* socket_out,
                               struct PP_Flash_NetAddress* local_addr_out,
                               struct PP_Flash_NetAddress* remote_addr_out,
                               struct PP_CompletionCallback callback);
};

#endif  // PPAPI_C_PRIVATE_PPB_FLASH_H_
