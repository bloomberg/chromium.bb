// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_truetype_font_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_truetype_font_api.h"
#include "ppapi/thunk/ppb_truetype_font_singleton_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t GetFontFamilies(PP_Instance instance,
                        PP_ArrayOutput output,
                        PP_CompletionCallback callback) {
  EnterInstanceAPI<PPB_TrueTypeFont_Singleton_API> enter(instance, callback);
  if (enter.failed())
    return PP_ERROR_FAILED;
  return enter.functions()->GetFontFamilies(instance, output, enter.callback());
}

PP_Resource Create(PP_Instance instance,
                   const PP_TrueTypeFontDesc_Dev* desc) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateTrueTypeFont(instance, *desc);
}

PP_Bool IsFont(PP_Resource resource) {
  EnterResource<PPB_TrueTypeFont_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Describe(PP_Resource font,
                 PP_TrueTypeFontDesc_Dev* desc,
                 PP_CompletionCallback callback) {
  EnterResource<PPB_TrueTypeFont_API> enter(font, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Describe(desc, enter.callback()));
}

int32_t GetTableTags(PP_Resource font,
                     PP_ArrayOutput output,
                     PP_CompletionCallback callback) {
  EnterResource<PPB_TrueTypeFont_API> enter(font, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetTableTags(output,
                                                      enter.callback()));
}

int32_t GetTable(PP_Resource font,
                 uint32_t table,
                 int32_t offset,
                 int32_t max_data_length,
                 PP_ArrayOutput output,
                 PP_CompletionCallback callback) {
  EnterResource<PPB_TrueTypeFont_API> enter(font, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetTable(table,
                                                  offset,
                                                  max_data_length,
                                                  output,
                                                  enter.callback()));
}

const PPB_TrueTypeFont_Dev_0_1 g_ppb_truetypefont_thunk_0_1 = {
  &GetFontFamilies,
  &Create,
  &IsFont,
  &Describe,
  &GetTableTags,
  &GetTable
};

}  // namespace

const PPB_TrueTypeFont_Dev_0_1* GetPPB_TrueTypeFont_Dev_0_1_Thunk() {
  return &g_ppb_truetypefont_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
