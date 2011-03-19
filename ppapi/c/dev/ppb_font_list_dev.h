/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_FONT_LIST_DEV_H_
#define PPAPI_C_DEV_PPB_FONT_LIST_DEV_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

#define PPB_FONT_LIST_DEV_INTERFACE "PPB_FontList(Dev);0.1"

struct PPB_FontList_Dev {
  /* Creates a font list resource. By default, the list will be empty, you must
   * call Initialize before calling any other functions to actually populate
   * the fonts.
   */
  PP_Resource (*Create)(PP_Instance instance);

  /* Actually retrieves the fonts associated with this resource. This function
   * will complete asynchronously (it may be slow to get all of the fonts from
   * the operating system).
   */
  int32_t (*Initialize)(PP_Resource font_list,
                        struct PP_CompletionCallback completion);

  /* Returns the number of entries in the font list. If you call on an invalid
   * resource or a FontList that has not issued its create completion callback
   * yet, it will return 0.
   */
  int32_t (*GetListSize)(PP_Resource font_list);

  /* Retrieves the font family name at the given index, returning it in a
   * string var. Valid indices are [0, GetListSize). Passing an invalid
   * resource, an out-of-range index, or a FontList that has not issued its
   * create completion callback from Initialize() yet will result in a Null var
   * being returned.
   */
  struct PP_Var (*GetFontFaceAt)(PP_Resource font_list, int32_t index);
};

#endif  /* PPAPI_C_DEV_PPB_FONT_LIST_DEV_H_ */

