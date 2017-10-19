// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGL2RenderingContext_h
#define WebGL2RenderingContext_h

#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"
#include <memory>

namespace blink {

class CanvasContextCreationAttributes;
class EXTColorBufferFloat;
class EXTTextureFilterAnisotropic;
class OESTextureFloatLinear;
class WebGLDebugRendererInfo;
class WebGLLoseContext;

class WebGL2RenderingContext : public WebGL2RenderingContextBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
    WTF_MAKE_NONCOPYABLE(Factory);

   public:
    Factory() {}
    ~Factory() override {}

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost*,
        const CanvasContextCreationAttributes&) override;
    CanvasRenderingContext::ContextType GetContextType() const override {
      return CanvasRenderingContext::kContextWebgl2;
    }
    void OnError(HTMLCanvasElement*, const String& error) override;
  };

  CanvasRenderingContext::ContextType GetContextType() const override {
    return CanvasRenderingContext::kContextWebgl2;
  }
  ImageBitmap* TransferToImageBitmap(ScriptState*) final;
  String ContextName() const override { return "WebGL2RenderingContext"; }
  void RegisterContextExtensions() override;
  void SetCanvasGetContextResult(RenderingContext&) final;
  void SetOffscreenCanvasGetContextResult(OffscreenRenderingContext&) final;

  virtual void Trace(blink::Visitor*);

  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

 protected:
  WebGL2RenderingContext(
      CanvasRenderingContextHost*,
      std::unique_ptr<WebGraphicsContext3DProvider>,
      const CanvasContextCreationAttributes& requested_attributes);

  Member<EXTColorBufferFloat> ext_color_buffer_float_;
  Member<EXTDisjointTimerQueryWebGL2> ext_disjoint_timer_query_web_gl2_;
  Member<EXTTextureFilterAnisotropic> ext_texture_filter_anisotropic_;
  Member<OESTextureFloatLinear> oes_texture_float_linear_;
  Member<WebGLCompressedTextureASTC> webgl_compressed_texture_astc_;
  Member<WebGLCompressedTextureATC> webgl_compressed_texture_atc_;
  Member<WebGLCompressedTextureETC> webgl_compressed_texture_etc_;
  Member<WebGLCompressedTextureETC1> webgl_compressed_texture_etc1_;
  Member<WebGLCompressedTexturePVRTC> webgl_compressed_texture_pvrtc_;
  Member<WebGLCompressedTextureS3TC> webgl_compressed_texture_s3tc_;
  Member<WebGLCompressedTextureS3TCsRGB> webgl_compressed_texture_s3tc_srgb_;
  Member<WebGLDebugRendererInfo> webgl_debug_renderer_info_;
  Member<WebGLDebugShaders> webgl_debug_shaders_;
  Member<WebGLGetBufferSubDataAsync> webgl_get_buffer_sub_data_async_;
  Member<WebGLLoseContext> webgl_lose_context_;
};

DEFINE_TYPE_CASTS(WebGL2RenderingContext,
                  CanvasRenderingContext,
                  context,
                  context->Is3d() &&
                      WebGLRenderingContextBase::GetWebGLVersion(context) == 2,
                  context.Is3d() &&
                      WebGLRenderingContextBase::GetWebGLVersion(&context) ==
                          2);

}  // namespace blink

#endif
