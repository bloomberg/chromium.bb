// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"

static const struct PPB_OpenGLES_Dev* g_gles2_interface;

int GL_APIENTRY glInitializePPAPI(
    PP_Module module,
    PPB_GetInterface get_browser_interface) {
  return 0;
}

int GL_APIENTRY glTerminatePPAPI() {
  return 0;
}

void GL_APIENTRY glSetCurrentContextPPAPI(PP_Resource context) {
}

PP_Resource GL_APIENTRY glGetCurrentContextPPAPI() {
  return 0;
}

const struct PPB_OpenGLES_Dev* GL_APIENTRY glGetInterfacePPAPI() {
  return g_gles2_interface;
}

