// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_file_chooser_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   PP_FileChooserMode_Dev mode,
                   struct PP_Var accept_types) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  scoped_refptr<StringVar> string_var =
      StringVar::FromPPVar(accept_types);
  std::string str = string_var ? string_var->value() : std::string();
  return enter.functions()->CreateFileChooser(instance, mode, str.c_str());
}

PP_Bool IsFileChooser(PP_Resource resource) {
  EnterResource<PPB_FileChooser_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Show0_5(PP_Resource chooser, PP_CompletionCallback callback) {
  EnterResource<PPB_FileChooser_API> enter(chooser, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Show0_5(enter.callback()));
}

PP_Resource GetNextChosenFile0_5(PP_Resource chooser) {
  EnterResource<PPB_FileChooser_API> enter(chooser, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetNextChosenFile();
}

int32_t Show(PP_Resource chooser,
             PP_ArrayOutput output,
             PP_CompletionCallback callback) {
  EnterResource<PPB_FileChooser_API> enter(chooser, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Show(output, enter.callback()));
}

int32_t ShowWithoutUserGesture0_5(PP_Resource chooser,
                                  PP_Bool save_as,
                                  PP_Var suggested_file_name,
                                  PP_CompletionCallback callback) {
  EnterResource<PPB_FileChooser_API> enter(chooser, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->ShowWithoutUserGesture0_5(
      save_as, suggested_file_name, enter.callback()));
}

int32_t ShowWithoutUserGesture0_6(PP_Resource chooser,
                                  PP_Bool save_as,
                                  PP_Var suggested_file_name,
                                  PP_ArrayOutput output,
                                  PP_CompletionCallback callback) {
  EnterResource<PPB_FileChooser_API> enter(chooser, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->ShowWithoutUserGesture(
      save_as, suggested_file_name, output, enter.callback()));
}

const PPB_FileChooser_Dev_0_5 g_ppb_file_chooser_0_5_thunk = {
  &Create,
  &IsFileChooser,
  &Show0_5,
  &GetNextChosenFile0_5
};

const PPB_FileChooser_Dev_0_6 g_ppb_file_chooser_0_6_thunk = {
  &Create,
  &IsFileChooser,
  &Show
};

const PPB_FileChooserTrusted_0_5 g_ppb_file_chooser_trusted_0_5_thunk = {
  &ShowWithoutUserGesture0_5
};

const PPB_FileChooserTrusted_0_6 g_ppb_file_chooser_trusted_0_6_thunk = {
  &ShowWithoutUserGesture0_6
};

}  // namespace

const PPB_FileChooser_Dev_0_5* GetPPB_FileChooser_Dev_0_5_Thunk() {
  return &g_ppb_file_chooser_0_5_thunk;
}

const PPB_FileChooser_Dev_0_6* GetPPB_FileChooser_Dev_0_6_Thunk() {
  return &g_ppb_file_chooser_0_6_thunk;
}

const PPB_FileChooserTrusted_0_5* GetPPB_FileChooserTrusted_0_5_Thunk() {
  return &g_ppb_file_chooser_trusted_0_5_thunk;
}

const PPB_FileChooserTrusted_0_6* GetPPB_FileChooserTrusted_0_6_Thunk() {
  return &g_ppb_file_chooser_trusted_0_6_thunk;
}

}  // namespace thunk
}  // namespace ppapi
