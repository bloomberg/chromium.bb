// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

namespace {

DeviceFuncs<PPB_FileChooser_Dev> file_chooser_f(PPB_FILECHOOSER_DEV_INTERFACE);

}  // namespace

namespace pp {

FileChooser_Dev::FileChooser_Dev(const Instance& instance,
                                 const PP_FileChooserOptions_Dev& options) {
  if (!file_chooser_f)
    return;
  PassRefFromConstructor(file_chooser_f->Create(instance.pp_instance(),
                                                &options));
}

FileChooser_Dev::FileChooser_Dev(const FileChooser_Dev& other)
    : Resource(other) {
}

FileChooser_Dev& FileChooser_Dev::operator=(const FileChooser_Dev& other) {
  FileChooser_Dev copy(other);
  swap(copy);
  return *this;
}

void FileChooser_Dev::swap(FileChooser_Dev& other) {
  Resource::swap(other);
}

int32_t FileChooser_Dev::Show(const CompletionCallback& cc) {
  if (!file_chooser_f)
    return PP_ERROR_NOINTERFACE;
  return file_chooser_f->Show(pp_resource(), cc.pp_completion_callback());
}

FileRef_Dev FileChooser_Dev::GetNextChosenFile() const {
  if (!file_chooser_f)
    return FileRef_Dev();
  return FileRef_Dev(FileRef_Dev::PassRef(),
                 file_chooser_f->GetNextChosenFile(pp_resource()));
}

}  // namespace pp
