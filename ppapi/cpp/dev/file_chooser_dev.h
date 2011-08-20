// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_FILE_CHOOSER_DEV_H_
#define PPAPI_CPP_DEV_FILE_CHOOSER_DEV_H_

#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class FileRef;
class Instance;
class Var;

class FileChooser_Dev : public Resource {
 public:
  /// Creates an is_null() FileChooser object.
  FileChooser_Dev() {}

  /// This function creates a file chooser dialog resource.  The chooser is
  /// associated with a particular instance, so that it may be positioned on the
  /// screen relative to the tab containing the instance.  Returns 0 if passed
  /// an invalid instance.
  ///
  /// @param mode A PPB_FileChooser_Dev instance can be used to select a single
  /// file (PP_FILECHOOSERMODE_OPEN) or multiple files
  /// (PP_FILECHOOSERMODE_OPENMULTIPLE). Unlike the HTML5 <input type="file">
  /// tag, a PPB_FileChooser_Dev instance cannot be used to select a directory.
  /// In order to get the list of files in a directory, the
  /// PPB_DirectoryReader_Dev interface must be used.
  ///
  /// @param accept_mime_types A comma-separated list of MIME types such as
  /// "audio/ *,text/plain" (note there should be no space between the '/' and
  /// the '*', but one is added to avoid confusing C++ comments).  The dialog
  /// may restrict selectable files to the specified MIME types. An empty string
  /// or an undefined var may be given to indicate that all types should be
  /// accepted.
  ///
  /// TODO(darin): What if the mime type is unknown to the system?  The plugin
  /// may wish to describe the mime type and provide a matching file extension.
  /// It is more webby to use mime types here instead of file extensions.
  FileChooser_Dev(const Instance* instance,
                  PP_FileChooserMode_Dev mode,
                  const Var& accept_mime_types);

  FileChooser_Dev(const FileChooser_Dev& other);

  /// This function displays a previously created file chooser resource as a
  /// dialog box, prompting the user to choose a file or files. The callback is
  /// called with PP_OK on successful completion with a file (or files) selected
  /// or PP_ERROR_USERCANCEL if the user selected no file.
  ///
  /// @return PP_OK_COMPLETIONPENDING if request to show the dialog was
  /// successful, another error code from pp_errors.h on failure.
  int32_t Show(const CompletionCallback& cc);

  /// After a successful completion callback call from Show, this method may be
  /// used to query the chosen files.  It should be called in a loop until it
  /// returns an is_null() FileRef.  Depending on the PP_ChooseFileMode
  /// requested when the FileChooser was created, the file refs will either
  /// be readable or writable.  Their file system type will be
  /// PP_FileSystemType_External.  If the user chose no files or cancelled the
  /// dialog, then this method will simply return an is_null() FileRef the
  /// first time it is called.
  FileRef GetNextChosenFile() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_FILE_CHOOSER_DEV_H_
