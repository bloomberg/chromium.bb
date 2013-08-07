/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_nacl_private.idl modified Tue Aug  6 11:51:26 2013. */

#ifndef PPAPI_C_PRIVATE_PPB_NACL_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_NACL_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_instance_private.h"

#define PPB_NACL_PRIVATE_INTERFACE_1_0 "PPB_NaCl_Private;1.0"
#define PPB_NACL_PRIVATE_INTERFACE PPB_NACL_PRIVATE_INTERFACE_1_0

/**
 * @file
 * This file contains NaCl private interfaces. This interface is not versioned
 * and is for internal Chrome use. It may change without notice. */


#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/c/private/ppb_instance_private.h"

/**
 * @addtogroup Enums
 * @{
 */
/** NaCl-specific errors that should be reported to the user */
typedef enum {
  /**
   *  The manifest program element does not contain a program usable on the
   *  user's architecture
   */
  PP_NACL_MANIFEST_MISSING_ARCH = 0
} PP_NaClError;
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/* PPB_NaCl_Private */
struct PPB_NaCl_Private_1_0 {
  /* Launches NaCl's sel_ldr process.  Returns PP_EXTERNAL_PLUGIN_OK on success
   * and writes a NaClHandle to imc_handle. Returns PP_EXTERNAL_PLUGIN_FAILED on
   * failure. The |enable_ppapi_dev| parameter controls whether GetInterface
   * returns 'Dev' interfaces to the NaCl plugin.  The |uses_ppapi| flag
   * indicates that the nexe run by sel_ldr will use the PPAPI APIs.
   * This implies that LaunchSelLdr is run from the main thread.  If a nexe
   * does not need PPAPI, then it can run off the main thread.
   * The |uses_irt| flag indicates whether the IRT should be loaded in this
   * NaCl process.  This is true for ABI stable nexes.
   * The |enable_dyncode_syscalls| flag indicates whether or not the nexe
   * will be able to use dynamic code system calls (e.g., mmap with PROT_EXEC).
   * The |enable_exception_handling| flag indicates whether or not the nexe
   * will be able to use hardware exception handling.
   */
  PP_ExternalPluginResult (*LaunchSelLdr)(PP_Instance instance,
                                          const char* alleged_url,
                                          PP_Bool uses_irt,
                                          PP_Bool uses_ppapi,
                                          PP_Bool enable_ppapi_dev,
                                          PP_Bool enable_dyncode_syscalls,
                                          PP_Bool enable_exception_handling,
                                          void* imc_handle,
                                          struct PP_Var* error_message);
  /* This function starts the IPC proxy so the nexe can communicate with the
   * browser. Returns PP_EXTERNAL_PLUGIN_OK on success, otherwise a result code
   * indicating the failure. PP_EXTERNAL_PLUGIN_FAILED is returned if
   * LaunchSelLdr wasn't called with the instance.
   * PP_EXTERNAL_PLUGIN_ERROR_MODULE is returned if the module can't be
   * initialized. PP_EXTERNAL_PLUGIN_ERROR_INSTANCE is returned if the instance
   * can't be initialized.
   */
  PP_ExternalPluginResult (*StartPpapiProxy)(PP_Instance instance);
  /* On POSIX systems, this function returns the file descriptor of
   * /dev/urandom.  On non-POSIX systems, this function returns 0.
   */
  int32_t (*UrandomFD)(void);
  /* Whether the Pepper 3D interfaces should be disabled in the NaCl PPAPI
   * proxy. This is so paranoid admins can effectively prevent untrusted shader
   * code to be processed by the graphics stack.
   */
  PP_Bool (*Are3DInterfacesDisabled)(void);
  /* This is Windows-specific.  This is a replacement for DuplicateHandle() for
   * use inside the Windows sandbox.  Note that we provide this via dependency
   * injection only to avoid the linkage problems that occur because the NaCl
   * plugin is built as a separate DLL/DSO
   * (see http://code.google.com/p/chromium/issues/detail?id=114439#c8).
   */
  int32_t (*BrokerDuplicateHandle)(PP_FileHandle source_handle,
                                   uint32_t process_id,
                                   PP_FileHandle* target_handle,
                                   uint32_t desired_access,
                                   uint32_t options);
  /* Check if PNaCl is installed and attempt to install if necessary.
   * Callback is called when the check is done and PNaCl is already installed,
   * or after an on-demand install is attempted. Called back with PP_OK if
   * PNaCl is available. Called back with an error otherwise.
   */
  int32_t (*EnsurePnaclInstalled)(PP_Instance instance,
                                  struct PP_CompletionCallback callback);
  /* Returns a read-only file descriptor of a file rooted in the Pnacl
   * component directory, or an invalid handle on failure.
   */
  PP_FileHandle (*GetReadonlyPnaclFd)(const char* filename);
  /* This creates a temporary file that will be deleted by the time
   * the last handle is closed (or earlier on POSIX systems), and
   * returns a posix handle to that temporary file.
   */
  PP_FileHandle (*CreateTemporaryFile)(PP_Instance instance);
  /* Create a temporary file, which will be deleted by the time the last
   * handle is closed (or earlier on POSIX systems), to use for the nexe
   * with the cache information given by |pexe_url|, |abi_version|, |opt_level|,
   * |last_modified|, and |etag|. If the nexe is already present
   * in the cache, |is_hit| is set to PP_TRUE and the contents of the nexe
   * will be copied into the temporary file. Otherwise |is_hit| is set to
   * PP_FALSE and the temporary file will be writeable.
   * Currently the implementation is a stub, which always sets is_hit to false
   * and calls the implementation of CreateTemporaryFile. In a subsequent CL
   * it will call into the browser which will remember the association between
   * the cache key and the fd, and copy the nexe into the cache after the
   * translation finishes.
   */
  int32_t (*GetNexeFd)(PP_Instance instance,
                       const char* pexe_url,
                       uint32_t abi_version,
                       uint32_t opt_level,
                       const char* last_modified,
                       const char* etag,
                       PP_Bool* is_hit,
                       PP_FileHandle* nexe_handle,
                       struct PP_CompletionCallback callback);
  /* Report to the browser that translation of the pexe for |instance|
   * has finished, or aborted with an error. If |success| is true, the
   * browser may then store the translation in the cache. The renderer
   * must first have called GetNexeFd for the same instance. (The browser is
   * not guaranteed to store the nexe even if |success| is true; if there is
   * an error on the browser side, or the file is too big for the cache, or
   * the browser is in incognito mode, no notification will be delivered to
   * the plugin.)
   */
  void (*ReportTranslationFinished)(PP_Instance instance, PP_Bool success);
  /* Return true if we are off the record.
   */
  PP_Bool (*IsOffTheRecord)(void);
  /* Return true if PNaCl is turned on.
   */
  PP_Bool (*IsPnaclEnabled)(void);
  /* Display a UI message to the user. */
  PP_ExternalPluginResult (*ReportNaClError)(PP_Instance instance,
                                             PP_NaClError message_id);
  /* Opens a NaCl executable file in the application's extension directory
   * corresponding to the file URL and returns a file descriptor, or an invalid
   * handle on failure. |metadata| is left unchanged on failure.
   */
  PP_FileHandle (*OpenNaClExecutable)(PP_Instance instance,
                                      const char* file_url,
                                      uint64_t* file_token_lo,
                                      uint64_t* file_token_hi);
};

typedef struct PPB_NaCl_Private_1_0 PPB_NaCl_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_NACL_PRIVATE_H_ */

