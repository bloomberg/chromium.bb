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

#include "modules/webgl/WebGLRenderingContext.h"

#include <memory>
#include "bindings/modules/v8/offscreen_rendering_context.h"
#include "bindings/modules/v8/rendering_context.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/loader/FrameLoader.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/ANGLEInstancedArrays.h"
#include "modules/webgl/EXTBlendMinMax.h"
#include "modules/webgl/EXTColorBufferHalfFloat.h"
#include "modules/webgl/EXTDisjointTimerQuery.h"
#include "modules/webgl/EXTFragDepth.h"
#include "modules/webgl/EXTShaderTextureLOD.h"
#include "modules/webgl/EXTTextureFilterAnisotropic.h"
#include "modules/webgl/EXTsRGB.h"
#include "modules/webgl/OESElementIndexUint.h"
#include "modules/webgl/OESStandardDerivatives.h"
#include "modules/webgl/OESTextureFloat.h"
#include "modules/webgl/OESTextureFloatLinear.h"
#include "modules/webgl/OESTextureHalfFloat.h"
#include "modules/webgl/OESTextureHalfFloatLinear.h"
#include "modules/webgl/OESVertexArrayObject.h"
#include "modules/webgl/WebGLColorBufferFloat.h"
#include "modules/webgl/WebGLCompressedTextureASTC.h"
#include "modules/webgl/WebGLCompressedTextureATC.h"
#include "modules/webgl/WebGLCompressedTextureETC.h"
#include "modules/webgl/WebGLCompressedTextureETC1.h"
#include "modules/webgl/WebGLCompressedTexturePVRTC.h"
#include "modules/webgl/WebGLCompressedTextureS3TC.h"
#include "modules/webgl/WebGLCompressedTextureS3TCsRGB.h"
#include "modules/webgl/WebGLContextEvent.h"
#include "modules/webgl/WebGLDebugRendererInfo.h"
#include "modules/webgl/WebGLDebugShaders.h"
#include "modules/webgl/WebGLDepthTexture.h"
#include "modules/webgl/WebGLDrawBuffers.h"
#include "modules/webgl/WebGLLoseContext.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "platform/wtf/CheckedNumeric.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"

namespace blink {

// An helper function for the two create() methods. The return value is an
// indicate of whether the create() should return nullptr or not.
static bool ShouldCreateContext(
    WebGraphicsContext3DProvider* context_provider) {
  if (!context_provider)
    return false;
  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(gl);
  if (!extensions_util)
    return false;
  if (extensions_util->SupportsExtension("GL_EXT_debug_marker")) {
    String context_label(
        String::Format("WebGLRenderingContext-%p", context_provider));
    gl->PushGroupMarkerEXT(0, context_label.Ascii().data());
  }
  return true;
}

CanvasRenderingContext* WebGLRenderingContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributes& attrs) {
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider(
      CreateWebGraphicsContext3DProvider(host, attrs, 1));
  if (!ShouldCreateContext(context_provider.get()))
    return nullptr;

  WebGLRenderingContext* rendering_context =
      new WebGLRenderingContext(host, std::move(context_provider), attrs);
  if (!rendering_context->GetDrawingBuffer()) {
    host->HostDispatchEvent(WebGLContextEvent::Create(
        EventTypeNames::webglcontextcreationerror, false, true,
        "Could not create a WebGL context."));
    return nullptr;
  }
  rendering_context->InitializeNewContext();
  rendering_context->RegisterContextExtensions();

  return rendering_context;
}

void WebGLRenderingContext::Factory::OnError(HTMLCanvasElement* canvas,
                                             const String& error) {
  canvas->DispatchEvent(WebGLContextEvent::Create(
      EventTypeNames::webglcontextcreationerror, false, true, error));
}

WebGLRenderingContext::WebGLRenderingContext(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    const CanvasContextCreationAttributes& requested_attributes)
    : WebGLRenderingContextBase(host,
                                std::move(context_provider),
                                requested_attributes,
                                1) {}

void WebGLRenderingContext::SetCanvasGetContextResult(
    RenderingContext& result) {
  result.SetWebGLRenderingContext(this);
}

void WebGLRenderingContext::SetOffscreenCanvasGetContextResult(
    OffscreenRenderingContext& result) {
  result.SetWebGLRenderingContext(this);
}

ImageBitmap* WebGLRenderingContext::TransferToImageBitmap(
    ScriptState* script_state) {
  return TransferToImageBitmapBase(script_state);
}

