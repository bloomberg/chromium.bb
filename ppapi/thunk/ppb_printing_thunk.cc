// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/pp_print_settings_dev.h"
#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_printing_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool GetDefaultPrintSettings_0_6(PP_Instance instance,
                                    PP_PrintSettings_Dev* print_settings) {
  // TODO(raymes): This is obsolete now. Just return some default settings.
  // Remove this when all versions of Flash we care about are no longer using
  // it.
  PP_PrintSettings_Dev default_settings = {
    // |printable_area|: all of the sheet of paper.
    { { 0, 0 }, { 612, 792 } },
    // |content_area|: 0.5" margins all around.
    { { 36, 36 }, { 540, 720 } },
    // |paper_size|: 8.5" x 11" (US letter).
    { 612, 792 },
    300,  // |dpi|.
    PP_PRINTORIENTATION_NORMAL,  // |orientation|.
    PP_PRINTSCALINGOPTION_NONE,  // |print_scaling_option|.
    PP_FALSE,  // |grayscale|.
    PP_PRINTOUTPUTFORMAT_PDF  // |format|.
  };
  *print_settings = default_settings;
  return PP_TRUE;
}

PP_Resource Create(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreatePrinting(instance);
}

int32_t GetDefaultPrintSettings(PP_Resource resource,
                                PP_PrintSettings_Dev* print_settings,
                                PP_CompletionCallback callback) {
  EnterResource<PPB_Printing_API> enter(resource, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetDefaultPrintSettings(print_settings,
                                              enter.callback()));
}

const PPB_Printing_Dev_0_6 g_ppb_printing_dev_thunk_0_6 = {
  &GetDefaultPrintSettings_0_6,
};

const PPB_Printing_Dev g_ppb_printing_dev_thunk_0_7 = {
  &Create,
  &GetDefaultPrintSettings,
};

}  // namespace

const PPB_Printing_Dev_0_6* GetPPB_Printing_Dev_0_6_Thunk() {
  return &g_ppb_printing_dev_thunk_0_6;
}

const PPB_Printing_Dev_0_7* GetPPB_Printing_Dev_0_7_Thunk() {
  return &g_ppb_printing_dev_thunk_0_7;
}

}  // namespace thunk
}  // namespace ppapi
