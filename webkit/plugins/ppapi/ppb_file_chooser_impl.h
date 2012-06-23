// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FILE_CHOOSER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FILE_CHOOSER_IMPL_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_file_chooser_api.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace ppapi {
class TrackedCallback;
}

namespace WebKit {
class WebString;
}

namespace webkit {
namespace ppapi {

class PPB_FileRef_Impl;

class PPB_FileChooser_Impl : public ::ppapi::Resource,
                             public ::ppapi::thunk::PPB_FileChooser_API {
 public:
  // Structure to store the information of chosen files.
  struct ChosenFileInfo {
    ChosenFileInfo(const std::string& path, const std::string& display_name);
    std::string path;
    // |display_name| may be empty.
    std::string display_name;
  };

  PPB_FileChooser_Impl(PP_Instance instance,
                       PP_FileChooserMode_Dev mode,
                       const char* accept_types);
  virtual ~PPB_FileChooser_Impl();

  static PP_Resource Create(PP_Instance instance,
                            PP_FileChooserMode_Dev mode,
                            const char* accept_types);

  // Resource overrides.
  virtual PPB_FileChooser_Impl* AsPPB_FileChooser_Impl();

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_FileChooser_API* AsPPB_FileChooser_API() OVERRIDE;

  // Stores the list of selected files.
  void StoreChosenFiles(const std::vector<ChosenFileInfo>& files);

  // Check that |callback| is valid (only non-blocking operation is supported)
  // and that no callback is already pending. Returns |PP_OK| if okay, else
  // |PP_ERROR_...| to be returned to the plugin.
  int32_t ValidateCallback(scoped_refptr< ::ppapi::TrackedCallback> callback);

  // Sets up |callback| as the pending callback. This should only be called once
  // it is certain that |PP_OK_COMPLETIONPENDING| will be returned.
  void RegisterCallback(scoped_refptr< ::ppapi::TrackedCallback> callback);

  void RunCallback(int32_t result);

  // PPB_FileChooser_API implementation.
  virtual int32_t Show(
      const PP_ArrayOutput& output,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t ShowWithoutUserGesture(
      PP_Bool save_as,
      PP_Var suggested_file_name,
      const PP_ArrayOutput& output,
      scoped_refptr< ::ppapi::TrackedCallback> callback);
  virtual int32_t Show0_5(
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual PP_Resource GetNextChosenFile() OVERRIDE;
  virtual int32_t ShowWithoutUserGesture0_5(
      PP_Bool save_as,
      PP_Var suggested_file_name,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;

  // Splits a comma-separated MIME type/extension list |accept_types|, trims the
  // resultant split types, makes them lowercase, and returns them.
  // Though this should be private, this is public for testing.
  WEBKIT_PLUGINS_EXPORT static std::vector<WebKit::WebString> ParseAcceptValue(
      const std::string& accept_types);

 private:
  PP_FileChooserMode_Dev mode_;
  std::string accept_types_;
  scoped_refptr< ::ppapi::TrackedCallback> callback_;

  // When using the v0.6 of the API, this will contain the output for the
  // resources when the show command is complete. When using 0.5, this
  // object will be is_null() and the chosen_files_ will be used instead.
  ::ppapi::ArrayWriter output_;

  // Used to store and iterate over the results when using 0.5 of the API.
  // These are valid when we get a file result and output_ is not null.
  std::vector< scoped_refptr<Resource> > chosen_files_;
  size_t next_chosen_file_index_;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FILE_CHOOSER_IMPL_H_
