// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/private/ppb_flash_menu.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_flash_menu_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance, const PP_Flash_Menu* menu_data) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFlashMenu(instance, menu_data);
}

PP_Bool IsFlashMenu(PP_Resource resource) {
  EnterResource<PPB_Flash_Menu_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Show(PP_Resource resource,
             const PP_Point* location,
             int32_t* selected_id,
             PP_CompletionCallback callback) {
  EnterResource<PPB_Flash_Menu_API> enter(resource, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Show(location, selected_id, callback);
  return MayForceCallback(callback, result);
}

const PPB_Flash_Menu g_ppb_flash_menu_thunk = {
  &Create,
  &IsFlashMenu,
  &Show
};

}  // namespace

const PPB_Flash_Menu_0_2* GetPPB_Flash_Menu_0_2_Thunk() {
  return &g_ppb_flash_menu_thunk;
}

}  // namespace thunk
}  // namespace ppapi