void WebGLRenderingContext::RegisterContextExtensions() {
  // Register extensions.
  static const char* const kBothPrefixes[] = {
      "", "WEBKIT_", 0,
  };

  RegisterExtension<ANGLEInstancedArrays>(angle_instanced_arrays_);
  RegisterExtension<EXTBlendMinMax>(ext_blend_min_max_);
  RegisterExtension<EXTColorBufferHalfFloat>(ext_color_buffer_half_float_);
  RegisterExtension<EXTDisjointTimerQuery>(ext_disjoint_timer_query_);
  RegisterExtension<EXTFragDepth>(ext_frag_depth_);
  RegisterExtension<EXTShaderTextureLOD>(ext_shader_texture_lod_);
  RegisterExtension<EXTTextureFilterAnisotropic>(
      ext_texture_filter_anisotropic_, kApprovedExtension, kBothPrefixes);
  RegisterExtension<EXTsRGB>(exts_rgb_);
  RegisterExtension<OESElementIndexUint>(oes_element_index_uint_);
  RegisterExtension<OESStandardDerivatives>(oes_standard_derivatives_);
  RegisterExtension<OESTextureFloat>(oes_texture_float_);
  RegisterExtension<OESTextureFloatLinear>(oes_texture_float_linear_);
  RegisterExtension<OESTextureHalfFloat>(oes_texture_half_float_);
  RegisterExtension<OESTextureHalfFloatLinear>(oes_texture_half_float_linear_);
  RegisterExtension<OESVertexArrayObject>(oes_vertex_array_object_);
  RegisterExtension<WebGLColorBufferFloat>(webgl_color_buffer_float_);
  RegisterExtension<WebGLCompressedTextureASTC>(webgl_compressed_texture_astc_);
  RegisterExtension<WebGLCompressedTextureATC>(
      webgl_compressed_texture_atc_, kApprovedExtension, kBothPrefixes);
  RegisterExtension<WebGLCompressedTextureETC>(webgl_compressed_texture_etc_,
                                               kDraftExtension);
  RegisterExtension<WebGLCompressedTextureETC1>(webgl_compressed_texture_etc1_);
  RegisterExtension<WebGLCompressedTexturePVRTC>(
      webgl_compressed_texture_pvrtc_, kApprovedExtension, kBothPrefixes);
  RegisterExtension<WebGLCompressedTextureS3TC>(
      webgl_compressed_texture_s3tc_, kApprovedExtension, kBothPrefixes);
  RegisterExtension<WebGLCompressedTextureS3TCsRGB>(
      webgl_compressed_texture_s3tc_srgb_);
  RegisterExtension<WebGLDebugRendererInfo>(webgl_debug_renderer_info_);
  RegisterExtension<WebGLDebugShaders>(webgl_debug_shaders_);
  RegisterExtension<WebGLDepthTexture>(webgl_depth_texture_, kApprovedExtension,
                                       kBothPrefixes);
  RegisterExtension<WebGLDrawBuffers>(webgl_draw_buffers_);
  RegisterExtension<WebGLLoseContext>(webgl_lose_context_, kApprovedExtension,
                                      kBothPrefixes);
}

void WebGLRenderingContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(angle_instanced_arrays_);
  visitor->Trace(ext_blend_min_max_);
  visitor->Trace(ext_color_buffer_half_float_);
  visitor->Trace(ext_disjoint_timer_query_);
  visitor->Trace(ext_frag_depth_);
  visitor->Trace(ext_shader_texture_lod_);
  visitor->Trace(ext_texture_filter_anisotropic_);
  visitor->Trace(exts_rgb_);
  visitor->Trace(oes_element_index_uint_);
  visitor->Trace(oes_standard_derivatives_);
  visitor->Trace(oes_texture_float_);
  visitor->Trace(oes_texture_float_linear_);
  visitor->Trace(oes_texture_half_float_);
  visitor->Trace(oes_texture_half_float_linear_);
  visitor->Trace(oes_vertex_array_object_);
  visitor->Trace(webgl_color_buffer_float_);
  visitor->Trace(webgl_compressed_texture_astc_);
  visitor->Trace(webgl_compressed_texture_atc_);
  visitor->Trace(webgl_compressed_texture_etc_);
  visitor->Trace(webgl_compressed_texture_etc1_);
  visitor->Trace(webgl_compressed_texture_pvrtc_);
  visitor->Trace(webgl_compressed_texture_s3tc_);
  visitor->Trace(webgl_compressed_texture_s3tc_srgb_);
  visitor->Trace(webgl_debug_renderer_info_);
  visitor->Trace(webgl_debug_shaders_);
  visitor->Trace(webgl_depth_texture_);
  visitor->Trace(webgl_draw_buffers_);
  visitor->Trace(webgl_lose_context_);
  WebGLRenderingContextBase::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(WebGLRenderingContext) {
  // Extensions are managed base WebGLRenderingContextBase.
  WebGLRenderingContextBase::TraceWrappers(visitor);
}

}  // namespace blink
