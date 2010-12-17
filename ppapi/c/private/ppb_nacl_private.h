// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_NACL_UTIL_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_NACL_UTIL_PRIVATE_H_

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_NACL_PRIVATE_INTERFACE "PPB_NaCl(Private);0.1"

struct PPB_NaCl_Private {
  // This function launches NaCl's sel_ldr process.  On success, the function
  // returns true, otherwise it returns false.  When it returns true, it will
  // write |socket_count| nacl::Handles to imc_handles and will write the
  // nacl::Handle of the created process to |nacl_process_handle|.  Finally,
  // the function will write the process ID of the created process to
  // |nacl_process_id|.
  bool (*LaunchSelLdr)(const char* alleged_url, int socket_count,
                       void* imc_handles, void* nacl_process_handle,
                       int* nacl_process_id);

  // On POSIX systems, this function returns the file descriptor of
  // /dev/urandom.  On non-POSIX systems, this function returns 0.
  int (*UrandomFD)(void);
};

#endif  // PPAPI_C_PRIVATE_PPB_NACL_PRIVATE_H_
