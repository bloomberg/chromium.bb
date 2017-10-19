// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGL2RenderingContext.h"

#include <memory>
#include "bindings/modules/v8/offscreen_rendering_context.h"
#include "bindings/modules/v8/rendering_context.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/loader/FrameLoader.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/EXTColorBufferFloat.h"
#include "modules/webgl/EXTDisjointTimerQueryWebGL2.h"
#include "modules/webgl/EXTTextureFilterAnisotropic.h"
#include "modules/webgl/OESTextureFloatLinear.h"
#include "modules/webgl/WebGLCompressedTextureASTC.h"
#include "modules/webgl/WebGLCompressedTextureATC.h"
#include "modules/webgl/WebGLCompressedTextureETC.h"
#include "modules/webgl/WebGLCompressedTextureETC1.h"
#include "modules/webgl/WebGLCompressedTexturePVRTC.h"
#include "modules/webgl/WebGLCompressedTextureS3TC.h"
#include "modules/webgl/WebGLCompressedTextureS3TCsRGB.h"
#include "modules/webgl/WebGLContextAttributeHelpers.h"
#include "modules/webgl/WebGLContextEvent.h"
#include "modules/webgl/WebGLDebugRendererInfo.h"
#include "modules/webgl/WebGLDebugShaders.h"
#include "modules/webgl/WebGLGetBufferSubDataAsync.h"
#include "modules/webgl/WebGLLoseContext.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"

namespace blink {

// An helper function for the two create() methods. The return value is an
// indicate of whether the create() should return nullptr or not.
static bool ShouldCreateContext(WebGraphicsContext3DProvider* context_provider,
                                CanvasRenderingContextHost* host) {
  if (!context_provider) {
    host->HostDispatchEvent(WebGLContextEvent::Create(
        EventTypeNames::webglcontextcreationerror, false, true,
        "Failed to create a WebGL2 context."));
    return false;
  }

  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(gl);
  if (!extensions_util)
    return false;
  if (extensions_util->SupportsExtension("GL_EXT_debug_marker")) {
    String context_label(
        String::Format("WebGL2RenderingContext-%p", context_provider));
    gl->PushGroupMarkerEXT(0, context_label.Ascii().data());
  }
  return true;
}

CanvasRenderingContext* WebGL2RenderingContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributes& attrs) {
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider(
      CreateWebGraphicsContext3DProvider(host, attrs, 2));
  if (!ShouldCreateContext(context_provider.get(), host))
    return nullptr;
  WebGL2RenderingContext* rendering_context =
      new WebGL2RenderingContext(host, std::move(context_provider), attrs);

  if (!rendering_context->GetDrawingBuffer()) {
    host->HostDispatchEvent(WebGLContextEvent::Create(
        EventTypeNames::webglcontextcreationerror, false, true,
        "Could not create a WebGL2 context."));
    return nullptr;
  }

  rendering_context->InitializeNewContext();
  rendering_context->RegisterContextExtensions();

  return rendering_context;
}

void WebGL2RenderingContext::Factory::OnError(HTMLCanvasElement* canvas,
                                              const String& error) {
  canvas->DispatchEvent(WebGLContextEvent::Create(
      EventTypeNames::webglcontextcreationerror, false, true, error));
}

WebGL2RenderingContext::WebGL2RenderingContext(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    const CanvasContextCreationAttributes& requested_attributes)
    : WebGL2RenderingContextBase(host,
                                 std::move(context_provider),
                                 requested_attributes) {}

void WebGL2RenderingContext::SetCanvasGetContextResult(
    RenderingContext& result) {
  result.SetWebGL2RenderingContext(this);
}

void WebGL2RenderingContext::SetOffscreenCanvasGetContextResult(
    OffscreenRenderingContext& result) {
  result.SetWebGL2RenderingContext(this);
}

ImageBitmap* WebGL2RenderingContext::TransferToImageBitmap(
    ScriptState* script_state) {
  return TransferToImageBitmapBase(script_state);
}

void WebGL2RenderingContext::RegisterContextExtensions() {
  // Register extensions.
  RegisterExtension<EXTColorBufferFloat>(ext_color_buffer_float_);
  RegisterExtension<EXTDisjointTimerQueryWebGL2>(
      ext_disjoint_timer_query_web_gl2_);
  RegisterExtension<EXTTextureFilterAnisotropic>(
      ext_texture_filter_anisotropic_);
  RegisterExtension<OESTextureFloatLinear>(oes_texture_float_linear_);
  RegisterExtension<WebGLCompressedTextureASTC>(webgl_compressed_texture_astc_);
  RegisterExtension<WebGLCompressedTextureATC>(webgl_compressed_texture_atc_);
  RegisterExtension<WebGLCompressedTextureETC>(webgl_compressed_texture_etc_);
  RegisterExtension<WebGLCompressedTextureETC1>(webgl_compressed_texture_etc1_);
  RegisterExtension<WebGLCompressedTexturePVRTC>(
      webgl_compressed_texture_pvrtc_);
  RegisterExtension<WebGLCompressedTextureS3TC>(webgl_compressed_texture_s3tc_);
  RegisterExtension<WebGLCompressedTextureS3TCsRGB>(
      webgl_compressed_texture_s3tc_srgb_);
  RegisterExtension<WebGLDebugRendererInfo>(webgl_debug_renderer_info_);
  RegisterExtension<WebGLDebugShaders>(webgl_debug_shaders_);
  RegisterExtension<WebGLGetBufferSubDataAsync>(
      webgl_get_buffer_sub_data_async_, kDraftExtension);
  RegisterExtension<WebGLLoseContext>(webgl_lose_context_);
}

void WebGL2RenderingContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(ext_color_buffer_float_);
  visitor->Trace(ext_disjoint_timer_query_web_gl2_);
  visitor->Trace(ext_texture_filter_anisotropic_);
  visitor->Trace(oes_texture_float_linear_);
  visitor->Trace(webgl_compressed_texture_astc_);
  visitor->Trace(webgl_compressed_texture_atc_);
  visitor->Trace(webgl_compressed_texture_etc_);
  visitor->Trace(webgl_compressed_texture_etc1_);
  visitor->Trace(webgl_compressed_texture_pvrtc_);
  visitor->Trace(webgl_compressed_texture_s3tc_);
  visitor->Trace(webgl_compressed_texture_s3tc_srgb_);
  visitor->Trace(webgl_debug_renderer_info_);
  visitor->Trace(webgl_debug_shaders_);
  visitor->Trace(webgl_get_buffer_sub_data_async_);
  visitor->Trace(webgl_lose_context_);
  WebGL2RenderingContextBase::Trace(visitor);
}

void WebGL2RenderingContext::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  // Extensions are managed by WebGL2RenderingContextBase.
  WebGL2RenderingContextBase::TraceWrappers(visitor);
}

}  // namespace blink
