// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/src/shared/ppapi_proxy/view_data.h"

namespace ppapi_proxy {

ViewData::ViewData()
    : viewport_rect(PP_MakeRectFromXYWH(0, 0, 0, 0)),
      is_fullscreen(PP_FALSE),
      is_page_visible(PP_FALSE),
      clip_rect(PP_MakeRectFromXYWH(0, 0, 0, 0)) {
}

}  // namespace ppapi_proxy
