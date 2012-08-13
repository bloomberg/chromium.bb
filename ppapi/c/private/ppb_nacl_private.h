// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_NACL_UTIL_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_NACL_UTIL_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/pp_file_handle.h"

#define PPB_NACL_PRIVATE_INTERFACE "PPB_NaCl(Private);0.9"

struct PPB_NaCl_Private {
  // This function launches NaCl's sel_ldr process.  On success, the function
  // returns true, otherwise it returns false.  When it returns true, it will
  // write |socket_count| nacl::Handles to imc_handles.  Unless
  // EnableBackgroundSelLdrLaunch is called, this method must be invoked from
  // the main thread.
  PP_Bool (*LaunchSelLdr)(PP_Instance instance,
                          const char* alleged_url,
                          int socket_count,
                          void* imc_handles);

  // This function starts the PPAPI proxy so the nexe can communicate with the
  // browser's renderer process.
  PP_Bool (*StartPpapiProxy)(PP_Instance instance);

  // On POSIX systems, this function returns the file descriptor of
  // /dev/urandom.  On non-POSIX systems, this function returns 0.
  int (*UrandomFD)(void);

  // Whether the Pepper 3D interfaces should be disabled in the NaCl PPAPI
  // proxy. This is so paranoid admins can effectively prevent untrusted shader
  // code to be processed by the graphics stack.
  bool (*Are3DInterfacesDisabled)();

  // Enables the creation of sel_ldr processes from other than the main thread.
  void (*EnableBackgroundSelLdrLaunch)();

  // This is Windows-specific.  This is a replacement for
  // DuplicateHandle() for use inside the Windows sandbox.  Note that
  // we provide this via dependency injection only to avoid the
  // linkage problems that occur because the NaCl plugin is built as a
  // separate DLL/DSO (see
  // http://code.google.com/p/chromium/issues/detail?id=114439#c8).
  // We use void* rather than the Windows HANDLE type to avoid an
  // #ifdef here.  We use int rather than PP_Bool/bool so that this is
  // usable with NaClSetBrokerDuplicateHandleFunc() without further
  // wrapping.
  int (*BrokerDuplicateHandle)(void* source_handle,
                               uint32_t process_id,
                               void** target_handle,
                               uint32_t desired_access,
                               uint32_t options);

  // Returns a read-only file descriptor of a file rooted in the Pnacl
  // component directory, or -1 on error.
  // Do we want this to take a completion callback and be async, or
  // or could we make this happen on another thread?
  PP_FileHandle (*GetReadonlyPnaclFd)(const char* filename);

  // This creates a temporary file that will be deleted by the time
  // the last handle is closed (or earlier on POSIX systems), and
  // returns a posix handle to that temporary file.
  PP_FileHandle (*CreateTemporaryFile)(PP_Instance instance);

  // Return true if we are off the record.
  PP_Bool (*IsOffTheRecord)(void);

  // Return true if PNaCl is turned on.
  PP_Bool (*IsPnaclEnabled)(void);
};

#endif  // PPAPI_C_PRIVATE_PPB_NACL_PRIVATE_H_
