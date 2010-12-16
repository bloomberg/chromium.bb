// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_file_chooser_impl.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileChooserCompletion.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileChooserParams.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebCString;
using WebKit::WebFileChooserCompletion;
using WebKit::WebFileChooserParams;
using WebKit::WebString;
using WebKit::WebVector;

namespace webkit {
namespace ppapi {

namespace {

PP_Resource Create(PP_Instance instance_id,
                   const PP_FileChooserOptions_Dev* options) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  if ((options->mode != PP_FILECHOOSERMODE_OPEN) &&
      (options->mode != PP_FILECHOOSERMODE_OPENMULTIPLE))
    return 0;

  PPB_FileChooser_Impl* chooser = new PPB_FileChooser_Impl(instance, options);
  return chooser->GetReference();
}

PP_Bool IsFileChooser(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_FileChooser_Impl>(resource));
}

int32_t Show(PP_Resource chooser_id, PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileChooser_Impl> chooser(
      Resource::GetAs<PPB_FileChooser_Impl>(chooser_id));
  if (!chooser)
    return PP_ERROR_BADRESOURCE;

  return chooser->Show(callback);
}

PP_Resource GetNextChosenFile(PP_Resource chooser_id) {
  scoped_refptr<PPB_FileChooser_Impl> chooser(
      Resource::GetAs<PPB_FileChooser_Impl>(chooser_id));
  if (!chooser)
    return 0;

  scoped_refptr<PPB_FileRef_Impl> file_ref(chooser->GetNextChosenFile());
  if (!file_ref)
    return 0;

  return file_ref->GetReference();
}

const PPB_FileChooser_Dev ppb_filechooser = {
  &Create,
  &IsFileChooser,
  &Show,
  &GetNextChosenFile
};

class FileChooserCompletionImpl : public WebFileChooserCompletion {
 public:
  FileChooserCompletionImpl(PPB_FileChooser_Impl* file_chooser)
      : file_chooser_(file_chooser) {
    DCHECK(file_chooser_);
  }

  virtual ~FileChooserCompletionImpl() {}

  virtual void didChooseFile(const WebVector<WebString>& file_names) {
    std::vector<std::string> files;
    for (size_t i = 0; i < file_names.size(); i++)
      files.push_back(file_names[i].utf8().data());

    file_chooser_->StoreChosenFiles(files);
  }

 private:
  PPB_FileChooser_Impl* file_chooser_;
};

}  // namespace

PPB_FileChooser_Impl::PPB_FileChooser_Impl(PluginInstance* instance,
                         const PP_FileChooserOptions_Dev* options)
    : Resource(instance->module()),
      delegate_(instance->delegate()),
      mode_(options->mode),
      accept_mime_types_(options->accept_mime_types),
      completion_callback_() {
}

PPB_FileChooser_Impl::~PPB_FileChooser_Impl() {
}

// static
const PPB_FileChooser_Dev* PPB_FileChooser_Impl::GetInterface() {
  return &ppb_filechooser;
}

PPB_FileChooser_Impl* PPB_FileChooser_Impl::AsPPB_FileChooser_Impl() {
  return this;
}

void PPB_FileChooser_Impl::StoreChosenFiles(
    const std::vector<std::string>& files) {
  next_chosen_file_index_ = 0;
  std::vector<std::string>::const_iterator end_it = files.end();
  for (std::vector<std::string>::const_iterator it = files.begin();
       it != end_it; it++) {
    chosen_files_.push_back(make_scoped_refptr(
        new PPB_FileRef_Impl(module(), FilePath().AppendASCII(*it))));
  }

  if (!completion_callback_.func)
    return;

  PP_CompletionCallback callback = {0};
  std::swap(callback, completion_callback_);
  PP_RunCompletionCallback(&callback, 0);
}

int32_t PPB_FileChooser_Impl::Show(PP_CompletionCallback callback) {
  DCHECK((mode_ == PP_FILECHOOSERMODE_OPEN) ||
         (mode_ == PP_FILECHOOSERMODE_OPENMULTIPLE));
  DCHECK(!completion_callback_.func);
  completion_callback_ = callback;

  WebFileChooserParams params;
  params.multiSelect = (mode_ == PP_FILECHOOSERMODE_OPENMULTIPLE);
  params.acceptTypes = WebString::fromUTF8(accept_mime_types_);
  params.directory = false;

  return delegate_->RunFileChooser(
      params, new FileChooserCompletionImpl(this));
}

scoped_refptr<PPB_FileRef_Impl> PPB_FileChooser_Impl::GetNextChosenFile() {
  if (next_chosen_file_index_ >= chosen_files_.size())
    return NULL;

  return chosen_files_[next_chosen_file_index_++];
}

}  // namespace ppapi
}  // namespace webkit

