/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_FILE_INFO_H_
#define PPAPI_C_PP_FILE_INFO_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

typedef enum {
  PP_FILETYPE_REGULAR,
  PP_FILETYPE_DIRECTORY,
  PP_FILETYPE_OTHER  /* A catch-all for unidentified types. */
} PP_FileType;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FileType, 4);

typedef enum {
  PP_FILESYSTEMTYPE_INVALID = 0,  /* For identifying invalid return values. */
  PP_FILESYSTEMTYPE_EXTERNAL,
  PP_FILESYSTEMTYPE_LOCALPERSISTENT,
  PP_FILESYSTEMTYPE_LOCALTEMPORARY
} PP_FileSystemType;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FileSystemType, 4);

struct PP_FileInfo {
  int64_t size;  /* Measured in bytes */
  PP_FileType type;
  PP_FileSystemType system_type;
  PP_Time creation_time;
  PP_Time last_access_time;
  PP_Time last_modified_time;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_FileInfo, 40);

#endif  /* PPAPI_C_PP_FILE_INFO_H_ */
