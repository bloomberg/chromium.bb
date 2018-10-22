//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SyncVk.cpp:
//    Implements the class methods for SyncVk.
//

#include "libANGLE/renderer/vulkan/SyncVk.h"

#include "common/debug.h"

namespace rx
{

SyncVk::SyncVk() : SyncImpl()
{
}

SyncVk::~SyncVk()
{
}

gl::Error SyncVk::set(const gl::Context *context, GLenum condition, GLbitfield flags)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error SyncVk::clientWait(const gl::Context *context,
                             GLbitfield flags,
                             GLuint64 timeout,
                             GLenum *outResult)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error SyncVk::serverWait(const gl::Context *context, GLbitfield flags, GLuint64 timeout)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error SyncVk::getStatus(const gl::Context *context, GLint *outResult)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

}  // namespace rx
