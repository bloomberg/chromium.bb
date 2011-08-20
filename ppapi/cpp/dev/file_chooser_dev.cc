// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/file_chooser_dev.h"

#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileChooser_Dev>() {
  return PPB_FILECHOOSER_DEV_INTERFACE;
}

}  // namespace

FileChooser_Dev::FileChooser_Dev(const Instance* instance,
                                 PP_FileChooserMode_Dev mode,
                                 const Var& accept_mime_types) {
  if (!has_interface<PPB_FileChooser_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_FileChooser_Dev>()->Create(
      instance->pp_instance(), mode, accept_mime_types.pp_var()));
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

FileRef FileChooser_Dev::GetNextChosenFile() const {
  if (!has_interface<PPB_FileChooser_Dev>())
    return FileRef();
  return FileRef(FileRef::PassRef(),
      get_interface<PPB_FileChooser_Dev>()->GetNextChosenFile(pp_resource()));
}

}  // namespace pp
