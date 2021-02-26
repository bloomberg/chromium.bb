//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EAGLFunctions.cpp: Exposing the soft-linked EAGL interface.

#include "common/platform.h"

#if defined(ANGLE_ENABLE_EAGL)

#    import <OpenGLES/EAGL.h>
#    import <OpenGLES/EAGLDrawable.h>
#    import <OpenGLES/EAGLIOSurface.h>

#    include "libANGLE/renderer/gl/apple/SoftLinking.h"

SOFT_LINK_FRAMEWORK_SOURCE(OpenGLES)

SOFT_LINK_CLASS(OpenGLES, EAGLContext)

#endif  // defined(ANGLE_ENABLE_EAGL)