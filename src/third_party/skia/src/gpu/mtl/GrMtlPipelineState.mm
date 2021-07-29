/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/mtl/GrMtlPipelineState.h"

#include "src/gpu/GrPipeline.h"
#include "src/gpu/GrRenderTarget.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/effects/GrTextureEffect.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLGeometryProcessor.h"
#include "src/gpu/glsl/GrGLSLXferProcessor.h"
#include "src/gpu/mtl/GrMtlBuffer.h"
#include "src/gpu/mtl/GrMtlGpu.h"
#include "src/gpu/mtl/GrMtlRenderCommandEncoder.h"
#include "src/gpu/mtl/GrMtlTexture.h"

#if !__has_feature(objc_arc)
#error This file must be compiled with Arc. Use -fobjc-arc flag
#endif

GR_NORETAIN_BEGIN

GrMtlPipelineState::SamplerBindings::SamplerBindings(GrSamplerState state,
                                                     GrTexture* texture,
                                                     GrMtlGpu* gpu)
        : fTexture(static_cast<GrMtlTexture*>(texture)->mtlTexture()) {
    fSampler = gpu->resourceProvider().findOrCreateCompatibleSampler(state);
}

GrMtlPipelineState::GrMtlPipelineState(
            GrMtlGpu* gpu,
            id<MTLRenderPipelineState> pipelineState,
            MTLPixelFormat pixelFormat,
            const GrGLSLBuiltinUniformHandles& builtinUniformHandles,
            const UniformInfoArray& uniforms,
            uint32_t uniformBufferSize,
            uint32_t numSamplers,
            std::unique_ptr<GrGLSLGeometryProcessor> geometryProcessor,
            std::unique_ptr<GrGLSLXferProcessor> xferProcessor,
            std::vector<std::unique_ptr<GrGLSLFragmentProcessor>> fpImpls)
        : fGpu(gpu)
        , fPipelineState(pipelineState)
        , fPixelFormat(pixelFormat)
        , fBuiltinUniformHandles(builtinUniformHandles)
        , fNumSamplers(numSamplers)
        , fGeometryProcessor(std::move(geometryProcessor))
        , fXferProcessor(std::move(xferProcessor))
        , fFPImpls(std::move(fpImpls))
        , fDataManager(uniforms, uniformBufferSize) {
    (void) fPixelFormat; // Suppress unused-var warning.
}

void GrMtlPipelineState::setData(const GrRenderTarget* renderTarget,
                                 const GrProgramInfo& programInfo) {
    this->setRenderTargetState(renderTarget, programInfo.origin());
    fGeometryProcessor->setData(fDataManager, *fGpu->caps()->shaderCaps(), programInfo.geomProc());

    for (int i = 0; i < programInfo.pipeline().numFragmentProcessors(); ++i) {
        auto& fp = programInfo.pipeline().getFragmentProcessor(i);
        for (auto [fp, impl] : GrGLSLFragmentProcessor::ParallelRange(fp, *fFPImpls[i])) {
            impl.setData(fDataManager, fp);
        }
    }

    programInfo.pipeline().setDstTextureUniforms(fDataManager, &fBuiltinUniformHandles);
    fXferProcessor->setData(fDataManager, programInfo.pipeline().getXferProcessor());

    fDataManager.resetDirtyBits();

#ifdef SK_DEBUG
    if (programInfo.isStencilEnabled()) {
        SkASSERT(renderTarget->getStencilAttachment());
        SkASSERT(renderTarget->numStencilBits(renderTarget->numSamples() > 1) == 8);
    }
#endif

    fStencil = programInfo.nonGLStencilSettings();
}

void GrMtlPipelineState::setTextures(const GrGeometryProcessor& geomProc,
                                     const GrPipeline& pipeline,
                                     const GrSurfaceProxy* const geomProcTextures[]) {
    fSamplerBindings.reset();
    for (int i = 0; i < geomProc.numTextureSamplers(); ++i) {
        SkASSERT(geomProcTextures[i]->asTextureProxy());
        const auto& sampler = geomProc.textureSampler(i);
        auto texture = static_cast<GrMtlTexture*>(geomProcTextures[i]->peekTexture());
        fSamplerBindings.emplace_back(sampler.samplerState(), texture, fGpu);
    }

    if (GrTextureProxy* dstTextureProxy = pipeline.dstProxyView().asTextureProxy()) {
        fSamplerBindings.emplace_back(
                GrSamplerState::Filter::kNearest, dstTextureProxy->peekTexture(), fGpu);
    }

    pipeline.visitTextureEffects([&](const GrTextureEffect& te) {
        fSamplerBindings.emplace_back(te.samplerState(), te.texture(), fGpu);
    });

    SkASSERT(fNumSamplers == fSamplerBindings.count());
}

void GrMtlPipelineState::setDrawState(GrMtlRenderCommandEncoder* renderCmdEncoder,
                                      const GrSwizzle& writeSwizzle,
                                      const GrXferProcessor& xferProcessor) {
    this->bindUniforms(renderCmdEncoder);
    this->setBlendConstants(renderCmdEncoder, writeSwizzle, xferProcessor);
    this->setDepthStencilState(renderCmdEncoder);
}

void GrMtlPipelineState::bindUniforms(GrMtlRenderCommandEncoder* renderCmdEncoder) {
    fDataManager.uploadAndBindUniformBuffers(fGpu, renderCmdEncoder);
}

