/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef WebGLVertexArrayObjectOES_h
#define WebGLVertexArrayObjectOES_h

#include "core/html/canvas/WebGLBuffer.h"
#include "core/html/canvas/WebGLContextObject.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class WebGLVertexArrayObjectOES final : public WebGLContextObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    enum VaoType {
        VaoTypeDefault,
        VaoTypeUser,
    };

    ~WebGLVertexArrayObjectOES() override;

    static PassRefPtrWillBeRawPtr<WebGLVertexArrayObjectOES> create(WebGLRenderingContextBase*, VaoType);

    Platform3DObject object() const { return m_object; }

    // Cached values for vertex attrib range checks
    class VertexAttribState final {
        ALLOW_ONLY_INLINE_ALLOCATION();
    public:
        VertexAttribState()
            : enabled(false)
            , bytesPerElement(0)
            , size(4)
            , type(GL_FLOAT)
            , normalized(false)
            , stride(16)
            , originalStride(0)
            , offset(0)
            , divisor(0)
        {
        }

        DECLARE_TRACE();

        bool enabled;
        RefPtrWillBeMember<WebGLBuffer> bufferBinding;
        GLsizei bytesPerElement;
        GLint size;
        GLenum type;
        bool normalized;
        GLsizei stride;
        GLsizei originalStride;
        GLintptr offset;
        GLuint divisor;
    };

    bool isDefaultObject() const { return m_type == VaoTypeDefault; }

    bool hasEverBeenBound() const { return object() && m_hasEverBeenBound; }
    void setHasEverBeenBound() { m_hasEverBeenBound = true; }

    PassRefPtrWillBeRawPtr<WebGLBuffer> boundElementArrayBuffer() const { return m_boundElementArrayBuffer; }
    void setElementArrayBuffer(PassRefPtrWillBeRawPtr<WebGLBuffer>);

    VertexAttribState& getVertexAttribState(int index) { return m_vertexAttribState[index]; }
    void setVertexAttribState(GLuint, GLsizei, GLint, GLenum, GLboolean, GLsizei, GLintptr, PassRefPtrWillBeRawPtr<WebGLBuffer>);
    void unbindBuffer(PassRefPtrWillBeRawPtr<WebGLBuffer>);
    void setVertexAttribDivisor(GLuint index, GLuint divisor);

    DECLARE_VIRTUAL_TRACE();

private:
    WebGLVertexArrayObjectOES(WebGLRenderingContextBase*, VaoType);

    void dispatchDetached(blink::WebGraphicsContext3D*);
    bool hasObject() const override { return m_object != 0; }
    void deleteObjectImpl(blink::WebGraphicsContext3D*) override;

    Platform3DObject m_object;

    VaoType m_type;
    bool m_hasEverBeenBound;
#if ENABLE(OILPAN)
    bool m_destructionInProgress;
#endif
    RefPtrWillBeMember<WebGLBuffer> m_boundElementArrayBuffer;
    WillBeHeapVector<VertexAttribState> m_vertexAttribState;
};

} // namespace blink

namespace WTF {

template<>
struct VectorTraits<blink::WebGLVertexArrayObjectOES::VertexAttribState> : SimpleClassVectorTraits<blink::WebGLVertexArrayObjectOES::VertexAttribState> {
    // Must use the constructor.
    static const bool canInitializeWithMemset = false;
    static const bool canCopyWithMemcpy = true;
};

} // namespace WTF

#endif // WebGLVertexArrayObjectOES_h
