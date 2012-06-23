// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_file_chooser_impl.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::StringVar;
using ppapi::thunk::PPB_FileChooser_API;
using ppapi::TrackedCallback;
using WebKit::WebCString;
using WebKit::WebFileChooserCompletion;
using WebKit::WebFileChooserParams;
using WebKit::WebString;
using WebKit::WebVector;

namespace webkit {
namespace ppapi {

namespace {

class FileChooserCompletionImpl : public WebFileChooserCompletion {
 public:
  FileChooserCompletionImpl(PPB_FileChooser_Impl* file_chooser)
      : file_chooser_(file_chooser) {
    DCHECK(file_chooser_);
  }

  virtual ~FileChooserCompletionImpl() {}

  virtual void didChooseFile(const WebVector<WebString>& file_names) {
    std::vector<PPB_FileChooser_Impl::ChosenFileInfo> files;
    for (size_t i = 0; i < file_names.size(); i++) {
      files.push_back(
          PPB_FileChooser_Impl::ChosenFileInfo(file_names[i].utf8(),
                                               std::string()));
    }

    file_chooser_->StoreChosenFiles(files);

    // It is the responsibility of this method to delete the instance.
    delete this;
  }
  virtual void didChooseFile(const WebVector<SelectedFileInfo>& file_names) {
    std::vector<PPB_FileChooser_Impl::ChosenFileInfo> files;
    for (size_t i = 0; i < file_names.size(); i++) {
      files.push_back(
          PPB_FileChooser_Impl::ChosenFileInfo(
              file_names[i].path.utf8(),
              file_names[i].displayName.utf8()));
    }

    file_chooser_->StoreChosenFiles(files);

    // It is the responsibility of this method to delete the instance.
    delete this;
  }

 private:
  scoped_refptr<PPB_FileChooser_Impl> file_chooser_;
};

}  // namespace

PPB_FileChooser_Impl::ChosenFileInfo::ChosenFileInfo(
    const std::string& path,
    const std::string& display_name)
    : path(path),
      display_name(display_name) {
}

PPB_FileChooser_Impl::PPB_FileChooser_Impl(
    PP_Instance instance,
    PP_FileChooserMode_Dev mode,
    const char* accept_types)
    : Resource(::ppapi::OBJECT_IS_IMPL, instance),
      mode_(mode),
      next_chosen_file_index_(0) {
  if (accept_types)
    accept_types_ = std::string(accept_types);
}

PPB_FileChooser_Impl::~PPB_FileChooser_Impl() {
}

// static
PP_Resource PPB_FileChooser_Impl::Create(
    PP_Instance instance,
    PP_FileChooserMode_Dev mode,
    const char* accept_types) {
  if (mode != PP_FILECHOOSERMODE_OPEN &&
      mode != PP_FILECHOOSERMODE_OPENMULTIPLE)
    return 0;
  return (new PPB_FileChooser_Impl(instance, mode,
                                   accept_types))->GetReference();
}

PPB_FileChooser_Impl* PPB_FileChooser_Impl::AsPPB_FileChooser_Impl() {
  return this;
}

PPB_FileChooser_API* PPB_FileChooser_Impl::AsPPB_FileChooser_API() {
  return this;
}

void PPB_FileChooser_Impl::StoreChosenFiles(
    const std::vector<ChosenFileInfo>& files) {
  next_chosen_file_index_ = 0;

  // It is possible that |callback_| has been run: before the user takes action
  // on the file chooser, the page navigates away and causes the plugin module
  // (whose instance requested to show the file chooser) to be destroyed. In
  // that case, |callback_| has been aborted when we get here.
  if (!TrackedCallback::IsPending(callback_)) {
    // To be cautious, reset our internal state.
    output_.Reset();
    chosen_files_.clear();
    return;
  }

  std::vector< scoped_refptr<Resource> > chosen_files;
  for (std::vector<ChosenFileInfo>::const_iterator it = files.begin();
       it != files.end(); ++it) {
#if defined(OS_WIN)
    FilePath file_path(base::SysUTF8ToWide(it->path));
#else
    FilePath file_path(it->path);
#endif

    chosen_files.push_back(scoped_refptr<Resource>(
        PPB_FileRef_Impl::CreateExternal(pp_instance(),
                                         file_path,
                                         it->display_name)));
  }

  int32_t result_code = (chosen_files.size() > 0) ? PP_OK : PP_ERROR_USERCANCEL;
  if (output_.is_valid())
    output_.StoreResourceVector(chosen_files);
  else  // v0.5 API.
    chosen_files_.swap(chosen_files);
  RunCallback(result_code);
}

int32_t PPB_FileChooser_Impl::ValidateCallback(
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(callback_))
    return PP_ERROR_INPROGRESS;