void GrMtlPipelineState::bindTextures(GrMtlRenderCommandEncoder* renderCmdEncoder) {
    SkASSERT(fNumSamplers == fSamplerBindings.count());
    for (int index = 0; index < fNumSamplers; ++index) {
        renderCmdEncoder->setFragmentTexture(fSamplerBindings[index].fTexture, index);
        renderCmdEncoder->setFragmentSamplerState(fSamplerBindings[index].fSampler, index);
    }
}

void GrMtlPipelineState::setRenderTargetState(const GrRenderTarget* rt, GrSurfaceOrigin origin) {
    // Set RT adjustment and RT flip
    SkISize dimensions = rt->dimensions();
    SkASSERT(fBuiltinUniformHandles.fRTAdjustmentUni.isValid());
    if (fRenderTargetState.fRenderTargetOrigin != origin ||
        fRenderTargetState.fRenderTargetSize != dimensions) {
        fRenderTargetState.fRenderTargetSize = dimensions;
        fRenderTargetState.fRenderTargetOrigin = origin;

        // The client will mark a swap buffer as kTopLeft when making a SkSurface because
        // Metal's framebuffer space has (0, 0) at the top left. This agrees with Skia's device
        // coords. However, in NDC (-1, -1) is the bottom left. So we flip when origin is kTopLeft.
        bool flip = (origin == kTopLeft_GrSurfaceOrigin);
        std::array<float, 4> v = SkSL::Compiler::GetRTAdjustVector(dimensions, flip);
        fDataManager.set4fv(fBuiltinUniformHandles.fRTAdjustmentUni, 1, v.data());
        if (fBuiltinUniformHandles.fRTFlipUni.isValid()) {
            // Note above that framebuffer space has origin top left. So we need !flip here.
            std::array<float, 2> d = SkSL::Compiler::GetRTFlipVector(rt->height(), !flip);
            fDataManager.set2fv(fBuiltinUniformHandles.fRTFlipUni, 1, d.data());
        }
    }
}

void GrMtlPipelineState::setBlendConstants(GrMtlRenderCommandEncoder* renderCmdEncoder,
                                           const GrSwizzle& swizzle,
                                           const GrXferProcessor& xferProcessor) {
    if (!renderCmdEncoder) {
        return;
    }

    const GrXferProcessor::BlendInfo& blendInfo = xferProcessor.getBlendInfo();
    GrBlendCoeff srcCoeff = blendInfo.fSrcBlend;
    GrBlendCoeff dstCoeff = blendInfo.fDstBlend;
    if (GrBlendCoeffRefsConstant(srcCoeff) || GrBlendCoeffRefsConstant(dstCoeff)) {
        // Swizzle the blend to match what the shader will output.
        SkPMColor4f blendConst = swizzle.applyTo(blendInfo.fBlendConstant);

        renderCmdEncoder->setBlendColor(blendConst);
    }
}

void GrMtlPipelineState::setDepthStencilState(GrMtlRenderCommandEncoder* renderCmdEncoder) {
    const GrSurfaceOrigin& origin = fRenderTargetState.fRenderTargetOrigin;
    GrMtlDepthStencil* state =
            fGpu->resourceProvider().findOrCreateCompatibleDepthStencilState(fStencil, origin);
    if (!fStencil.isDisabled()) {
        if (fStencil.isTwoSided()) {
            if (@available(macOS 10.11, iOS 9.0, *)) {
                renderCmdEncoder->setStencilFrontBackReferenceValues(
                        fStencil.postOriginCCWFace(origin).fRef,
                        fStencil.postOriginCWFace(origin).fRef);
            } else {
                // Two-sided stencil not supported on older versions of iOS
                // TODO: Find a way to recover from this
                SkASSERT(false);
            }
        } else {
            renderCmdEncoder->setStencilReferenceValue(fStencil.singleSidedFace().fRef);
        }
    }
    renderCmdEncoder->setDepthStencilState(state->mtlDepthStencil());
}

void GrMtlPipelineState::SetDynamicScissorRectState(GrMtlRenderCommandEncoder* renderCmdEncoder,
                                                    const GrRenderTarget* renderTarget,
                                                    GrSurfaceOrigin rtOrigin,
                                                    SkIRect scissorRect) {
    if (!scissorRect.intersect(SkIRect::MakeWH(renderTarget->width(), renderTarget->height()))) {
        scissorRect.setEmpty();
    }

    MTLScissorRect scissor;
    scissor.x = scissorRect.fLeft;
    scissor.width = scissorRect.width();
    if (kTopLeft_GrSurfaceOrigin == rtOrigin) {
        scissor.y = scissorRect.fTop;
    } else {
        SkASSERT(kBottomLeft_GrSurfaceOrigin == rtOrigin);
        scissor.y = renderTarget->height() - scissorRect.fBottom;
    }
    scissor.height = scissorRect.height();

    SkASSERT(scissor.x >= 0);
    SkASSERT(scissor.y >= 0);

    renderCmdEncoder->setScissorRect(scissor);
}

bool GrMtlPipelineState::doesntSampleAttachment(
        const MTLRenderPassAttachmentDescriptor* attachment) const {
    for (int i = 0; i < fSamplerBindings.count(); ++i) {
        if (attachment.texture == fSamplerBindings[i].fTexture) {
            return false;
        }
    }
    return true;
}

GR_NORETAIN_END
