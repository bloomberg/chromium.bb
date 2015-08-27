// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/platform/native/mgl_onscreen_thunks.h"

#include <assert.h>

#include "mojo/public/platform/native/thunk_export.h"

static struct MGLOnscreenThunks g_onscreen_thunks = {0};

void MGLResizeSurface(uint32_t width, uint32_t height) {
  assert(g_onscreen_thunks.MGLResizeSurface);
  g_onscreen_thunks.MGLResizeSurface(width, height);
}

void MGLSwapBuffers(void) {
  assert(g_onscreen_thunks.MGLSwapBuffers);
  g_onscreen_thunks.MGLSwapBuffers();
}

THUNK_EXPORT size_t MojoSetMGLOnscreenThunks(
    const struct MGLOnscreenThunks* mgl_onscreen_thunks) {
  if (mgl_onscreen_thunks->size >= sizeof(g_onscreen_thunks))
    g_onscreen_thunks = *mgl_onscreen_thunks;
  return sizeof(g_onscreen_thunks);
}
