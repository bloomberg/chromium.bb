// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_switches.h"

namespace gfx {

std::string GLContext::GetExtensions() {
  DCHECK(IsCurrent(NULL));
  const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  return std::string(ext ? ext : "");
}

bool GLContext::HasExtension(const char* name) {
  std::string extensions = GetExtensions();
  extensions += " ";

  std::string delimited_name(name);
  delimited_name += " ";

  return extensions.find(delimited_name) != std::string::npos;
}

bool GLContext::LosesAllContextsOnContextLost()
{
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      return false;
    case kGLImplementationEGLGLES2:
      return true;
    case kGLImplementationOSMesaGL:
      return false;
    case kGLImplementationMockGL:
      return false;
    default:
      NOTREACHED();
      return true;
  }
}

}  // namespace gfx
