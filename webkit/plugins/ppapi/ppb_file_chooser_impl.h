// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FILE_CHOOSER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FILE_CHOOSER_IMPL_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_file_chooser_api.h"
#include "webkit/plugins/webkit_plugins_export.h"

struct PP_CompletionCallback;

namespace WebKit {
class WebString;
}

namespace webkit {
namespace ppapi {

class PPB_FileRef_Impl;
class TrackedCompletionCallback;

class PPB_FileChooser_Impl : public ::ppapi::Resource,
                             public ::ppapi::thunk::PPB_FileChooser_API {
 public:
  PPB_FileChooser_Impl(PP_Instance instance,
                       PP_FileChooserMode_Dev mode,
                       const char* accept_mime_types);
  virtual ~PPB_FileChooser_Impl();

  static PP_Resource Create(PP_Instance instance,
                            PP_FileChooserMode_Dev mode,
                            const char* accept_mime_types);

  // Resource overrides.
  virtual PPB_FileChooser_Impl* AsPPB_FileChooser_Impl();

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_FileChooser_API* AsPPB_FileChooser_API() OVERRIDE;

  // Stores the list of selected files.
  void StoreChosenFiles(const std::vector<std::string>& files);

  // Check that |callback| is valid (only non-blocking operation is supported)
  // and that no callback is already pending. Returns |PP_OK| if okay, else
  // |PP_ERROR_...| to be returned to the plugin.
  int32_t ValidateCallback(const PP_CompletionCallback& callback);

  // Sets up |callback| as the pending callback. This should only be called once
  // it is certain that |PP_OK_COMPLETIONPENDING| will be returned.
  void RegisterCallback(const PP_CompletionCallback& callback);

  void RunCallback(int32_t result);

  // PPB_FileChooser_API implementation.
  virtual int32_t Show(const PP_CompletionCallback& callback) OVERRIDE;
  virtual PP_Resource GetNextChosenFile() OVERRIDE;

  virtual int32_t ShowWithoutUserGesture(
      bool save_as,
      const char* suggested_file_name,
      const PP_CompletionCallback& callback) OVERRIDE;

  // Splits a comma-separated MIME type list |accept_mime_types|, trims the
  // resultant split types, makes them lowercase, and returns them.
  // Though this should be private, this is public for testing.
  WEBKIT_PLUGINS_EXPORT static std::vector<WebKit::WebString> ParseAcceptValue(
      const std::string& accept_mime_types);

 private:
  PP_FileChooserMode_Dev mode_;
  std::string accept_mime_types_;
  scoped_refptr<TrackedCompletionCallback> callback_;
  std::vector< scoped_refptr<PPB_FileRef_Impl> > chosen_files_;
  size_t next_chosen_file_index_;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FILE_CHOOSER_IMPL_H_
