/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_directory_reader_dev.idl modified Fri Feb 15 16:46:46 2013. */

#ifndef PPAPI_C_DEV_PPB_DIRECTORY_READER_DEV_H_
#define PPAPI_C_DEV_PPB_DIRECTORY_READER_DEV_H_

#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_DIRECTORYREADER_DEV_INTERFACE_0_6 "PPB_DirectoryReader(Dev);0.6"
#define PPB_DIRECTORYREADER_DEV_INTERFACE PPB_DIRECTORYREADER_DEV_INTERFACE_0_6

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
struct PPB_DirectoryReader_Dev_0_6 {
  /* Creates a DirectoryReader for the given directory.  Upon success, the
   * corresponding directory is classified as "in use" by the resulting
   * DirectoryReader object until such time as the DirectoryReader object is
   * destroyed. */
  PP_Resource (*Create)(PP_Resource directory_ref);
  /* Returns PP_TRUE if the given resource is a DirectoryReader. Returns
   * PP_FALSE if the resource is invalid or some type other than a
   * DirectoryReader. */
  PP_Bool (*IsDirectoryReader)(PP_Resource resource);
  /* Reads all entries in the directory.
   *
   * @param[in] directory_reader A <code>PP_Resource</code>
   * corresponding to a directory reader resource.
   * @param[in] output An output array which will receive
   * <code>PP_DirectoryEntry_Dev</code> objects on success.
   * @param[in] callback A <code>PP_CompletionCallback</code> to run on
   * completion.
   *
   * @return An error code from <code>pp_errors.h</code>.
   */
  int32_t (*ReadEntries)(PP_Resource directory_reader,
                         struct PP_ArrayOutput output,
                         struct PP_CompletionCallback callback);
};

typedef struct PPB_DirectoryReader_Dev_0_6 PPB_DirectoryReader_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_DIRECTORY_READER_DEV_H_ */

