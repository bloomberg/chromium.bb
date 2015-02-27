// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/canvas/WebGL2RenderingContext.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/canvas/CHROMIUMSubscribeUniform.h"
#include "core/html/canvas/ContextAttributeHelpers.h"
#include "core/html/canvas/EXTTextureFilterAnisotropic.h"
#include "core/html/canvas/OESTextureFloatLinear.h"
#include "core/html/canvas/WebGLCompressedTextureATC.h"
#include "core/html/canvas/WebGLCompressedTextureETC1.h"
#include "core/html/canvas/WebGLCompressedTexturePVRTC.h"
#include "core/html/canvas/WebGLCompressedTextureS3TC.h"
#include "core/html/canvas/WebGLContextEvent.h"
#include "core/html/canvas/WebGLDebugRendererInfo.h"
#include "core/html/canvas/WebGLDebugShaders.h"
#include "core/html/canvas/WebGLLoseContext.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "public/platform/Platform.h"

namespace blink {

PassOwnPtrWillBeRawPtr<WebGL2RenderingContext> WebGL2RenderingContext::create(HTMLCanvasElement* canvas, const CanvasContextCreationAttributes& attrs)
{
    if (!RuntimeEnabledFeatures::unsafeES3APIsEnabled()) {
        canvas->dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextcreationerror, false, true, "Creation of WebGL2 contexts disabled."));
        return nullptr;
    }

    WebGLContextAttributes attributes = toWebGLContextAttributes(attrs);
    OwnPtr<blink::WebGraphicsContext3D> context(createWebGraphicsContext3D(canvas, attributes, 2));
    if (!context)
        return nullptr;
    OwnPtr<Extensions3DUtil> extensionsUtil = Extensions3DUtil::create(context.get());
    if (!extensionsUtil)
        return nullptr;
    if (extensionsUtil->supportsExtension("GL_EXT_debug_marker")) {
        String contextLabel(String::format("WebGL2RenderingContext-%p", context.get()));
        context->pushGroupMarkerEXT(contextLabel.ascii().data());
    }

    OwnPtrWillBeRawPtr<WebGL2RenderingContext> renderingContext = adoptPtrWillBeNoop(new WebGL2RenderingContext(canvas, context.release(), attributes));
    renderingContext->registerContextExtensions();

    if (!renderingContext->drawingBuffer()) {
        canvas->dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextcreationerror, false, true, "Could not create a WebGL2 context."));
        return nullptr;
    }

    return renderingContext.release();
}

WebGL2RenderingContext::WebGL2RenderingContext(HTMLCanvasElement* passedCanvas, PassOwnPtr<blink::WebGraphicsContext3D> context, const WebGLContextAttributes& requestedAttributes)
    : WebGL2RenderingContextBase(passedCanvas, context, requestedAttributes)
{
}

WebGL2RenderingContext::~WebGL2RenderingContext()
{

}

void WebGL2RenderingContext::registerContextExtensions()
{
    // Register extensions.
    registerExtension<CHROMIUMSubscribeUniform>(m_chromiumSubscribeUniform);
    registerExtension<EXTTextureFilterAnisotropic>(m_extTextureFilterAnisotropic);
    registerExtension<OESTextureFloatLinear>(m_oesTextureFloatLinear);
    registerExtension<WebGLCompressedTextureATC>(m_webglCompressedTextureATC);
    registerExtension<WebGLCompressedTextureETC1>(m_webglCompressedTextureETC1);
    registerExtension<WebGLCompressedTexturePVRTC>(m_webglCompressedTexturePVRTC);
    registerExtension<WebGLCompressedTextureS3TC>(m_webglCompressedTextureS3TC);
    registerExtension<WebGLDebugRendererInfo>(m_webglDebugRendererInfo);
    registerExtension<WebGLDebugShaders>(m_webglDebugShaders);
    registerExtension<WebGLLoseContext>(m_webglLoseContext);
}

DEFINE_TRACE(WebGL2RenderingContext)
{
    visitor->trace(m_chromiumSubscribeUniform);
    visitor->trace(m_extTextureFilterAnisotropic);
    visitor->trace(m_oesTextureFloatLinear);
    visitor->trace(m_webglCompressedTextureATC);
    visitor->trace(m_webglCompressedTextureETC1);
    visitor->trace(m_webglCompressedTexturePVRTC);
    visitor->trace(m_webglCompressedTextureS3TC);
    visitor->trace(m_webglDebugRendererInfo);
    visitor->trace(m_webglDebugShaders);
    visitor->trace(m_webglLoseContext);
    WebGL2RenderingContextBase::trace(visitor);
}

} // namespace blink
