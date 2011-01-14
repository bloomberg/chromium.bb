/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPP_FIND_DEV_H_
#define PPAPI_C_DEV_PPP_FIND_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"

#define PPP_FIND_DEV_INTERFACE "PPP_Find(Dev);0.3"

struct PPP_Find_Dev {
  // Finds the given UTF-8 text starting at the current selection. The number of
  // results will be updated asynchronously via NumberOfFindResultsChanged in
  // PPB_Find. Note that multiple StartFind calls can happen before StopFind is
  // called in the case of the search term changing.
  //
  // Return PP_FALSE if the plugin doesn't support find in page. Consequently,
  // it won't call any callbacks.
  PP_Bool (*StartFind)(PP_Instance instance,
                       const char* text,
                       PP_Bool case_sensitive);

  // Go to the next/previous result.
  void (*SelectFindResult)(PP_Instance instance,
                           PP_Bool forward);

  // Tells the plugin that the find operation has stopped, so it should clear
  // any highlighting.
  void (*StopFind)(PP_Instance instance);
};

#endif  /* PPAPI_C_DEV_PPP_FIND_DEV_H_ */

