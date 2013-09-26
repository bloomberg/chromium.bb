/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From trusted/ppb_file_io_trusted.idl modified Wed Sep 25 09:30:40 2013. */

#ifndef PPAPI_C_TRUSTED_PPB_FILE_IO_TRUSTED_H_
#define PPAPI_C_TRUSTED_PPB_FILE_IO_TRUSTED_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FILEIOTRUSTED_INTERFACE_0_4 "PPB_FileIOTrusted;0.4"
#define PPB_FILEIOTRUSTED_INTERFACE PPB_FILEIOTRUSTED_INTERFACE_0_4

/**
 * @file
 *
 * This file defines the trusted file IO interface
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/* Available only to trusted implementations. */
struct PPB_FileIOTrusted_0_4 {
  /**
   * Returns a file descriptor corresponding to the given FileIO object. On
   * Windows, returns a HANDLE; on all other platforms, returns a POSIX file
   * descriptor. The FileIO object must have been opened with a successful
   * call to FileIO::Open.  The file descriptor will be closed automatically
   * when the FileIO object is closed or destroyed.
   *
   * TODO(hamaji): Remove this and use RequestOSFileHandle instead.
   */
  int32_t (*GetOSFileDescriptor)(PP_Resource file_io);
};

typedef struct PPB_FileIOTrusted_0_4 PPB_FileIOTrusted;
/**
 * @}
 */

#endif  /* PPAPI_C_TRUSTED_PPB_FILE_IO_TRUSTED_H_ */

