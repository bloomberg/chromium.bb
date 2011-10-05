/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From trusted/ppb_file_chooser_trusted.idl modified Tue Oct 04 10:48:17 2011. */

#ifndef PPAPI_C_TRUSTED_PPB_FILE_CHOOSER_TRUSTED_H_
#define PPAPI_C_TRUSTED_PPB_FILE_CHOOSER_TRUSTED_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FILECHOOSER_TRUSTED_INTERFACE_0_5 "PPB_FileChooser_Trusted;0.5"
#define PPB_FILECHOOSER_TRUSTED_INTERFACE PPB_FILECHOOSER_TRUSTED_INTERFACE_0_5

/**
 * @file
 * This file defines the <code>PPB_FileChooser_Trusted</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_FileChooser_Trusted {
  /**
   * This function displays a previously created file chooser resource as a
   * dialog box, prompting the user to choose a file or files. The callback is
   * called with PP_OK on successful completion with a file (or files) selected
   * or PP_ERROR_USERCANCEL if the user selected no file.
   *
   * @param[in] chooser The file chooser resource.
   * @param[in] callback A <code>CompletionCallback</code> to be called after
   * the user has closed the file chooser dialog.
   *
   * @return PP_OK_COMPLETIONPENDING if request to show the dialog was
   * successful, another error code from pp_errors.h on failure.
   */
  int32_t (*ShowWithoutUserGesture)(PP_Resource chooser,
                                    struct PP_CompletionCallback callback);
};
/**
 * @}
 */

#endif  /* PPAPI_C_TRUSTED_PPB_FILE_CHOOSER_TRUSTED_H_ */

