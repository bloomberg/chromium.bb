// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/file_type_conversions.h"

#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/pp_errors.h"

namespace webkit {
namespace ppapi {

int PlatformFileErrorToPepperError(base::PlatformFileError error_code) {
  switch (error_code) {
    case base::PLATFORM_FILE_OK:
      return PP_OK;
    case base::PLATFORM_FILE_ERROR_EXISTS:
      return PP_ERROR_FILEEXISTS;
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return PP_ERROR_FILENOTFOUND;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
    case base::PLATFORM_FILE_ERROR_SECURITY:
      return PP_ERROR_NOACCESS;
    case base::PLATFORM_FILE_ERROR_NO_MEMORY:
      return PP_ERROR_NOMEMORY;
    case base::PLATFORM_FILE_ERROR_NO_SPACE:
      return PP_ERROR_NOSPACE;
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
      return PP_ERROR_FAILED;
    default:
      return PP_ERROR_FAILED;
  }
}

bool PepperFileOpenFlagsToPlatformFileFlags(int32_t pp_open_flags,
                                            int* flags_out) {
  if (!flags_out)
    return false;

  bool pp_read = !!(pp_open_flags & PP_FILEOPENFLAG_READ);
  bool pp_write = !!(pp_open_flags & PP_FILEOPENFLAG_WRITE);
  bool pp_create = !!(pp_open_flags & PP_FILEOPENFLAG_CREATE);
  bool pp_truncate = !!(pp_open_flags & PP_FILEOPENFLAG_TRUNCATE);
  bool pp_exclusive = !!(pp_open_flags & PP_FILEOPENFLAG_EXCLUSIVE);

  int flags = 0;
  if (pp_read)
    flags |= base::PLATFORM_FILE_READ;
  if (pp_write) {
    flags |= base::PLATFORM_FILE_WRITE;
    flags |= base::PLATFORM_FILE_WRITE_ATTRIBUTES;
  }

  if (pp_truncate && !pp_write)
    return false;

  if (pp_create) {
    if (pp_exclusive) {
      flags |= base::PLATFORM_FILE_CREATE;
    } else if (pp_truncate) {
      flags |= base::PLATFORM_FILE_CREATE_ALWAYS;
    } else {
      flags |= base::PLATFORM_FILE_OPEN_ALWAYS;
    }
  } else if (pp_truncate) {
    flags |= base::PLATFORM_FILE_OPEN_TRUNCATED;
  } else {
    flags |= base::PLATFORM_FILE_OPEN;
  }

  *flags_out = flags;
  return true;
}

}  // namespace ppapi
}  // namespace webkit

