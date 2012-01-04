// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_VIEW_DATA_H_
#define PPAPI_NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_VIEW_DATA_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_rect.h"

namespace ppapi_proxy {

struct ViewData {
  ViewData();

  PP_Rect viewport_rect;
  PP_Bool is_fullscreen;
  PP_Bool is_page_visible;
  PP_Rect clip_rect;
};

}  // namespace ppapi_proxy

#endif  // PPAPI_NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_VIEW_DATA_H_