  return PP_OK;
}

void PPB_FileChooser_Impl::RegisterCallback(
    scoped_refptr<TrackedCallback> callback) {
  DCHECK(!TrackedCallback::IsPending(callback_));

  PluginModule* plugin_module = ResourceHelper::GetPluginModule(this);
  if (!plugin_module)
    return;

  callback_ = callback;
}

void PPB_FileChooser_Impl::RunCallback(int32_t result) {
  TrackedCallback::ClearAndRun(&callback_, result);
}

int32_t PPB_FileChooser_Impl::Show(const PP_ArrayOutput& output,
                                   scoped_refptr<TrackedCallback> callback) {
  int32_t result = Show0_5(callback);
  if (result == PP_OK_COMPLETIONPENDING)
    output_.set_pp_array_output(output);
  return result;
}

int32_t PPB_FileChooser_Impl::ShowWithoutUserGesture(
    PP_Bool save_as,
    PP_Var suggested_file_name,
    const PP_ArrayOutput& output,
    scoped_refptr<TrackedCallback> callback) {
  int32_t result = ShowWithoutUserGesture0_5(save_as, suggested_file_name,
                                             callback);
  if (result == PP_OK_COMPLETIONPENDING)
    output_.set_pp_array_output(output);
  return result;
}

int32_t PPB_FileChooser_Impl::Show0_5(scoped_refptr<TrackedCallback> callback) {
  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  if (!plugin_instance->IsProcessingUserGesture())
    return PP_ERROR_NO_USER_GESTURE;
  return ShowWithoutUserGesture0_5(PP_FALSE, PP_MakeUndefined(), callback);
}

int32_t PPB_FileChooser_Impl::ShowWithoutUserGesture0_5(
    PP_Bool save_as,
    PP_Var suggested_file_name,
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;

  DCHECK((mode_ == PP_FILECHOOSERMODE_OPEN) ||
         (mode_ == PP_FILECHOOSERMODE_OPENMULTIPLE));

  WebFileChooserParams params;
  if (save_as) {
    params.saveAs = true;
    StringVar* str = StringVar::FromPPVar(suggested_file_name);
    if (str)
      params.initialValue = WebString::fromUTF8(str->value().c_str());
  } else {
    params.multiSelect = (mode_ == PP_FILECHOOSERMODE_OPENMULTIPLE);
  }
  params.acceptTypes = ParseAcceptValue(accept_types_);
  params.directory = false;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  if (!plugin_delegate->RunFileChooser(params,
                                       new FileChooserCompletionImpl(this)))
    return PP_ERROR_FAILED;

  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

PP_Resource PPB_FileChooser_Impl::GetNextChosenFile() {
  if (next_chosen_file_index_ >= chosen_files_.size())
    return 0;

  return chosen_files_[next_chosen_file_index_++]->GetReference();
}

std::vector<WebString> PPB_FileChooser_Impl::ParseAcceptValue(
    const std::string& accept_types) {
  if (accept_types.empty())
    return std::vector<WebString>();
  std::vector<std::string> type_list;
  base::SplitString(accept_types, ',', &type_list);
  std::vector<WebString> normalized_type_list;
  normalized_type_list.reserve(type_list.size());
  for (size_t i = 0; i < type_list.size(); ++i) {
    std::string type = type_list[i];
    TrimWhitespaceASCII(type, TRIM_ALL, &type);

    // If the type is a single character, it definitely cannot be valid. In the
    // case of a file extension it would be a single ".". In the case of a MIME
    // type it would just be a "/".
    if (type.length() < 2)
      continue;
    if (type.find_first_of('/') == std::string::npos && type[0] != '.')
      continue;
    StringToLowerASCII(&type);
    normalized_type_list.push_back(WebString::fromUTF8(type.data(),
                                                       type.size()));
  }
  return normalized_type_list;
}

}  // namespace ppapi
}  // namespace webkit
