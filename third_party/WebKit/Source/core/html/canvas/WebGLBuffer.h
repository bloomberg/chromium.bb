/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebGLBuffer_h
#define WebGLBuffer_h

#include "core/html/canvas/WebGLSharedObject.h"

#include <wtf/ArrayBuffer.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class WebGLBuffer : public WebGLSharedObject {
public:
    virtual ~WebGLBuffer();

    static PassRefPtr<WebGLBuffer> create(WebGLRenderingContext*);

    GC3Denum getTarget() const { return m_target; }
    void setTarget(GC3Denum);

    bool hasEverBeenBound() const { return object() && m_target; }

protected:
    WebGLBuffer(WebGLRenderingContext*);

    virtual void deleteObjectImpl(GraphicsContext3D*, Platform3DObject);

private:
    virtual bool isBuffer() const { return true; }

    GC3Denum m_target;
};

} // namespace WebCore

#endif // WebGLBuffer_h
