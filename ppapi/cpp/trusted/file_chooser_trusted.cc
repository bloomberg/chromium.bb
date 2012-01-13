// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/trusted/file_chooser_trusted.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileChooserTrusted>() {
  return PPB_FILECHOOSER_TRUSTED_INTERFACE;
}

}  // namespace

FileChooser_Trusted::FileChooser_Trusted() : save_as_(false) {
}

FileChooser_Trusted::FileChooser_Trusted(const Instance* instance,
                                         PP_FileChooserMode_Dev mode,
                                         const Var& accept_mime_types,
                                         bool save_as,
                                         const std::string& suggested_file_name)
    : FileChooser_Dev(instance, mode, accept_mime_types),
      save_as_(save_as),
      suggested_file_name_(suggested_file_name) {
}

FileChooser_Trusted::FileChooser_Trusted(const FileChooser_Trusted& other)
    : FileChooser_Dev(other),
      save_as_(other.save_as_),
      suggested_file_name_(other.suggested_file_name_) {
}

FileChooser_Trusted& FileChooser_Trusted::operator=(
    const FileChooser_Trusted& other) {
  FileChooser_Dev::operator=(other);
  save_as_ = other.save_as_;
  suggested_file_name_ = other.suggested_file_name_;
  return *this;
}

int32_t FileChooser_Trusted::Show(const CompletionCallback& cc) {
  if (!has_interface<PPB_FileChooserTrusted>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileChooserTrusted>()->ShowWithoutUserGesture(
      pp_resource(),
      PP_FromBool(save_as_),
      Var(suggested_file_name_).pp_var(),
      cc.pp_completion_callback());
}

}  // namespace pp
