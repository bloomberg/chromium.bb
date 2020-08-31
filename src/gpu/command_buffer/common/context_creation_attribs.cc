// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/context_creation_attribs.h"

#include "base/notreached.h"

namespace gpu {

bool IsGLContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_WEBGL2_COMPUTE:
      return true;
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsWebGLContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_WEBGL2_COMPUTE:
      return true;
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsWebGL1OrES2ContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_OPENGLES2:
      return true;
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL2_COMPUTE:
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsWebGL2OrES3ContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL2:
      return true;
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_WEBGL2_COMPUTE:
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsWebGL2OrES3OrHigherContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_WEBGL2_COMPUTE:
      return true;
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsWebGL2ComputeContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_WEBGL2_COMPUTE:
      return true;
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_WEBGPU:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsWebGPUContextType(ContextType context_type) {
  // Switch statement to cause a compile-time error if we miss a case.
  switch (context_type) {
    case CONTEXT_TYPE_WEBGPU:
      return true;
    case CONTEXT_TYPE_OPENGLES2:
    case CONTEXT_TYPE_OPENGLES3:
    case CONTEXT_TYPE_WEBGL1:
    case CONTEXT_TYPE_WEBGL2:
    case CONTEXT_TYPE_WEBGL2_COMPUTE:
      return false;
  }

  NOTREACHED();
  return false;
}

ContextCreationAttribs::ContextCreationAttribs() = default;

ContextCreationAttribs::ContextCreationAttribs(
    const ContextCreationAttribs& other) = default;

ContextCreationAttribs& ContextCreationAttribs::operator=(
    const ContextCreationAttribs& other) = default;

}  // namespace gpu
