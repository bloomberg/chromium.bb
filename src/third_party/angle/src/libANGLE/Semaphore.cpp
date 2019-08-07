//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Semaphore.h: Implements the gl::Semaphore class [EXT_external_objects]

#include "libANGLE/Semaphore.h"

#include "common/angleutils.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/SemaphoreImpl.h"

namespace gl
{

Semaphore::Semaphore(rx::GLImplFactory *factory, GLuint id)
    : RefCountObject(id), mImplementation(factory->createSemaphore())
{}

Semaphore::~Semaphore() {}

void Semaphore::onDestroy(const Context *context)
{
    mImplementation->onDestroy(context);
}

angle::Result Semaphore::importFd(Context *context, HandleType handleType, GLint fd)
{
    return mImplementation->importFd(context, handleType, fd);
}

}  // namespace gl
