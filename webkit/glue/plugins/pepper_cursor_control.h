// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_CURSOR_CONTROL_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_CURSOR_CONTROL_H_

typedef struct _ppb_CursorControl PPB_CursorControl;

namespace pepper {

// There's no class implementing CursorControl so we just expose a getter for
// the interface implemented in the .cc file here.
const PPB_CursorControl* GetCursorControlInterface();

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_CURSOR_CONTROL_H_

