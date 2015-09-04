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

#ifndef WebGLRenderingContext_h
#define WebGLRenderingContext_h

#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

class CanvasContextCreationAttributes;

class WebGLRenderingContext final : public WebGLRenderingContextBase {
    DEFINE_WRAPPERTYPEINFO();
public:
    class Factory : public CanvasRenderingContextFactory {
        WTF_MAKE_NONCOPYABLE(Factory);
    public:
        Factory() {}
        ~Factory() override {}

        PassOwnPtrWillBeRawPtr<CanvasRenderingContext> create(HTMLCanvasElement*, const CanvasContextCreationAttributes&, Document&) override;
        CanvasRenderingContext::ContextType contextType() const override { return CanvasRenderingContext::ContextWebgl; }
        void onError(HTMLCanvasElement*, const String& error) override;
    };

    ~WebGLRenderingContext() override;

    CanvasRenderingContext::ContextType contextType() const override { return CanvasRenderingContext::ContextWebgl; }
    unsigned version() const override { return 1; }
    String contextName() const override { return "WebGLRenderingContext"; }
    void registerContextExtensions() override;

    EAGERLY_FINALIZE();
    DECLARE_VIRTUAL_TRACE();

private:
    WebGLRenderingContext(HTMLCanvasElement*, PassOwnPtr<WebGraphicsContext3D>, const WebGLContextAttributes&);

    // Enabled extension objects.
    PersistentWillBeMember<ANGLEInstancedArrays> m_angleInstancedArrays;
    PersistentWillBeMember<CHROMIUMSubscribeUniform> m_chromiumSubscribeUniform;
    PersistentWillBeMember<EXTBlendMinMax> m_extBlendMinMax;
    PersistentWillBeMember<EXTDisjointTimerQuery> m_extDisjointTimerQuery;
    PersistentWillBeMember<EXTFragDepth> m_extFragDepth;
    PersistentWillBeMember<EXTShaderTextureLOD> m_extShaderTextureLOD;
    PersistentWillBeMember<EXTsRGB> m_extsRGB;
    PersistentWillBeMember<EXTTextureFilterAnisotropic> m_extTextureFilterAnisotropic;
    PersistentWillBeMember<OESTextureFloat> m_oesTextureFloat;
    PersistentWillBeMember<OESTextureFloatLinear> m_oesTextureFloatLinear;
    PersistentWillBeMember<OESTextureHalfFloat> m_oesTextureHalfFloat;
    PersistentWillBeMember<OESTextureHalfFloatLinear> m_oesTextureHalfFloatLinear;
    PersistentWillBeMember<OESStandardDerivatives> m_oesStandardDerivatives;
    PersistentWillBeMember<OESVertexArrayObject> m_oesVertexArrayObject;
    PersistentWillBeMember<OESElementIndexUint> m_oesElementIndexUint;
    PersistentWillBeMember<WebGLLoseContext> m_webglLoseContext;
    PersistentWillBeMember<WebGLDebugRendererInfo> m_webglDebugRendererInfo;
    PersistentWillBeMember<WebGLDebugShaders> m_webglDebugShaders;
    PersistentWillBeMember<WebGLDrawBuffers> m_webglDrawBuffers;
    PersistentWillBeMember<WebGLCompressedTextureASTC> m_webglCompressedTextureASTC;
    PersistentWillBeMember<WebGLCompressedTextureATC> m_webglCompressedTextureATC;
    PersistentWillBeMember<WebGLCompressedTextureETC1> m_webglCompressedTextureETC1;
    PersistentWillBeMember<WebGLCompressedTexturePVRTC> m_webglCompressedTexturePVRTC;
    PersistentWillBeMember<WebGLCompressedTextureS3TC> m_webglCompressedTextureS3TC;
    PersistentWillBeMember<WebGLDepthTexture> m_webglDepthTexture;
};

DEFINE_TYPE_CASTS(WebGLRenderingContext, CanvasRenderingContext, context,
    context->is3d() && WebGLRenderingContextBase::getWebGLVersion(context) == 1,
    context.is3d() && WebGLRenderingContextBase::getWebGLVersion(&context) == 1);

} // namespace blink

#endif // WebGLRenderingContext_h
