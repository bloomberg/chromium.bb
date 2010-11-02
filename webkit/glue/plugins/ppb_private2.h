// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PPB_PRIVATE2_H_
#define WEBKIT_GLUE_PLUGINS_PPB_PRIVATE2_H_

#ifdef _WIN32
#include <windows.h>
#endif

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

#define PPB_PRIVATE2_INTERFACE "PPB_Private2;4"

#ifdef _WIN32
typedef HANDLE PP_FileHandle;
static const PP_FileHandle PP_kInvalidFileHandle = NULL;
#else
typedef int PP_FileHandle;
static const PP_FileHandle PP_kInvalidFileHandle = -1;
#endif

struct PP_FontDescription_Dev;
struct PP_FileInfo_Dev;

struct PP_DirEntry_Dev {
  const char* name;
  bool is_dir;
};

struct PP_DirContents_Dev {
  int32_t count;
  PP_DirEntry_Dev* entries;
};

struct PPB_Private2 {
  // Sets or clears the rendering hint that the given plugin instance is always
  // on top of page content. Somewhat more optimized painting can be used in
  // this case.
  void (*SetInstanceAlwaysOnTop)(PP_Instance instance, bool on_top);

  bool (*DrawGlyphs)(PP_Resource pp_image_data,
                     const PP_FontDescription_Dev* font_desc,
                     uint32_t color,
                     PP_Point position,
                     PP_Rect clip,
                     float transformation[3][3],
                     uint32_t glyph_count,
                     uint16_t glyph_indices[],
                     PP_Point glyph_advances[]);

  // Retrieves the proxy that will be used for the given URL. The result will
  // be a string in PAC format, or an undefined var on error.
  PP_Var (*GetProxyForURL)(PP_Module module, const char* url);

  // Opens a module-local file, returning a file descriptor (posix) or a HANDLE
  // (win32) into file. Module-local file paths (here and below) are
  // '/'-separated UTF-8 strings, relative to a module-specific root. The return
  // value is the ppapi error, PP_OK if success, one of the PP_ERROR_* in case
  // of failure.
  int32_t (*OpenModuleLocalFile)(PP_Module module,
                                 const char* path,
                                 int32_t mode,
                                 PP_FileHandle* file);

  // Renames a module-local file. The return value is the ppapi error, PP_OK if
  // success, one of the PP_ERROR_* in case of failure.
  int32_t (*RenameModuleLocalFile)(PP_Module module,
                                   const char* path_from,
                                   const char* path_to);

  // Deletes a module-local file or directory. If recursive is set and the path
  // points to a directory, deletes all the contents of the directory. The
  // return value is the ppapi error, PP_OK if success, one of the PP_ERROR_* in
  // case of failure.
  int32_t (*DeleteModuleLocalFileOrDir)(PP_Module module,
                                        const char* path,
                                        bool recursive);

  // Creates a module-local directory. The return value is the ppapi error,
  // PP_OK if success, one of the PP_ERROR_* in case of failure.
  int32_t (*CreateModuleLocalDir)(PP_Module module, const char* path);

  // Queries information about a module-local file. The return value is the
  // ppapi error, PP_OK if success, one of the PP_ERROR_* in case of failure.
  int32_t (*QueryModuleLocalFile)(PP_Module module,
                                  const char* path,
                                  PP_FileInfo_Dev* info);

  // Gets the list of files contained in a module-local directory. The return
  // value is the ppapi error, PP_OK if success, one of the PP_ERROR_* in case
  // of failure. If non-NULL, the returned contents should be freed with
  // FreeModuleLocalDirContents.
  int32_t (*GetModuleLocalDirContents)(PP_Module module,
                                       const char* path,
                                       PP_DirContents_Dev** contents);

  // Frees the data allocated by GetModuleLocalDirContents.
  void (*FreeModuleLocalDirContents)(PP_Module module,
                                     PP_DirContents_Dev* contents);

  // Navigate to URL. May open a new tab if target is not "_self". Return true
  // if success. This differs from javascript:window.open() in that it bypasses
  // the popup blocker, even when this is not called from an event handler.
  bool (*NavigateToURL)(PP_Instance instance,
                        const char* url,
                        const char* target);
};

#endif  // WEBKIT_GLUE_PLUGINS_PPB_PRIVATE2_H_
