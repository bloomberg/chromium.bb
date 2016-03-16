// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockGLES2Interface_h
#define MockGLES2Interface_h

#include "gpu/command_buffer/client/gles2_interface_stub.h"

class MockGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
public:
    GLenum GetGraphicsResetStatusKHR() override
    {
        return m_contextLost ? GL_INVALID_OPERATION : GL_NO_ERROR;
    }

    void setIsContextLost(bool lost) { m_contextLost = lost; }

private:
    bool m_contextLost = false;
};

#endif // MockGLES2Interface_h
