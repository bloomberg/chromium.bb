// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_CURSOR_CONTROL_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_CURSOR_CONTROL_IMPL_H_

struct PPB_CursorControl_Dev;

namespace webkit {
namespace ppapi {

// There's no class implementing CursorControl so we just expose a getter for
// the interface implemented in the .cc file here.
const PPB_CursorControl_Dev* GetCursorControlInterface();

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_CURSOR_CONTROL_IMPL_H_

