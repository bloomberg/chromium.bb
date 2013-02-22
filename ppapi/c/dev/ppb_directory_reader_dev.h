/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_directory_reader_dev.idl modified Mon Nov 26 13:52:22 2012. */

#ifndef PPAPI_C_DEV_PPB_DIRECTORY_READER_DEV_H_
#define PPAPI_C_DEV_PPB_DIRECTORY_READER_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_DIRECTORYREADER_DEV_INTERFACE_0_5 "PPB_DirectoryReader(Dev);0.5"
#define PPB_DIRECTORYREADER_DEV_INTERFACE PPB_DIRECTORYREADER_DEV_INTERFACE_0_5

/**
 * @file
 *
 * This file defines the <code>PPB_DirectoryReader_Dev</code> interface.
 */


/**
 * @addtogroup Structs
 * @{
 */
struct PP_DirectoryEntry_Dev {
  PP_Resource file_ref;
  PP_FileType file_type;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DirectoryEntry_Dev, 8);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_DirectoryReader_Dev_0_5 {
  /* Creates a DirectoryReader for the given directory.  Upon success, the
   * corresponding directory is classified as "in use" by the resulting
   * DirectoryReader object until such time as the DirectoryReader object is
   * destroyed. */
  PP_Resource (*Create)(PP_Resource directory_ref);
  /* Returns PP_TRUE if the given resource is a DirectoryReader. Returns
   * PP_FALSE if the resource is invalid or some type other than a
   * DirectoryReader. */
  PP_Bool (*IsDirectoryReader)(PP_Resource resource);
  /* Reads the next entry in the directory.  Returns PP_OK and sets
   * entry->file_ref to 0 to indicate reaching the end of the directory.  If
   * entry->file_ref is non-zero when passed to GetNextEntry, it will be
   * released before the next file_ref is stored.
   *
   * EXAMPLE USAGE:
   *
   *   PP_Resource reader = reader_funcs->Create(dir_ref);
   *   PP_DirectoryEntry entry = {0};
   *   while ((reader_funcs->GetNextEntry(reader, &entry,
   *                                      PP_BlockUntilComplete()) == PP_OK) &&
   *          entry->file_ref) {
   *     ProcessDirectoryEntry(entry);
   *   }
   *   core_funcs->ReleaseResource(reader);
   */
  int32_t (*GetNextEntry)(PP_Resource directory_reader,
                          struct PP_DirectoryEntry_Dev* entry,
                          struct PP_CompletionCallback callback);
};

typedef struct PPB_DirectoryReader_Dev_0_5 PPB_DirectoryReader_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_DIRECTORY_READER_DEV_H_ */

