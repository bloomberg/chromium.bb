// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/file_chooser_dev.h"

#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileChooser_Dev>() {
  return PPB_FILECHOOSER_DEV_INTERFACE;
}

}  // namespace

FileChooser_Dev::FileChooser_Dev(const Instance& instance,
                                 const PP_FileChooserOptions_Dev& options) {
  if (!has_interface<PPB_FileChooser_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_FileChooser_Dev>()->Create(
      instance.pp_instance(), &options));
}

FileChooser_Dev::FileChooser_Dev(const FileChooser_Dev& other)
    : Resource(other) {
}

int32_t FileChooser_Dev::Show(const CompletionCallback& cc) {
  if (!has_interface<PPB_FileChooser_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileChooser_Dev>()->Show(
      pp_resource(), cc.pp_completion_callback());
}

FileRef_Dev FileChooser_Dev::GetNextChosenFile() const {
  if (!has_interface<PPB_FileChooser_Dev>())
    return FileRef_Dev();
  return FileRef_Dev(FileRef_Dev::PassRef(),
      get_interface<PPB_FileChooser_Dev>()->GetNextChosenFile(pp_resource()));
}

}  // namespace pp
