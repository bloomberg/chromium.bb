// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_file_chooser_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   PP_FileChooserMode_Dev mode,
                   struct PP_Var accept_mime_types) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  scoped_refptr<StringVar> string_var =
      StringVar::FromPPVar(accept_mime_types);
  std::string str = string_var ? string_var->value() : std::string();
  return enter.functions()->CreateFileChooser(instance, mode, str.c_str());
}

PP_Bool IsFileChooser(PP_Resource resource) {
  EnterResource<PPB_FileChooser_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Show(PP_Resource chooser, PP_CompletionCallback callback) {
  EnterResource<PPB_FileChooser_API> enter(chooser, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Show(callback);
  return MayForceCallback(callback, result);
}

PP_Resource GetNextChosenFile(PP_Resource chooser) {
  EnterResource<PPB_FileChooser_API> enter(chooser, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetNextChosenFile();
}

int32_t ShowWithoutUserGesture(PP_Resource chooser,
                               PP_Bool save_as,
                               PP_Var suggested_file_name,
                               PP_CompletionCallback callback) {
  EnterResource<PPB_FileChooser_API> enter(chooser, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  scoped_refptr<StringVar> string_var =
      StringVar::FromPPVar(suggested_file_name);
  std::string str = string_var ? string_var->value() : std::string();
  int32_t result = enter.object()->ShowWithoutUserGesture(
      save_as == PP_TRUE, str.c_str(), callback);
  return MayForceCallback(callback, result);
}

const PPB_FileChooser_Dev g_ppb_file_chooser_thunk = {
  &Create,
  &IsFileChooser,
  &Show,
  &GetNextChosenFile
};

const PPB_FileChooserTrusted g_ppb_file_chooser_trusted_thunk = {
  &ShowWithoutUserGesture
};

}  // namespace

const PPB_FileChooser_Dev_0_5* GetPPB_FileChooser_Dev_0_5_Thunk() {
  return &g_ppb_file_chooser_thunk;
}

const PPB_FileChooserTrusted_0_5* GetPPB_FileChooser_Trusted_0_5_Thunk() {
  return &g_ppb_file_chooser_trusted_thunk;
}

}  // namespace thunk
}  // namespace ppapi
