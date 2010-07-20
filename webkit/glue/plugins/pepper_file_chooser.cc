// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_file_chooser.h"

#include "base/logging.h"
#include "third_party/ppapi/c/pp_completion_callback.h"
#include "third_party/ppapi/c/pp_errors.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

namespace {

PP_Resource Create(PP_Instance instance_id,
                   const PP_FileChooserOptions* options) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return 0;

  FileChooser* chooser = new FileChooser(instance, options);
  return chooser->GetReference();
}

bool IsFileChooser(PP_Resource resource) {
  return !!Resource::GetAs<FileChooser>(resource);
}

int32_t Show(PP_Resource chooser_id, PP_CompletionCallback callback) {
  scoped_refptr<FileChooser> chooser(
      Resource::GetAs<FileChooser>(chooser_id));
  if (!chooser)
    return PP_ERROR_BADRESOURCE;

  return chooser->Show(callback);
}

PP_Resource GetNextChosenFile(PP_Resource chooser_id) {
  scoped_refptr<FileChooser> chooser(
      Resource::GetAs<FileChooser>(chooser_id));
  if (!chooser)
    return 0;

  scoped_refptr<FileRef> file_ref(chooser->GetNextChosenFile());
  if (!file_ref)
    return 0;

  return file_ref->GetReference();
}

const PPB_FileChooser ppb_filechooser = {
  &Create,
  &IsFileChooser,
  &Show,
  &GetNextChosenFile
};

}  // namespace

FileChooser::FileChooser(PluginInstance* instance,
                         const PP_FileChooserOptions* options)
    : Resource(instance->module()),
      mode_(options->mode),
      accept_mime_types_(options->accept_mime_types) {
}

FileChooser::~FileChooser() {
}

// static
const PPB_FileChooser* FileChooser::GetInterface() {
  return &ppb_filechooser;
}

int32_t FileChooser::Show(PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_ERROR_FAILED;
}

scoped_refptr<FileRef> FileChooser::GetNextChosenFile() {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return NULL;
}

}  // namespace pepper
