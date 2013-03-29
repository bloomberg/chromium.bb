// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From trusted/ppb_file_chooser_trusted.idl,
//   modified Fri Mar 29 09:14:05 2013.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_chooser_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t ShowWithoutUserGesture_0_5(PP_Resource chooser,
                                   PP_Bool save_as,
                                   struct PP_Var suggested_file_name,
                                   struct PP_CompletionCallback callback) {
  EnterResource<PPB_FileChooser_API> enter(chooser, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->ShowWithoutUserGesture0_5(
      save_as,
      suggested_file_name,
      enter.callback()));
}

int32_t ShowWithoutUserGesture(PP_Resource chooser,
                               PP_Bool save_as,
                               struct PP_Var suggested_file_name,
                               struct PP_ArrayOutput output,
                               struct PP_CompletionCallback callback) {
  EnterResource<PPB_FileChooser_API> enter(chooser, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->ShowWithoutUserGesture(
      save_as,
      suggested_file_name,
      output,
      enter.callback()));
}

const PPB_FileChooserTrusted_0_5 g_ppb_filechoosertrusted_thunk_0_5 = {
  &ShowWithoutUserGesture_0_5
};

const PPB_FileChooserTrusted_0_6 g_ppb_filechoosertrusted_thunk_0_6 = {
  &ShowWithoutUserGesture
};

}  // namespace

const PPB_FileChooserTrusted_0_5* GetPPB_FileChooserTrusted_0_5_Thunk() {
  return &g_ppb_filechoosertrusted_thunk_0_5;
}

const PPB_FileChooserTrusted_0_6* GetPPB_FileChooserTrusted_0_6_Thunk() {
  return &g_ppb_filechoosertrusted_thunk_0_6;
}

}  // namespace thunk
}  // namespace ppapi
