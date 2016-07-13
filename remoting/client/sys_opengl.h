// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_SYS_OPENGL_H_
#define REMOTING_CLIENT_SYS_OPENGL_H_

#include "build/build_config.h"

#if defined(OS_IOS)
#include <OpenGLES/ES3/gl.h>
#elif defined(OS_ANDROID)
#include <GLES3/gl3.h>
#else
#error OpenGL renderer not supported on this platform.
#endif

#endif  // REMOTING_CLIENT_SYS_OPENGL_H_
