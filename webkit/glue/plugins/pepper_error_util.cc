// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_error_util.h"

#include "ppapi/c/pp_errors.h"

namespace pepper {

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

}  // namespace pepper
