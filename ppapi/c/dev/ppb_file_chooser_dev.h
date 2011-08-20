/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_FILE_CHOOSER_DEV_H_
#define PPAPI_C_DEV_PPB_FILE_CHOOSER_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"

struct PP_CompletionCallback;

typedef enum {
  PP_FILECHOOSERMODE_OPEN = 0,
  PP_FILECHOOSERMODE_OPENMULTIPLE = 1
  // TODO(darin): Should there be a way to choose a directory?
} PP_FileChooserMode_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FileChooserMode_Dev, 4);

#define PPB_FILECHOOSER_DEV_INTERFACE_0_5 "PPB_FileChooser(Dev);0.5"
#define PPB_FILECHOOSER_DEV_INTERFACE PPB_FILECHOOSER_DEV_INTERFACE_0_5

struct PPB_FileChooser_Dev {
  /**
   * This function creates a file chooser dialog resource.  The chooser is
   * associated with a particular instance, so that it may be positioned on the
   * screen relative to the tab containing the instance.  Returns 0 if passed
   * an invalid instance.
   *
   * @param mode A PPB_FileChooser_Dev instance can be used to select a single
   * file (PP_FILECHOOSERMODE_OPEN) or multiple files
   * (PP_FILECHOOSERMODE_OPENMULTIPLE). Unlike the HTML5 <input type="file">
   * tag, a PPB_FileChooser_Dev instance cannot be used to select a directory.
   * In order to get the list of files in a directory, the
   * PPB_DirectoryReader_Dev interface must be used.
   *
   * @param accept_mime_types A comma-separated list of MIME types such as
   * "audio/ *,text/plain" (note there should be no space between the '/' and
   * the '*', but one is added to avoid confusing C++ comments).  The dialog
   * may restrict selectable files to the specified MIME types. An empty string
   * or an undefined var may be given to indicate that all types should be
   * accepted.
   *
   * TODO(darin): What if the mime type is unknown to the system?  The plugin
   * may wish to describe the mime type and provide a matching file extension.
   * It is more webby to use mime types here instead of file extensions.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PP_FileChooserMode_Dev mode,
                        struct PP_Var accept_mime_types);

  /**
   * IsFileChooser returns PP_TRUE if the given resource is a FileChooser.
   * It returns PP_FALSE if the resource is invalid or some type other than a
   * FileChooser.
   */
  PP_Bool (*IsFileChooser)(PP_Resource resource);

  /**
   * This function displays a previously created file chooser resource as a
   * dialog box, prompting the user to choose a file or files. The callback is
   * called with PP_OK on successful completion with a file (or files) selected
   * or PP_ERROR_USERCANCEL if the user selected no file.
   *
   * @return PP_OK_COMPLETIONPENDING if request to show the dialog was
   * successful, another error code from pp_errors.h on failure.
   */
  int32_t (*Show)(PP_Resource chooser, struct PP_CompletionCallback callback);

  /**
   * After a successful completion callback call from Show, this method may be
   * used to query the chosen files.  It should be called in a loop until it
   * returns 0.  Depending on the PP_ChooseFileMode_Dev requested when the
   * FileChooser was created, the file refs will either be readable or
   * writable.  Their file system type will be PP_FileSystemType_External.  If
   * the user chose no files or cancelled the dialog, then this method will
   * simply return 0 the first time it is called.
   */
  PP_Resource (*GetNextChosenFile)(PP_Resource chooser);
};

// Deprecated 0.4 version ------------------------------------------------------

#define PPB_FILECHOOSER_DEV_INTERFACE_0_4 "PPB_FileChooser(Dev);0.4"

struct PP_FileChooserOptions_0_4_Dev {
  PP_FileChooserMode_Dev mode;
  const char* accept_mime_types;
};

struct PPB_FileChooser_0_4_Dev {
  PP_Resource (*Create)(PP_Instance instance,
                        const struct PP_FileChooserOptions_0_4_Dev* options);
  PP_Bool (*IsFileChooser)(PP_Resource resource);
  int32_t (*Show)(PP_Resource chooser, struct PP_CompletionCallback callback);
  PP_Resource (*GetNextChosenFile)(PP_Resource chooser);
};

#endif  /* PPAPI_C_DEV_PPB_FILE_CHOOSER_DEV_H_ */
