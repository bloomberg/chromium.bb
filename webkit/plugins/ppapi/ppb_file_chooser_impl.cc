// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_file_chooser_impl.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
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
      files.push_back(file_names[i].utf8());

    file_chooser_->StoreChosenFiles(files);
  }

 private:
  scoped_refptr<PPB_FileChooser_Impl> file_chooser_;
};

}  // namespace

PPB_FileChooser_Impl::PPB_FileChooser_Impl(
    PluginInstance* instance,
    const PP_FileChooserOptions_Dev* options)
    : Resource(instance),
      mode_(options->mode),
      accept_mime_types_(options->accept_mime_types ?
                         options->accept_mime_types : ""),
      next_chosen_file_index_(0) {
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
  chosen_files_.clear();
  next_chosen_file_index_ = 0;
  for (std::vector<std::string>::const_iterator it = files.begin();
       it != files.end(); ++it) {
#if defined(OS_WIN)
    FilePath file_path(base::SysUTF8ToWide(*it));
#else
    FilePath file_path(*it);
#endif

    chosen_files_.push_back(make_scoped_refptr(
        new PPB_FileRef_Impl(instance(), file_path)));
  }

  RunCallback(PP_OK);
}

int32_t PPB_FileChooser_Impl::ValidateCallback(
    const PP_CompletionCallback& callback) {
  // We only support non-blocking calls.
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  if (callback_.get() && !callback_->completed())
    return PP_ERROR_INPROGRESS;

  return PP_OK;
}

void PPB_FileChooser_Impl::RegisterCallback(
    const PP_CompletionCallback& callback) {
  DCHECK(callback.func);
  DCHECK(!callback_.get() || callback_->completed());

  PP_Resource resource_id = GetReferenceNoAddRef();
  CHECK(resource_id);
  callback_ = new TrackedCompletionCallback(
      instance()->module()->GetCallbackTracker(), resource_id, callback);
}

void PPB_FileChooser_Impl::RunCallback(int32_t result) {
  scoped_refptr<TrackedCompletionCallback> callback;
  callback.swap(callback_);
  callback->Run(result);  // Will complete abortively if necessary.
}

int32_t PPB_FileChooser_Impl::Show(const PP_CompletionCallback& callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;

  DCHECK((mode_ == PP_FILECHOOSERMODE_OPEN) ||
         (mode_ == PP_FILECHOOSERMODE_OPENMULTIPLE));

  WebFileChooserParams params;
  params.multiSelect = (mode_ == PP_FILECHOOSERMODE_OPENMULTIPLE);
  params.acceptTypes = WebString::fromUTF8(accept_mime_types_);
  params.directory = false;

  if (!instance()->delegate()->RunFileChooser(params,
          new FileChooserCompletionImpl(this)))
    return PP_ERROR_FAILED;

  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

scoped_refptr<PPB_FileRef_Impl> PPB_FileChooser_Impl::GetNextChosenFile() {
  if (next_chosen_file_index_ >= chosen_files_.size())
    return NULL;

  return chosen_files_[next_chosen_file_index_++];
}

}  // namespace ppapi
}  // namespace webkit
