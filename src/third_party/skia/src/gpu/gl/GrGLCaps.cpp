/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/gpu/GrContextOptions.h"
#include "src/core/SkTSearch.h"
#include "src/core/SkTSort.h"
#include "src/gpu/GrRenderTargetProxyPriv.h"
#include "src/gpu/GrShaderCaps.h"
#include "src/gpu/GrSurfaceProxyPriv.h"
#include "src/gpu/GrTextureProxyPriv.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/gl/GrGLCaps.h"
#include "src/gpu/gl/GrGLContext.h"
#include "src/gpu/gl/GrGLRenderTarget.h"
#include "src/gpu/gl/GrGLTexture.h"
#include "src/utils/SkJSONWriter.h"

GrGLCaps::GrGLCaps(const GrContextOptions& contextOptions,
                   const GrGLContextInfo& ctxInfo,
                   const GrGLInterface* glInterface) : INHERITED(contextOptions) {
    fStandard = ctxInfo.standard();

    fStencilFormats.reset();
    fMSFBOType = kNone_MSFBOType;
    fInvalidateFBType = kNone_InvalidateFBType;
    fMapBufferType = kNone_MapBufferType;
    fTransferBufferType = kNone_TransferBufferType;
    fMaxFragmentUniformVectors = 0;
    fPackFlipYSupport = false;
    fTextureUsageSupport = false;
    fAlpha8IsRenderable = false;
    fImagingSupport = false;
    fVertexArrayObjectSupport = false;
    fDebugSupport = false;
    fES2CompatibilitySupport = false;
    fDrawIndirectSupport = false;
    fMultiDrawIndirectSupport = false;
    fBaseInstanceSupport = false;
    fIsCoreProfile = false;
    fBindFragDataLocationSupport = false;
    fRectangleTextureSupport = false;
    fRGBA8888PixelsOpsAreSlow = false;
    fPartialFBOReadIsSlow = false;
    fMipMapLevelAndLodControlSupport = false;
    fRGBAToBGRAReadbackConversionsAreSlow = false;
    fUseBufferDataNullHint = false;
    fDoManualMipmapping = false;
    fClearToBoundaryValuesIsBroken = false;
    fClearTextureSupport = false;
    fDrawArraysBaseVertexIsBroken = false;
    fDisallowTexSubImageForUnormConfigTexturesEverBoundToFBO = false;
    fUseDrawInsteadOfAllRenderTargetWrites = false;
    fRequiresCullFaceEnableDisableWhenDrawingLinesAfterNonLines = false;
    fDetachStencilFromMSAABuffersBeforeReadPixels = false;
    fDontSetBaseOrMaxLevelForExternalTextures = false;
    fNeverDisableColorWrites = false;
    fProgramBinarySupport = false;
    fProgramParameterSupport = false;
    fSamplerObjectSupport = false;
    fFBFetchRequiresEnablePerSample = false;

    fBlitFramebufferFlags = kNoSupport_BlitFramebufferFlag;
    fMaxInstancesPerDrawWithoutCrashing = 0;

    fShaderCaps.reset(new GrShaderCaps(contextOptions));

    this->init(contextOptions, ctxInfo, glInterface);
}

void GrGLCaps::init(const GrContextOptions& contextOptions,
                    const GrGLContextInfo& ctxInfo,
                    const GrGLInterface* gli) {
    GrGLStandard standard = ctxInfo.standard();
    // standard can be unused (optimzed away) if SK_ASSUME_GL_ES is set
    sk_ignore_unused_variable(standard);
    GrGLVersion version = ctxInfo.version();

    if (GR_IS_GR_GL(standard)) {
        GrGLint max;
        GR_GL_GetIntegerv(gli, GR_GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &max);
        fMaxFragmentUniformVectors = max / 4;
        if (version >= GR_GL_VER(3, 2)) {
            GrGLint profileMask;
            GR_GL_GetIntegerv(gli, GR_GL_CONTEXT_PROFILE_MASK, &profileMask);
            fIsCoreProfile = SkToBool(profileMask & GR_GL_CONTEXT_CORE_PROFILE_BIT);
        }
    } else if (GR_IS_GR_GL_ES(standard) || GR_IS_GR_WEBGL(standard)) {
        GR_GL_GetIntegerv(gli, GR_GL_MAX_FRAGMENT_UNIFORM_VECTORS,
                          &fMaxFragmentUniformVectors);
    }

    if (fDriverBugWorkarounds.max_fragment_uniform_vectors_32) {
        fMaxFragmentUniformVectors = SkMin32(fMaxFragmentUniformVectors, 32);
    }
    GR_GL_GetIntegerv(gli, GR_GL_MAX_VERTEX_ATTRIBS, &fMaxVertexAttributes);

    if (GR_IS_GR_GL(standard)) {
        fWritePixelsRowBytesSupport = true;
        fReadPixelsRowBytesSupport = true;
        fPackFlipYSupport = false;
    } else if (GR_IS_GR_GL_ES(standard)) {
        fWritePixelsRowBytesSupport =
                version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_EXT_unpack_subimage");
        fReadPixelsRowBytesSupport =
                version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_NV_pack_subimage");
        fPackFlipYSupport =
            ctxInfo.hasExtension("GL_ANGLE_pack_reverse_row_order");
    } else if (GR_IS_GR_WEBGL(standard)) {
        // WebGL 2.0 has these
        fWritePixelsRowBytesSupport = version >= GR_GL_VER(2, 0);
        fReadPixelsRowBytesSupport = version >= GR_GL_VER(2, 0);
    }
    if (fDriverBugWorkarounds.pack_parameters_workaround_with_pack_buffer) {
        // In some cases drivers handle copying the last row incorrectly
        // when using GL_PACK_ROW_LENGTH.  Chromium handles this by iterating
        // through every row and conditionally clobbering that value, but
        // Skia already has a scratch buffer workaround when pack row length
        // is not supported, so just use that.
        fReadPixelsRowBytesSupport = false;
    }

    fTextureUsageSupport = GR_IS_GR_GL_ES(standard) &&
                           ctxInfo.hasExtension("GL_ANGLE_texture_usage");

    if (GR_IS_GR_GL(standard)) {
        fTextureBarrierSupport = version >= GR_GL_VER(4,5) ||
                                 ctxInfo.hasExtension("GL_ARB_texture_barrier") ||
                                 ctxInfo.hasExtension("GL_NV_texture_barrier");
    } else if (GR_IS_GR_GL_ES(standard)) {
        fTextureBarrierSupport = ctxInfo.hasExtension("GL_NV_texture_barrier");
    } // no WebGL support

    if (GR_IS_GR_GL(standard)) {
        fSampleLocationsSupport = version >= GR_GL_VER(3,2) ||
                                  ctxInfo.hasExtension("GL_ARB_texture_multisample");
    } else if (GR_IS_GR_GL_ES(standard)) {
        fSampleLocationsSupport = version >= GR_GL_VER(3,1);
    }  // no WebGL support

    fImagingSupport = GR_IS_GR_GL(standard) &&
                      ctxInfo.hasExtension("GL_ARB_imaging");

    if (((GR_IS_GR_GL(standard) && version >= GR_GL_VER(4,3)) ||
         (GR_IS_GR_GL_ES(standard) && version >= GR_GL_VER(3,0)) ||
         ctxInfo.hasExtension("GL_ARB_invalidate_subdata"))) {
        fInvalidateFBType = kInvalidate_InvalidateFBType;
    } else if (ctxInfo.hasExtension("GL_EXT_discard_framebuffer")) {
        fInvalidateFBType = kDiscard_InvalidateFBType;
    }

    // For future reference on Desktop GL, GL_PRIMITIVE_RESTART_FIXED_INDEX appears in 4.3, and
    // GL_PRIMITIVE_RESTART (where the client must call glPrimitiveRestartIndex) appears in 3.1.
    if (GR_IS_GR_GL_ES(standard)) {
        // Primitive restart can cause a 3x slowdown on Adreno. Enable conservatively.
        // FIXME: Primitive restart would likely be a win on iOS if we had an enum value for it.
        if (kARM_GrGLVendor == ctxInfo.vendor()) {
            fUsePrimitiveRestart = version >= GR_GL_VER(3,0);
        }
    }

    if (kARM_GrGLVendor == ctxInfo.vendor() ||
        kImagination_GrGLVendor == ctxInfo.vendor() ||
        kQualcomm_GrGLVendor == ctxInfo.vendor() ) {
        fPreferFullscreenClears = true;
    }

    if (GR_IS_GR_GL(standard)) {
        fVertexArrayObjectSupport = version >= GR_GL_VER(3, 0) ||
                                    ctxInfo.hasExtension("GL_ARB_vertex_array_object") ||
                                    ctxInfo.hasExtension("GL_APPLE_vertex_array_object");
    } else if (GR_IS_GR_GL_ES(standard)) {
        fVertexArrayObjectSupport = version >= GR_GL_VER(3, 0) ||
                                    ctxInfo.hasExtension("GL_OES_vertex_array_object");
    } else if (GR_IS_GR_WEBGL(standard)) {
        fVertexArrayObjectSupport = version >= GR_GL_VER(2, 0) ||
                                    ctxInfo.hasExtension("GL_OES_vertex_array_object") ||
                                    ctxInfo.hasExtension("OES_vertex_array_object");
    }

    if (GR_IS_GR_GL(standard) && version >= GR_GL_VER(4,3)) {
        fDebugSupport = true;
    } else if (GR_IS_GR_GL_ES(standard)) {
        fDebugSupport = ctxInfo.hasExtension("GL_KHR_debug");
    } // no WebGL support

    if (GR_IS_GR_GL(standard)) {
        fES2CompatibilitySupport = ctxInfo.hasExtension("GL_ARB_ES2_compatibility");
    }
    else if (GR_IS_GR_GL_ES(standard)) {
        fES2CompatibilitySupport = true;
    } else if (GR_IS_GR_WEBGL(standard)) {
        fES2CompatibilitySupport = true;
    }

    if (GR_IS_GR_GL(standard)) {
        fMultisampleDisableSupport = true;
    } else if (GR_IS_GR_GL_ES(standard)) {
        fMultisampleDisableSupport = ctxInfo.hasExtension("GL_EXT_multisample_compatibility");
    } // no WebGL support

    if (GR_IS_GR_GL(standard)) {
        // 3.1 has draw_instanced but not instanced_arrays, for the time being we only care about
        // instanced arrays, but we could make this more granular if we wanted
        fInstanceAttribSupport =
                version >= GR_GL_VER(3, 2) ||
                (ctxInfo.hasExtension("GL_ARB_draw_instanced") &&
                 ctxInfo.hasExtension("GL_ARB_instanced_arrays"));
    } else if (GR_IS_GR_GL_ES(standard)) {
        fInstanceAttribSupport =
                version >= GR_GL_VER(3, 0) ||
                (ctxInfo.hasExtension("GL_EXT_draw_instanced") &&
                 ctxInfo.hasExtension("GL_EXT_instanced_arrays"));
    }  else if (GR_IS_GR_WEBGL(standard)) {
        // WebGL 2.0 has DrawArraysInstanced and drawElementsInstanced
        fInstanceAttribSupport = version >= GR_GL_VER(2, 0);
    }

    if (GR_IS_GR_GL(standard)) {
        if (version >= GR_GL_VER(3, 0)) {
            fBindFragDataLocationSupport = true;
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        if (version >= GR_GL_VER(3, 0) && ctxInfo.hasExtension("GL_EXT_blend_func_extended")) {
            fBindFragDataLocationSupport = true;
        }
    } // no WebGL support

    fBindUniformLocationSupport = ctxInfo.hasExtension("GL_CHROMIUM_bind_uniform_location");

    if (GR_IS_GR_GL(standard)) {
        if (version >= GR_GL_VER(3, 1) || ctxInfo.hasExtension("GL_ARB_texture_rectangle") ||
            ctxInfo.hasExtension("GL_ANGLE_texture_rectangle")) {
            fRectangleTextureSupport = true;
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        // ANGLE will advertise the extension in ES2 contexts but actually using the texture in
        // a shader requires ES3 shading language.
        fRectangleTextureSupport = ctxInfo.hasExtension("GL_ANGLE_texture_rectangle") &&
                                   ctxInfo.glslGeneration() >= k330_GrGLSLGeneration;
    } // no WebGL support

    // GrCaps defaults fClampToBorderSupport to true, so disable when unsupported
    if (GR_IS_GR_GL(standard)) {
        // Clamp to border added in 1.3
        if (version < GR_GL_VER(1, 3) && !ctxInfo.hasExtension("GL_ARB_texture_border_clamp")) {
            fClampToBorderSupport = false;
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        // GLES didn't have clamp to border until 3.2, but provides several alternative extensions
        if (version < GR_GL_VER(3, 2) && !ctxInfo.hasExtension("GL_EXT_texture_border_clamp") &&
            !ctxInfo.hasExtension("GL_NV_texture_border_clamp") &&
            !ctxInfo.hasExtension("GL_OES_texture_border_clamp")) {
            fClampToBorderSupport = false;
        }
    } else if (GR_IS_GR_WEBGL(standard)) {
        // WebGL appears to only have REPEAT, CLAMP_TO_EDGE and MIRRORED_REPEAT
        fClampToBorderSupport = false;
    }

    if (GR_IS_GR_GL(standard)) {
        if (version >= GR_GL_VER(3,3) || ctxInfo.hasExtension("GL_ARB_texture_swizzle")) {
            this->fShaderCaps->fTextureSwizzleAppliedInShader = false;
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        if (version >= GR_GL_VER(3,0)) {
            this->fShaderCaps->fTextureSwizzleAppliedInShader = false;
        }
    } // no WebGL support

    if (GR_IS_GR_GL(standard)) {
        fMipMapLevelAndLodControlSupport = true;
    } else if (GR_IS_GR_GL_ES(standard)) {
        if (version >= GR_GL_VER(3,0)) {
            fMipMapLevelAndLodControlSupport = true;
        }
    } // no WebGL support

#ifdef SK_BUILD_FOR_WIN
    // We're assuming that on Windows Chromium we're using ANGLE.
    bool isANGLE = kANGLE_GrGLDriver == ctxInfo.driver() ||
                   kChromium_GrGLDriver == ctxInfo.driver();
    // Angle has slow read/write pixel paths for 32bit RGBA (but fast for BGRA).
    fRGBA8888PixelsOpsAreSlow = isANGLE;
    // On DX9 ANGLE reading a partial FBO is slow. TODO: Check whether this is still true and
    // check DX11 ANGLE.
    fPartialFBOReadIsSlow = isANGLE;
#endif

    bool isMESA = kMesa_GrGLDriver == ctxInfo.driver();
    bool isMAC = false;
#ifdef SK_BUILD_FOR_MAC
    isMAC = true;
#endif

    // Both mesa and mac have reduced performance if reading back an RGBA framebuffer as BGRA or
    // vis-versa.
    fRGBAToBGRAReadbackConversionsAreSlow = isMESA || isMAC;

    // Chrome's command buffer will zero out a buffer if null is passed to glBufferData to
    // avoid letting an application see uninitialized memory.
    if (GR_IS_GR_GL(standard) || GR_IS_GR_GL_ES(standard)) {
        fUseBufferDataNullHint = kChromium_GrGLDriver != ctxInfo.driver();
    } else if (GR_IS_GR_WEBGL(standard)) {
        // WebGL spec explicitly disallows null values.
        fUseBufferDataNullHint = false;
    }

    if (GR_IS_GR_GL(standard)) {
        fClearTextureSupport = (version >= GR_GL_VER(4,4) ||
                                ctxInfo.hasExtension("GL_ARB_clear_texture"));
    } else if (GR_IS_GR_GL_ES(standard)) {
        fClearTextureSupport = ctxInfo.hasExtension("GL_EXT_clear_texture");
    }  // no WebGL support

#if defined(SK_BUILD_FOR_ANDROID) && __ANDROID_API__ >= 26
    fSupportsAHardwareBufferImages = true;
#endif

    // We only enable srgb support if both textures and FBOs support srgb.
    if (GR_IS_GR_GL(standard)) {
        if (version >= GR_GL_VER(3,0)) {
            fSRGBSupport = true;
        } else if (ctxInfo.hasExtension("GL_EXT_texture_sRGB")) {
            if (ctxInfo.hasExtension("GL_ARB_framebuffer_sRGB") ||
                ctxInfo.hasExtension("GL_EXT_framebuffer_sRGB")) {
                fSRGBSupport = true;
            }
        }
        // All the above srgb extensions support toggling srgb writes
        if (fSRGBSupport) {
            fSRGBWriteControl = true;
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        fSRGBSupport = version >= GR_GL_VER(3,0) || ctxInfo.hasExtension("GL_EXT_sRGB");
        // ES through 3.1 requires EXT_srgb_write_control to support toggling
        // sRGB writing for destinations.
        fSRGBWriteControl = ctxInfo.hasExtension("GL_EXT_sRGB_write_control");
    } else if (GR_IS_GR_WEBGL(standard)) {
        // sRGB extension should be on most WebGL 1.0 contexts, although
        // sometimes under 2 names.
        fSRGBSupport = version >= GR_GL_VER(2,0) || ctxInfo.hasExtension("GL_EXT_sRGB") ||
                                                    ctxInfo.hasExtension("EXT_sRGB");
    }

    // This is very conservative, if we're on a platform where N32 is BGRA, and using ES, disable
    // all sRGB support. Too much code relies on creating surfaces with N32 + sRGB colorspace,
    // and sBGRA is basically impossible to support on any version of ES (with our current code).
    // In particular, ES2 doesn't support sBGRA at all, and even in ES3, there is no valid pair
    // of formats that can be used for TexImage calls to upload BGRA data to sRGBA (which is what
    // we *have* to use as the internal format, because sBGRA doesn't exist). This primarily
    // affects Windows.
    if (kSkia8888_GrPixelConfig == kBGRA_8888_GrPixelConfig &&
        (GR_IS_GR_GL_ES(standard) || GR_IS_GR_WEBGL(standard))) {
        fSRGBSupport = false;
    }

    /**************************************************************************
    * GrShaderCaps fields
    **************************************************************************/

    // This must be called after fCoreProfile is set on the GrGLCaps
    this->initGLSL(ctxInfo, gli);
    GrShaderCaps* shaderCaps = fShaderCaps.get();

    shaderCaps->fPathRenderingSupport = this->hasPathRenderingSupport(ctxInfo, gli);

    // Enable supported shader-related caps
    if (GR_IS_GR_GL(standard)) {
        shaderCaps->fDualSourceBlendingSupport = (version >= GR_GL_VER(3, 3) ||
            ctxInfo.hasExtension("GL_ARB_blend_func_extended")) &&
            ctxInfo.glslGeneration() >= k130_GrGLSLGeneration;

        shaderCaps->fShaderDerivativeSupport = true;

        // we don't support GL_ARB_geometry_shader4, just GL 3.2+ GS
        shaderCaps->fGeometryShaderSupport = version >= GR_GL_VER(3, 2) &&
            ctxInfo.glslGeneration() >= k150_GrGLSLGeneration;
        if (shaderCaps->fGeometryShaderSupport) {
            if (ctxInfo.glslGeneration() >= k400_GrGLSLGeneration) {
                shaderCaps->fGSInvocationsSupport = true;
            } else if (ctxInfo.hasExtension("GL_ARB_gpu_shader5")) {
                shaderCaps->fGSInvocationsSupport = true;
                shaderCaps->fGSInvocationsExtensionString = "GL_ARB_gpu_shader5";
            }
        }

        shaderCaps->fIntegerSupport = version >= GR_GL_VER(3, 0) &&
            ctxInfo.glslGeneration() >= k130_GrGLSLGeneration;
    } else if (GR_IS_GR_GL_ES(standard)) {
        shaderCaps->fDualSourceBlendingSupport = ctxInfo.hasExtension("GL_EXT_blend_func_extended");

        shaderCaps->fShaderDerivativeSupport = version >= GR_GL_VER(3, 0) ||
            ctxInfo.hasExtension("GL_OES_standard_derivatives");

        // Mali and early Adreno both have support for geometry shaders, but they appear to be
        // implemented in software. In practice with ccpr, they are slower than the backup impl that
        // only uses vertex shaders.
        if (kARM_GrGLVendor != ctxInfo.vendor() &&
            kAdreno3xx_GrGLRenderer != ctxInfo.renderer() &&
            kAdreno4xx_other_GrGLRenderer != ctxInfo.renderer()) {

            if (version >= GR_GL_VER(3,2)) {
                shaderCaps->fGeometryShaderSupport = true;
            } else if (ctxInfo.hasExtension("GL_EXT_geometry_shader")) {
                shaderCaps->fGeometryShaderSupport = true;
                shaderCaps->fGeometryShaderExtensionString = "GL_EXT_geometry_shader";
            }
            shaderCaps->fGSInvocationsSupport = shaderCaps->fGeometryShaderSupport;
        }

        shaderCaps->fIntegerSupport = version >= GR_GL_VER(3, 0) &&
            ctxInfo.glslGeneration() >= k330_GrGLSLGeneration; // We use this value for GLSL ES 3.0.
    } else if (GR_IS_GR_WEBGL(standard)) {
        shaderCaps->fShaderDerivativeSupport = ctxInfo.hasExtension("GL_OES_standard_derivatives") ||
                                               ctxInfo.hasExtension("OES_standard_derivatives");
    }

    // Protect ourselves against tracking huge amounts of texture state.
    static const uint8_t kMaxSaneSamplers = 32;
    GrGLint maxSamplers;
    GR_GL_GetIntegerv(gli, GR_GL_MAX_TEXTURE_IMAGE_UNITS, &maxSamplers);
    shaderCaps->fMaxFragmentSamplers = SkTMin<GrGLint>(kMaxSaneSamplers, maxSamplers);

    // SGX and Mali GPUs have tiled architectures that have trouble with frequently changing VBOs.
    // We've measured a performance increase using non-VBO vertex data for dynamic content on these
    // GPUs. Perhaps we should read the renderer string and limit this decision to specific GPU
    // families rather than basing it on the vendor alone.
    // The Chrome command buffer blocks the use of client side buffers (but may emulate VBOs with
    // them). Client side buffers are not allowed in core profiles.
    if (GR_IS_GR_GL(standard) || GR_IS_GR_GL_ES(standard)) {
        if (ctxInfo.driver() != kChromium_GrGLDriver && !fIsCoreProfile &&
            (ctxInfo.vendor() == kARM_GrGLVendor || ctxInfo.vendor() == kImagination_GrGLVendor ||
             ctxInfo.vendor() == kQualcomm_GrGLVendor)) {
            fPreferClientSideDynamicBuffers = true;
        }
    } // No client side arrays in WebGL https://www.khronos.org/registry/webgl/specs/1.0/#6.2

    if (!contextOptions.fAvoidStencilBuffers) {
        // To reduce surface area, if we avoid stencil buffers, we also disable MSAA.
        this->initFSAASupport(contextOptions, ctxInfo, gli);
        this->initStencilSupport(ctxInfo);
    }

    // Setup blit framebuffer
    if (GR_IS_GR_GL(standard)) {
        if (version >= GR_GL_VER(3,0) ||
            ctxInfo.hasExtension("GL_ARB_framebuffer_object") ||
            ctxInfo.hasExtension("GL_EXT_framebuffer_blit")) {
            fBlitFramebufferFlags = 0;
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        if (version >= GR_GL_VER(3, 0)) {
            fBlitFramebufferFlags = kNoFormatConversionForMSAASrc_BlitFramebufferFlag |
                                    kNoMSAADst_BlitFramebufferFlag |
                                    kRectsMustMatchForMSAASrc_BlitFramebufferFlag;
        } else if (ctxInfo.hasExtension("GL_CHROMIUM_framebuffer_multisample") ||
                   ctxInfo.hasExtension("GL_ANGLE_framebuffer_blit")) {
            // The CHROMIUM extension uses the ANGLE version of glBlitFramebuffer and includes its
            // limitations.
            fBlitFramebufferFlags = kNoScalingOrMirroring_BlitFramebufferFlag |
                                    kResolveMustBeFull_BlitFrambufferFlag |
                                    kNoMSAADst_BlitFramebufferFlag |
                                    kNoFormatConversion_BlitFramebufferFlag |
                                    kRectsMustMatchForMSAASrc_BlitFramebufferFlag;
        }
    } // No WebGL 1.0 support for BlitFramebuffer

    this->initBlendEqationSupport(ctxInfo);

    if (GR_IS_GR_GL(standard)) {
        fMapBufferFlags = kCanMap_MapFlag; // we require VBO support and the desktop VBO
                                            // extension includes glMapBuffer.
        if (version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_ARB_map_buffer_range")) {
            fMapBufferFlags |= kSubset_MapFlag;
            fMapBufferType = kMapBufferRange_MapBufferType;
        } else {
            fMapBufferType = kMapBuffer_MapBufferType;
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        // Unextended GLES2 doesn't have any buffer mapping.
        fMapBufferFlags = kNone_MapBufferType;
        if (ctxInfo.hasExtension("GL_CHROMIUM_map_sub")) {
            fMapBufferFlags = kCanMap_MapFlag | kSubset_MapFlag;
            fMapBufferType = kChromium_MapBufferType;
        } else if (version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_EXT_map_buffer_range")) {
            fMapBufferFlags = kCanMap_MapFlag | kSubset_MapFlag;
            fMapBufferType = kMapBufferRange_MapBufferType;
        } else if (ctxInfo.hasExtension("GL_OES_mapbuffer")) {
            fMapBufferFlags = kCanMap_MapFlag;
            fMapBufferType = kMapBuffer_MapBufferType;
        }
    } else if (GR_IS_GR_WEBGL(standard)) {
        // explicitly removed https://www.khronos.org/registry/webgl/specs/2.0/#5.14
        fMapBufferFlags = kNone_MapBufferType;
    }

    if (GR_IS_GR_GL(standard)) {
        if (version >= GR_GL_VER(2, 1) || ctxInfo.hasExtension("GL_ARB_pixel_buffer_object") ||
            ctxInfo.hasExtension("GL_EXT_pixel_buffer_object")) {
            fTransferBufferSupport = true;
            fTransferBufferType = kPBO_TransferBufferType;
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        if (version >= GR_GL_VER(3, 0) ||
            (ctxInfo.hasExtension("GL_NV_pixel_buffer_object") &&
             // GL_EXT_unpack_subimage needed to support subtexture rectangles
             ctxInfo.hasExtension("GL_EXT_unpack_subimage"))) {
            fTransferBufferSupport = true;
            fTransferBufferType = kPBO_TransferBufferType;
// TODO: get transfer buffers working in Chrome
//        } else if (ctxInfo.hasExtension("GL_CHROMIUM_pixel_transfer_buffer_object")) {
//            fTransferBufferSupport = true;
//            fTransferBufferType = kChromium_TransferBufferType;
        }
    } // no WebGL support

    // On many GPUs, map memory is very expensive, so we effectively disable it here by setting the
    // threshold to the maximum unless the client gives us a hint that map memory is cheap.
    if (fBufferMapThreshold < 0) {
#if 0
        // We think mapping on Chromium will be cheaper once we know ahead of time how much space
        // we will use for all GrMeshDrawOps. Right now we might wind up mapping a large buffer and
        // using a small subset.
        fBufferMapThreshold = kChromium_GrGLDriver == ctxInfo.driver() ? 0 : SK_MaxS32;
#else
        fBufferMapThreshold = SK_MaxS32;
#endif
    }

    if (GR_IS_GR_GL(standard)) {
        fNPOTTextureTileSupport = true;
        fMipMapSupport = true;
    } else if (GR_IS_GR_GL_ES(standard)) {
        // Unextended ES2 supports NPOT textures with clamp_to_edge and non-mip filters only
        // ES3 has no limitations.
        fNPOTTextureTileSupport = version >= GR_GL_VER(3,0) ||
                                  ctxInfo.hasExtension("GL_OES_texture_npot");
        // ES2 supports MIP mapping for POT textures but our caps don't allow for limited MIP
        // support. The OES extension or ES 3.0 allow for MIPS on NPOT textures. So, apparently,
        // does the undocumented GL_IMG_texture_npot extension. This extension does not seem to
        // to alllow arbitrary wrap modes, however.
        fMipMapSupport = fNPOTTextureTileSupport || ctxInfo.hasExtension("GL_IMG_texture_npot");
    } else if (GR_IS_GR_WEBGL(standard)) {
        // Texture access works in the WebGL 2.0 API as in the OpenGL ES 3.0 API
        fNPOTTextureTileSupport = version >= GR_GL_VER(2,0);
        // All mipmapping and all wrapping modes are supported for non-power-of-
        // two images [in WebGL 2.0].
        fMipMapSupport = fNPOTTextureTileSupport;
    }

    GR_GL_GetIntegerv(gli, GR_GL_MAX_TEXTURE_SIZE, &fMaxTextureSize);

    if (fDriverBugWorkarounds.max_texture_size_limit_4096) {
        fMaxTextureSize = SkTMin(fMaxTextureSize, 4096);
    }

    GR_GL_GetIntegerv(gli, GR_GL_MAX_RENDERBUFFER_SIZE, &fMaxRenderTargetSize);
    // Our render targets are always created with textures as the color
    // attachment, hence this min:
    fMaxRenderTargetSize = SkTMin(fMaxTextureSize, fMaxRenderTargetSize);
    fMaxPreferredRenderTargetSize = fMaxRenderTargetSize;

    if (kARM_GrGLVendor == ctxInfo.vendor()) {
        // On Mali G71, RT's above 4k have been observed to incur a performance cost.
        fMaxPreferredRenderTargetSize = SkTMin(4096, fMaxPreferredRenderTargetSize);
    }

    fGpuTracingSupport = ctxInfo.hasExtension("GL_EXT_debug_marker");

    // Disable scratch texture reuse on Mali and Adreno devices
    fReuseScratchTextures = kARM_GrGLVendor != ctxInfo.vendor();

#if 0
    fReuseScratchBuffers = kARM_GrGLVendor != ctxInfo.vendor() &&
                           kQualcomm_GrGLVendor != ctxInfo.vendor();
#endif

    if (ctxInfo.hasExtension("GL_EXT_window_rectangles")) {
        GR_GL_GetIntegerv(gli, GR_GL_MAX_WINDOW_RECTANGLES, &fMaxWindowRectangles);
    }

#ifdef SK_BUILD_FOR_WIN
    // On ANGLE deferring flushes can lead to GPU starvation
    fPreferVRAMUseOverFlushes = !isANGLE;
#endif

    if (kARM_GrGLVendor == ctxInfo.vendor()) {
        // ARM seems to do better with larger quantities of fine triangles, as opposed to using the
        // sample mask. (At least in our current round rect op.)
        fPreferTrianglesOverSampleMask = true;
    }

    if (kChromium_GrGLDriver == ctxInfo.driver()) {
        fMustClearUploadedBufferData = true;
    }

    // In a WASM build on Firefox, we see warnings like
    // WebGL warning: texSubImage2D: This operation requires zeroing texture data. This is slow.
    // WebGL warning: texSubImage2D: Texture has not been initialized prior to a partial upload,
    //                forcing the browser to clear it. This may be slow.
    // Setting the initial clear seems to make those warnings go away and offers a substantial
    // boost in performance in Firefox. Chrome sees a more modest increase.
    if (GR_IS_GR_WEBGL(standard)) {
        fShouldInitializeTextures = true;
    }

    if (GR_IS_GR_GL(standard)) {
        // ARB allows mixed size FBO attachments, EXT does not.
        if (version >= GR_GL_VER(3, 0) ||
            ctxInfo.hasExtension("GL_ARB_framebuffer_object")) {
            fOversizedStencilSupport = true;
        } else {
            SkASSERT(ctxInfo.hasExtension("GL_EXT_framebuffer_object"));
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        // ES 3.0 supports mixed size FBO attachments, 2.0 does not.
        fOversizedStencilSupport = version >= GR_GL_VER(3, 0);
    } else if (GR_IS_GR_WEBGL(standard)) {
        // WebGL 1.0 has some constraints for FBO attachments:
        // https://www.khronos.org/registry/webgl/specs/1.0/index.html#6.6
        // These constraints "no longer apply in WebGL 2"
        fOversizedStencilSupport = version >= GR_GL_VER(2, 0);
    }

    if (GR_IS_GR_GL(standard)) {
        fDrawIndirectSupport = version >= GR_GL_VER(4,0) ||
                               ctxInfo.hasExtension("GL_ARB_draw_indirect");
        fBaseInstanceSupport = version >= GR_GL_VER(4,2);
        fMultiDrawIndirectSupport = version >= GR_GL_VER(4,3) ||
                                    (fDrawIndirectSupport &&
                                     !fBaseInstanceSupport && // The ARB extension has no base inst.
                                     ctxInfo.hasExtension("GL_ARB_multi_draw_indirect"));
        fDrawRangeElementsSupport = version >= GR_GL_VER(2,0);
    } else if (GR_IS_GR_GL_ES(standard)) {
        fDrawIndirectSupport = version >= GR_GL_VER(3,1);
        fMultiDrawIndirectSupport = fDrawIndirectSupport &&
                                    ctxInfo.hasExtension("GL_EXT_multi_draw_indirect");
        fBaseInstanceSupport = fDrawIndirectSupport &&
                               ctxInfo.hasExtension("GL_EXT_base_instance");
        fDrawRangeElementsSupport = version >= GR_GL_VER(3,0);
    } else if (GR_IS_GR_WEBGL(standard)) {
        // WebGL lacks indirect support, but drawRange was added in WebGL 2.0
        fDrawRangeElementsSupport = version >= GR_GL_VER(2,0);
    }

    // TODO: support CHROMIUM_sync_point and maybe KHR_fence_sync
    if (GR_IS_GR_GL(standard)) {
        fFenceSyncSupport = (version >= GR_GL_VER(3, 2) || ctxInfo.hasExtension("GL_ARB_sync"));
    } else if (GR_IS_GR_GL_ES(standard)) {
        fFenceSyncSupport = (version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_APPLE_sync"));
    } else if (GR_IS_GR_WEBGL(standard)) {
        // Only in WebGL 2.0
        fFenceSyncSupport = version >= GR_GL_VER(2, 0);
    }
    // The same objects (GL sync objects) are used to implement GPU/CPU fence syncs and GPU/GPU
    // semaphores.
    fSemaphoreSupport = fFenceSyncSupport;

    // Safely moving textures between contexts requires semaphores.
    fCrossContextTextureSupport = fSemaphoreSupport;

    // Half float vertex attributes requires GL3 or ES3
    // It can also work with OES_VERTEX_HALF_FLOAT, but that requires a different enum.
    if (GR_IS_GR_GL(standard)) {
        fHalfFloatVertexAttributeSupport = (version >= GR_GL_VER(3, 0));
    } else if (GR_IS_GR_GL_ES(standard)) {
        fHalfFloatVertexAttributeSupport = (version >= GR_GL_VER(3, 0));
    } else if (GR_IS_GR_WEBGL(standard)) {
        // This appears to be supported in 2.0, looking at the spec.
        fHalfFloatVertexAttributeSupport = (version >= GR_GL_VER(2, 0));
    }

    fDynamicStateArrayGeometryProcessorTextureSupport = true;

    if (GR_IS_GR_GL(standard)) {
        fProgramBinarySupport = (version >= GR_GL_VER(4, 1));
        fProgramParameterSupport = (version >= GR_GL_VER(4, 1));
    } else if (GR_IS_GR_GL_ES(standard)) {
        fProgramBinarySupport =
                (version >= GR_GL_VER(3, 0)) || ctxInfo.hasExtension("GL_OES_get_program_binary");
        fProgramParameterSupport = (version >= GR_GL_VER(3, 0));
    } // Explicitly not supported in WebGL 2.0
      // https://www.khronos.org/registry/webgl/specs/2.0/#5.4
    if (fProgramBinarySupport) {
        GrGLint count;
        GR_GL_GetIntegerv(gli, GR_GL_NUM_PROGRAM_BINARY_FORMATS, &count);
        fProgramBinarySupport = count > 0;
    }
    if (GR_IS_GR_GL(standard)) {
        fSamplerObjectSupport =
                version >= GR_GL_VER(3,3) || ctxInfo.hasExtension("GL_ARB_sampler_objects");
    } else if (GR_IS_GR_GL_ES(standard)) {
        fSamplerObjectSupport = version >= GR_GL_VER(3,0);
    } else if (GR_IS_GR_WEBGL(standard)) {
        fSamplerObjectSupport = version >= GR_GL_VER(2,0);
    }

    FormatWorkarounds formatWorkarounds;

    if (!contextOptions.fDisableDriverCorrectnessWorkarounds) {
        this->applyDriverCorrectnessWorkarounds(ctxInfo, contextOptions, shaderCaps,
                                                &formatWorkarounds);
    }

    // Requires fTextureRedSupport, fTextureSwizzleSupport, msaa support, ES compatibility have
    // already been detected.
    this->initFormatTable(ctxInfo, gli, formatWorkarounds);
    this->initConfigTable(contextOptions, ctxInfo, gli);

    this->applyOptionsOverrides(contextOptions);
    shaderCaps->applyOptionsOverrides(contextOptions);

    // For now these two are equivalent but we could have dst read in shader via some other method.
    shaderCaps->fDstReadInShaderSupport = shaderCaps->fFBFetchSupport;
}

const char* get_glsl_version_decl_string(GrGLStandard standard, GrGLSLGeneration generation,
                                         bool isCoreProfile) {
    if (GR_IS_GR_GL(standard)) {
        switch (generation) {
            case k110_GrGLSLGeneration:
                return "#version 110\n";
            case k130_GrGLSLGeneration:
                return "#version 130\n";
            case k140_GrGLSLGeneration:
                return "#version 140\n";
            case k150_GrGLSLGeneration:
                if (isCoreProfile) {
                    return "#version 150\n";
                } else {
                    return "#version 150 compatibility\n";
                }
            case k330_GrGLSLGeneration:
                if (isCoreProfile) {
                    return "#version 330\n";
                } else {
                    return "#version 330 compatibility\n";
                }
            case k400_GrGLSLGeneration:
                if (isCoreProfile) {
                    return "#version 400\n";
                } else {
                    return "#version 400 compatibility\n";
                }
            case k420_GrGLSLGeneration:
                if (isCoreProfile) {
                    return "#version 420\n";
                } else {
                    return "#version 420 compatibility\n";
                }
            default:
                break;
        }
    } else if (GR_IS_GR_GL_ES(standard) || GR_IS_GR_WEBGL(standard)) {
        switch (generation) {
            case k110_GrGLSLGeneration:
                // ES2s shader language is based on version 1.20 but is version
                // 1.00 of the ES language.
                return "#version 100\n";
            case k330_GrGLSLGeneration:
                return "#version 300 es\n";
            case k310es_GrGLSLGeneration:
                return "#version 310 es\n";
            case k320es_GrGLSLGeneration:
                return "#version 320 es\n";
            default:
                break;
        }
    }
    return "<no version>";
}

bool is_float_fp32(const GrGLContextInfo& ctxInfo, const GrGLInterface* gli, GrGLenum precision) {
    if (GR_IS_GR_GL(ctxInfo.standard()) &&
        ctxInfo.version() < GR_GL_VER(4,1) &&
        !ctxInfo.hasExtension("GL_ARB_ES2_compatibility")) {
        // We're on a desktop GL that doesn't have precision info. Assume they're all 32bit float.
        return true;
    }
    // glGetShaderPrecisionFormat doesn't accept GL_GEOMETRY_SHADER as a shader type. Hopefully the
    // geometry shaders don't have lower precision than vertex and fragment.
    for (GrGLenum shader : {GR_GL_FRAGMENT_SHADER, GR_GL_VERTEX_SHADER}) {
        GrGLint range[2];
        GrGLint bits;
        GR_GL_GetShaderPrecisionFormat(gli, shader, precision, range, &bits);
        if (range[0] < 127 || range[1] < 127 || bits < 23) {
            return false;
        }
    }
    return true;
}

void GrGLCaps::initGLSL(const GrGLContextInfo& ctxInfo, const GrGLInterface* gli) {
    GrGLStandard standard = ctxInfo.standard();
    GrGLVersion version = ctxInfo.version();

    /**************************************************************************
    * Caps specific to GrShaderCaps
    **************************************************************************/

    GrShaderCaps* shaderCaps = fShaderCaps.get();
    shaderCaps->fGLSLGeneration = ctxInfo.glslGeneration();
    if (GR_IS_GR_GL_ES(standard)) {
        // fFBFetchRequiresEnablePerSample is not a shader cap but is initialized below to keep it
        // with related FB fetch logic.
        if (ctxInfo.hasExtension("GL_EXT_shader_framebuffer_fetch")) {
            shaderCaps->fFBFetchNeedsCustomOutput = (version >= GR_GL_VER(3, 0));
            shaderCaps->fFBFetchSupport = true;
            shaderCaps->fFBFetchColorName = "gl_LastFragData[0]";
            shaderCaps->fFBFetchExtensionString = "GL_EXT_shader_framebuffer_fetch";
            fFBFetchRequiresEnablePerSample = false;
        } else if (ctxInfo.hasExtension("GL_NV_shader_framebuffer_fetch")) {
            // Actually, we haven't seen an ES3.0 device with this extension yet, so we don't know.
            shaderCaps->fFBFetchNeedsCustomOutput = false;
            shaderCaps->fFBFetchSupport = true;
            shaderCaps->fFBFetchColorName = "gl_LastFragData[0]";
            shaderCaps->fFBFetchExtensionString = "GL_NV_shader_framebuffer_fetch";
            fFBFetchRequiresEnablePerSample = false;
        } else if (ctxInfo.hasExtension("GL_ARM_shader_framebuffer_fetch")) {
            // The arm extension also requires an additional flag which we will set onResetContext.
            shaderCaps->fFBFetchNeedsCustomOutput = false;
            shaderCaps->fFBFetchSupport = true;
            shaderCaps->fFBFetchColorName = "gl_LastFragColorARM";
            shaderCaps->fFBFetchExtensionString = "GL_ARM_shader_framebuffer_fetch";
            fFBFetchRequiresEnablePerSample = true;
        }
        shaderCaps->fUsesPrecisionModifiers = true;
    } else if (GR_IS_GR_WEBGL(standard)) {
        shaderCaps->fUsesPrecisionModifiers = true;
    }

    if (GR_IS_GR_GL(standard)) {
        shaderCaps->fFlatInterpolationSupport = ctxInfo.glslGeneration() >= k130_GrGLSLGeneration;
    } else if (GR_IS_GR_GL_ES(standard) || GR_IS_GR_WEBGL(standard)) {
        shaderCaps->fFlatInterpolationSupport =
            ctxInfo.glslGeneration() >= k330_GrGLSLGeneration; // This is the value for GLSL ES 3.0.
    } // not sure for WebGL

    // Flat interpolation appears to be slow on Qualcomm GPUs (tested Adreno 405 and 530). ANGLE
    // Avoid on ANGLE too, it inserts a geometry shader into the pipeline to implement flat interp.
    shaderCaps->fPreferFlatInterpolation = shaderCaps->fFlatInterpolationSupport &&
                                           kQualcomm_GrGLVendor != ctxInfo.vendor() &&
                                           kANGLE_GrGLDriver != ctxInfo.driver();
    if (GR_IS_GR_GL(standard)) {
        shaderCaps->fNoPerspectiveInterpolationSupport =
            ctxInfo.glslGeneration() >= k130_GrGLSLGeneration;
    } else if (GR_IS_GR_GL_ES(standard)) {
        if (ctxInfo.hasExtension("GL_NV_shader_noperspective_interpolation") &&
            ctxInfo.glslGeneration() >= k330_GrGLSLGeneration /* GLSL ES 3.0 */) {
            shaderCaps->fNoPerspectiveInterpolationSupport = true;
            shaderCaps->fNoPerspectiveInterpolationExtensionString =
                "GL_NV_shader_noperspective_interpolation";
        }
    }  // Not sure for WebGL

    if (GR_IS_GR_GL(standard)) {
        shaderCaps->fSampleVariablesSupport = ctxInfo.glslGeneration() >= k400_GrGLSLGeneration;
    } else if (GR_IS_GR_GL_ES(standard)) {
        if (ctxInfo.glslGeneration() >= k320es_GrGLSLGeneration) {
            shaderCaps->fSampleVariablesSupport = true;
        } else if (ctxInfo.hasExtension("GL_OES_sample_variables")) {
            shaderCaps->fSampleVariablesSupport = true;
            shaderCaps->fSampleVariablesExtensionString = "GL_OES_sample_variables";
        }
    }
    shaderCaps->fSampleVariablesStencilSupport = shaderCaps->fSampleVariablesSupport;

    if (kQualcomm_GrGLVendor == ctxInfo.vendor() || kATI_GrGLVendor == ctxInfo.vendor()) {
        // FIXME: The sample mask round rect op draws nothing on several Adreno and Radeon bots.
        // Other ops that use sample mask while rendering to stencil seem to work fine. Temporarily
        // disable sample mask on color buffers while we investigate.
        // http://skbug.com/8921
        shaderCaps->fSampleVariablesSupport = false;
    }

    shaderCaps->fVersionDeclString = get_glsl_version_decl_string(standard,
                                                                  shaderCaps->fGLSLGeneration,
                                                                  fIsCoreProfile);

    if (GR_IS_GR_GL_ES(standard) || GR_IS_GR_WEBGL(standard)) {
        if (k110_GrGLSLGeneration == shaderCaps->fGLSLGeneration) {
            shaderCaps->fShaderDerivativeExtensionString = "GL_OES_standard_derivatives";
        }
    } // WebGL might have to check for OES_standard_derivatives

    // Frag Coords Convention support is not part of ES
    if (GR_IS_GR_GL(standard) &&
        (ctxInfo.glslGeneration() >= k150_GrGLSLGeneration ||
         ctxInfo.hasExtension("GL_ARB_fragment_coord_conventions"))) {
        shaderCaps->fFragCoordConventionsExtensionString = "GL_ARB_fragment_coord_conventions";
    }

    if (GR_IS_GR_GL_ES(standard)) {
        shaderCaps->fSecondaryOutputExtensionString = "GL_EXT_blend_func_extended";
    }

    if (ctxInfo.hasExtension("GL_OES_EGL_image_external")) {
        if (ctxInfo.glslGeneration() == k110_GrGLSLGeneration) {
            shaderCaps->fExternalTextureSupport = true;
            shaderCaps->fExternalTextureExtensionString = "GL_OES_EGL_image_external";
        } else if (ctxInfo.hasExtension("GL_OES_EGL_image_external_essl3") ||
                   ctxInfo.hasExtension("OES_EGL_image_external_essl3")) {
            // At least one driver has been found that has this extension without the "GL_" prefix.
            shaderCaps->fExternalTextureSupport = true;
            shaderCaps->fExternalTextureExtensionString = "GL_OES_EGL_image_external_essl3";
        }
    }

    if (GR_IS_GR_GL(standard)) {
        shaderCaps->fVertexIDSupport = true;
    } else if (GR_IS_GR_GL_ES(standard) || GR_IS_GR_WEBGL(standard)) {
        // Desktop GLSL 3.30 == ES GLSL 3.00.
        shaderCaps->fVertexIDSupport = ctxInfo.glslGeneration() >= k330_GrGLSLGeneration;
    }

    if (GR_IS_GR_GL(standard)) {
        shaderCaps->fFPManipulationSupport = ctxInfo.glslGeneration() >= k400_GrGLSLGeneration;
    } else if (GR_IS_GR_GL_ES(standard) || GR_IS_GR_WEBGL(standard)) {
        shaderCaps->fFPManipulationSupport = ctxInfo.glslGeneration() >= k310es_GrGLSLGeneration;
    }

    shaderCaps->fFloatIs32Bits = is_float_fp32(ctxInfo, gli, GR_GL_HIGH_FLOAT);
    shaderCaps->fHalfIs32Bits = is_float_fp32(ctxInfo, gli, GR_GL_MEDIUM_FLOAT);
    shaderCaps->fHasLowFragmentPrecision = kMali4xx_GrGLRenderer == ctxInfo.renderer();

    if (GR_IS_GR_GL(standard)) {
        shaderCaps->fBuiltinFMASupport = ctxInfo.glslGeneration() >= k400_GrGLSLGeneration;
    } else if (GR_IS_GR_GL_ES(standard)) {
        shaderCaps->fBuiltinFMASupport = ctxInfo.glslGeneration() >= k320es_GrGLSLGeneration;
    }
}

bool GrGLCaps::hasPathRenderingSupport(const GrGLContextInfo& ctxInfo, const GrGLInterface* gli) {
    bool hasChromiumPathRendering = ctxInfo.hasExtension("GL_CHROMIUM_path_rendering");

    if (!(ctxInfo.hasExtension("GL_NV_path_rendering") || hasChromiumPathRendering)) {
        return false;
    }

    if (GR_IS_GR_GL(ctxInfo.standard())) {
        if (ctxInfo.version() < GR_GL_VER(4, 3) &&
            !ctxInfo.hasExtension("GL_ARB_program_interface_query")) {
            return false;
        }
    } else if (GR_IS_GR_GL_ES(ctxInfo.standard())) {
        if (!hasChromiumPathRendering &&
            ctxInfo.version() < GR_GL_VER(3, 1)) {
            return false;
        }
    } else if (GR_IS_GR_WEBGL(ctxInfo.standard())) {
        // No WebGL support
        return false;
    }
    // We only support v1.3+ of GL_NV_path_rendering which allows us to
    // set individual fragment inputs with ProgramPathFragmentInputGen. The API
    // additions are detected by checking the existence of the function.
    // We also use *Then* functions that not all drivers might have. Check
    // them for consistency.
    if (!gli->fFunctions.fStencilThenCoverFillPath ||
        !gli->fFunctions.fStencilThenCoverStrokePath ||
        !gli->fFunctions.fStencilThenCoverFillPathInstanced ||
        !gli->fFunctions.fStencilThenCoverStrokePathInstanced ||
        !gli->fFunctions.fProgramPathFragmentInputGen) {
        return false;
    }
    return true;
}

void GrGLCaps::initFSAASupport(const GrContextOptions& contextOptions, const GrGLContextInfo& ctxInfo,
                               const GrGLInterface* gli) {
    // We need dual source blending and the ability to disable multisample in order to support mixed
    // samples in every corner case.
    if (fMultisampleDisableSupport && this->shaderCaps()->dualSourceBlendingSupport()) {
        fMixedSamplesSupport = ctxInfo.hasExtension("GL_NV_framebuffer_mixed_samples") ||
                               ctxInfo.hasExtension("GL_CHROMIUM_framebuffer_mixed_samples");
    }

    if (GR_IS_GR_GL(ctxInfo.standard())) {
        if (ctxInfo.version() >= GR_GL_VER(3,0) ||
            ctxInfo.hasExtension("GL_ARB_framebuffer_object")) {
            fMSFBOType = kStandard_MSFBOType;
            if (!fIsCoreProfile && ctxInfo.renderer() != kOSMesa_GrGLRenderer) {
                // Core profile removes ALPHA8 support.
                // OpenGL 3.0+ (and GL_ARB_framebuffer_object) supports ALPHA8 as renderable.
                // However, osmesa fails if it is used even when GL_ARB_framebuffer_object is
                // present.
                fAlpha8IsRenderable = true;
            }
        } else if (ctxInfo.hasExtension("GL_EXT_framebuffer_multisample") &&
                   ctxInfo.hasExtension("GL_EXT_framebuffer_blit")) {
            fMSFBOType = kStandard_MSFBOType;
        }
    } else if (GR_IS_GR_GL_ES(ctxInfo.standard())) {
        if (ctxInfo.version() >= GR_GL_VER(3,0) &&
            ctxInfo.renderer() != kGalliumLLVM_GrGLRenderer) {
            // The gallium llvmpipe renderer for es3.0 does not have textureRed support even though
            // it is part of the spec. Thus alpha8 will not be renderable for those devices.
            fAlpha8IsRenderable = true;
        }
        // We prefer multisampled-render-to-texture extensions over ES3 MSAA because we've observed
        // ES3 driver bugs on at least one device with a tiled GPU (N10).
        if (ctxInfo.hasExtension("GL_EXT_multisampled_render_to_texture")) {
            fMSFBOType = kES_EXT_MsToTexture_MSFBOType;
        } else if (ctxInfo.hasExtension("GL_IMG_multisampled_render_to_texture")) {
            fMSFBOType = kES_IMG_MsToTexture_MSFBOType;
        } else if (ctxInfo.version() >= GR_GL_VER(3,0)) {
            fMSFBOType = kStandard_MSFBOType;
        } else if (ctxInfo.hasExtension("GL_CHROMIUM_framebuffer_multisample")) {
            fMSFBOType = kStandard_MSFBOType;
        } else if (ctxInfo.hasExtension("GL_ANGLE_framebuffer_multisample")) {
            fMSFBOType = kStandard_MSFBOType;
        } else if (ctxInfo.hasExtension("GL_APPLE_framebuffer_multisample")) {
            fMSFBOType = kES_Apple_MSFBOType;
        }
    } else if (GR_IS_GR_WEBGL(ctxInfo.standard())) {
        // No support in WebGL
        fMSFBOType = kNone_MSFBOType;
    }

    // We disable MSAA for older (pre-Gen9) Intel GPUs for performance reasons.
    // ApolloLake is the first Gen9 chipset.
    if (kIntel_GrGLVendor == ctxInfo.vendor() &&
        (ctxInfo.renderer() < kIntelApolloLake_GrGLRenderer ||
         ctxInfo.renderer() == kOther_GrGLRenderer)) {
        fMSFBOType = kNone_MSFBOType;
    }
}

void GrGLCaps::initBlendEqationSupport(const GrGLContextInfo& ctxInfo) {
    GrShaderCaps* shaderCaps = static_cast<GrShaderCaps*>(fShaderCaps.get());

    bool layoutQualifierSupport = false;
    if ((GR_IS_GR_GL(fStandard) && shaderCaps->generation() >= k140_GrGLSLGeneration)  ||
        (GR_IS_GR_GL_ES(fStandard) && shaderCaps->generation() >= k330_GrGLSLGeneration)) {
        layoutQualifierSupport = true;
    } else if (GR_IS_GR_WEBGL(fStandard)) {
        return;
    }

    if (ctxInfo.hasExtension("GL_NV_blend_equation_advanced_coherent")) {
        fBlendEquationSupport = kAdvancedCoherent_BlendEquationSupport;
        shaderCaps->fAdvBlendEqInteraction = GrShaderCaps::kAutomatic_AdvBlendEqInteraction;
    } else if (ctxInfo.hasExtension("GL_KHR_blend_equation_advanced_coherent") &&
               layoutQualifierSupport) {
        fBlendEquationSupport = kAdvancedCoherent_BlendEquationSupport;
        shaderCaps->fAdvBlendEqInteraction = GrShaderCaps::kGeneralEnable_AdvBlendEqInteraction;
    } else if (ctxInfo.hasExtension("GL_NV_blend_equation_advanced")) {
        fBlendEquationSupport = kAdvanced_BlendEquationSupport;
        shaderCaps->fAdvBlendEqInteraction = GrShaderCaps::kAutomatic_AdvBlendEqInteraction;
    } else if (ctxInfo.hasExtension("GL_KHR_blend_equation_advanced") && layoutQualifierSupport) {
        fBlendEquationSupport = kAdvanced_BlendEquationSupport;
        shaderCaps->fAdvBlendEqInteraction = GrShaderCaps::kGeneralEnable_AdvBlendEqInteraction;
        // TODO: Use kSpecificEnables_AdvBlendEqInteraction if "blend_support_all_equations" is
        // slow on a particular platform.
    }
}

namespace {
const GrGLuint kUnknownBitCount = GrGLStencilAttachment::kUnknownBitCount;
}

void GrGLCaps::initStencilSupport(const GrGLContextInfo& ctxInfo) {

    // Build up list of legal stencil formats (though perhaps not supported on
    // the particular gpu/driver) from most preferred to least.

    // these consts are in order of most preferred to least preferred
    // we don't bother with GL_STENCIL_INDEX1 or GL_DEPTH32F_STENCIL8

    static const StencilFormat
                  // internal Format      stencil bits      total bits        packed?
        gS8    = {GR_GL_STENCIL_INDEX8,   8,                8,                false},
        gS16   = {GR_GL_STENCIL_INDEX16,  16,               16,               false},
        gD24S8 = {GR_GL_DEPTH24_STENCIL8, 8,                32,               true },
        gS4    = {GR_GL_STENCIL_INDEX4,   4,                4,                false},
    //  gS     = {GR_GL_STENCIL_INDEX,    kUnknownBitCount, kUnknownBitCount, false},
        gDS    = {GR_GL_DEPTH_STENCIL,    kUnknownBitCount, kUnknownBitCount, true };

    if (GR_IS_GR_GL(ctxInfo.standard())) {
        bool supportsPackedDS =
            ctxInfo.version() >= GR_GL_VER(3,0) ||
            ctxInfo.hasExtension("GL_EXT_packed_depth_stencil") ||
            ctxInfo.hasExtension("GL_ARB_framebuffer_object");

        // S1 thru S16 formats are in GL 3.0+, EXT_FBO, and ARB_FBO since we
        // require FBO support we can expect these are legal formats and don't
        // check. These also all support the unsized GL_STENCIL_INDEX.
        fStencilFormats.push_back() = gS8;
        fStencilFormats.push_back() = gS16;
        if (supportsPackedDS) {
            fStencilFormats.push_back() = gD24S8;
        }
        fStencilFormats.push_back() = gS4;
        if (supportsPackedDS) {
            fStencilFormats.push_back() = gDS;
        }
    } else if (GR_IS_GR_GL_ES(ctxInfo.standard())) {
        // ES2 has STENCIL_INDEX8 without extensions but requires extensions
        // for other formats.
        // ES doesn't support using the unsized format.

        fStencilFormats.push_back() = gS8;
        //fStencilFormats.push_back() = gS16;
        if (ctxInfo.version() >= GR_GL_VER(3,0) ||
            ctxInfo.hasExtension("GL_OES_packed_depth_stencil")) {
            fStencilFormats.push_back() = gD24S8;
        }
        if (ctxInfo.hasExtension("GL_OES_stencil4")) {
            fStencilFormats.push_back() = gS4;
        }
    } else if (GR_IS_GR_WEBGL(ctxInfo.standard())) {
        fStencilFormats.push_back() = gS8;
        if (ctxInfo.version() >= GR_GL_VER(2,0)) {
            fStencilFormats.push_back() = gD24S8;
        }
    }
}

#ifdef SK_ENABLE_DUMP_GPU
void GrGLCaps::onDumpJSON(SkJSONWriter* writer) const {

    // We are called by the base class, which has already called beginObject(). We choose to nest
    // all of our caps information in a named sub-object.
    writer->beginObject("GL caps");

    writer->beginArray("Stencil Formats");

    for (int i = 0; i < fStencilFormats.count(); ++i) {
        writer->beginObject(nullptr, false);
        writer->appendS32("stencil bits", fStencilFormats[i].fStencilBits);
        writer->appendS32("total bits", fStencilFormats[i].fTotalBits);
        writer->endObject();
    }

    writer->endArray();

    static const char* kMSFBOExtStr[] = {
        "None",
        "Standard",
        "Apple",
        "IMG MS To Texture",
        "EXT MS To Texture",
    };
    GR_STATIC_ASSERT(0 == kNone_MSFBOType);
    GR_STATIC_ASSERT(1 == kStandard_MSFBOType);
    GR_STATIC_ASSERT(2 == kES_Apple_MSFBOType);
    GR_STATIC_ASSERT(3 == kES_IMG_MsToTexture_MSFBOType);
    GR_STATIC_ASSERT(4 == kES_EXT_MsToTexture_MSFBOType);
    GR_STATIC_ASSERT(SK_ARRAY_COUNT(kMSFBOExtStr) == kLast_MSFBOType + 1);

    static const char* kInvalidateFBTypeStr[] = {
        "None",
        "Discard",
        "Invalidate",
    };
    GR_STATIC_ASSERT(0 == kNone_InvalidateFBType);
    GR_STATIC_ASSERT(1 == kDiscard_InvalidateFBType);
    GR_STATIC_ASSERT(2 == kInvalidate_InvalidateFBType);
    GR_STATIC_ASSERT(SK_ARRAY_COUNT(kInvalidateFBTypeStr) == kLast_InvalidateFBType + 1);

    static const char* kMapBufferTypeStr[] = {
        "None",
        "MapBuffer",
        "MapBufferRange",
        "Chromium",
    };
    GR_STATIC_ASSERT(0 == kNone_MapBufferType);
    GR_STATIC_ASSERT(1 == kMapBuffer_MapBufferType);
    GR_STATIC_ASSERT(2 == kMapBufferRange_MapBufferType);
    GR_STATIC_ASSERT(3 == kChromium_MapBufferType);
    GR_STATIC_ASSERT(SK_ARRAY_COUNT(kMapBufferTypeStr) == kLast_MapBufferType + 1);

    writer->appendBool("Core Profile", fIsCoreProfile);
    writer->appendString("MSAA Type", kMSFBOExtStr[fMSFBOType]);
    writer->appendString("Invalidate FB Type", kInvalidateFBTypeStr[fInvalidateFBType]);
    writer->appendString("Map Buffer Type", kMapBufferTypeStr[fMapBufferType]);
    writer->appendS32("Max FS Uniform Vectors", fMaxFragmentUniformVectors);
    writer->appendBool("Pack Flip Y support", fPackFlipYSupport);

    writer->appendBool("Texture Usage support", fTextureUsageSupport);
    writer->appendBool("Alpha8 is renderable", fAlpha8IsRenderable);
    writer->appendBool("GL_ARB_imaging support", fImagingSupport);
    writer->appendBool("Vertex array object support", fVertexArrayObjectSupport);
    writer->appendBool("Debug support", fDebugSupport);
    writer->appendBool("Draw indirect support", fDrawIndirectSupport);
    writer->appendBool("Multi draw indirect support", fMultiDrawIndirectSupport);
    writer->appendBool("Base instance support", fBaseInstanceSupport);
    writer->appendBool("RGBA 8888 pixel ops are slow", fRGBA8888PixelsOpsAreSlow);
    writer->appendBool("Partial FBO read is slow", fPartialFBOReadIsSlow);
    writer->appendBool("Bind uniform location support", fBindUniformLocationSupport);
    writer->appendBool("Rectangle texture support", fRectangleTextureSupport);
    writer->appendBool("BGRA to RGBA readback conversions are slow",
                       fRGBAToBGRAReadbackConversionsAreSlow);
    writer->appendBool("Use buffer data null hint", fUseBufferDataNullHint);

    writer->appendBool("Intermediate texture for partial updates of unorm textures ever bound to FBOs",
                       fDisallowTexSubImageForUnormConfigTexturesEverBoundToFBO);
    writer->appendBool("Intermediate texture for all updates of textures bound to FBOs",
                       fUseDrawInsteadOfAllRenderTargetWrites);
    writer->appendBool("Max instances per draw without crashing (or zero)",
                       fMaxInstancesPerDrawWithoutCrashing);

    writer->beginArray("configs");

    for (int i = 0; i < kGrPixelConfigCnt; ++i) {
        writer->beginObject(nullptr, false);
//        writer->appendHexU32("flags", fConfigTable[i].fFlags);
//        writer->appendHexU32("b_internal", fConfigTable[i].fFormats.fBaseInternalFormat);
//        writer->appendHexU32("s_internal", fConfigTable[i].fFormats.fSizedInternalFormat);
        writer->appendHexU32("e_format_read_pixels",
                             fConfigTable[i].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage]);
        writer->appendHexU32(
                "e_format_teximage",
                fConfigTable[i].fFormats.fExternalFormat[kTexImage_ExternalFormatUsage]);
        writer->appendHexU32("e_type", fConfigTable[i].fFormats.fExternalType);
//        writer->appendHexU32("i_for_teximage", fConfigTable[i].fFormats.fInternalFormatTexImage);
//        writer->appendHexU32("i_for_renderbuffer",
//                             fConfigTable[i].fFormats.fInternalFormatRenderbuffer);
        writer->endObject();
    }
// TODO: Add dump of FormatInfos

    writer->endArray();
    writer->endObject();
}
#else
void GrGLCaps::onDumpJSON(SkJSONWriter* writer) const { }
#endif

bool GrGLCaps::bgraIsInternalFormat() const {
    return this->getFormatFromColorType(GrColorType::kBGRA_8888) == GrGLFormat::kBGRA8;
}

bool GrGLCaps::getTexImageFormats(GrPixelConfig surfaceConfig, GrPixelConfig externalConfig,
                                  GrGLenum* internalFormat, GrGLenum* externalFormat,
                                  GrGLenum* externalType) const {
    if (!this->getExternalFormat(surfaceConfig, externalConfig, kTexImage_ExternalFormatUsage,
                                 externalFormat, externalType)) {
        return false;
    }
    *internalFormat = this->getTexImageInternalFormat(this->pixelConfigToFormat(surfaceConfig));
    return true;
}

bool GrGLCaps::getCompressedTexImageFormats(GrPixelConfig surfaceConfig,
                                            GrGLenum* internalFormat) const {
    if (!GrPixelConfigIsCompressed(surfaceConfig)) {
        return false;
    }
    *internalFormat = this->getTexImageInternalFormat(this->pixelConfigToFormat(surfaceConfig));
    return true;
}

bool GrGLCaps::getReadPixelsFormat(GrPixelConfig surfaceConfig, GrPixelConfig externalConfig,
                                   GrGLenum* externalFormat, GrGLenum* externalType) const {
    if (!this->getExternalFormat(surfaceConfig, externalConfig, kReadPixels_ExternalFormatUsage,
                                 externalFormat, externalType)) {
        return false;
    }
    return true;
}

bool GrGLCaps::getExternalFormat(GrPixelConfig surfaceConfig, GrPixelConfig memoryConfig,
                                 ExternalFormatUsage usage, GrGLenum* externalFormat,
                                 GrGLenum* externalType) const {
    SkASSERT(externalFormat && externalType);
    if (GrPixelConfigIsCompressed(memoryConfig)) {
        return false;
    }

    bool surfaceIsAlphaOnly = GrPixelConfigIsAlphaOnly(surfaceConfig);
    bool memoryIsAlphaOnly = GrPixelConfigIsAlphaOnly(memoryConfig);

    *externalFormat = fConfigTable[memoryConfig].fFormats.fExternalFormat[usage];
    *externalType = fConfigTable[memoryConfig].fFormats.fExternalType;

    // When GL_RED is supported as a texture format, our alpha-only textures are stored using
    // GL_RED and we swizzle in order to map all components to 'r'. However, in this case the
    // surface is not alpha-only and we want alpha to really mean the alpha component of the
    // texture, not the red component.
    if (memoryIsAlphaOnly && !surfaceIsAlphaOnly) {
        if (GR_GL_RED == *externalFormat) {
            *externalFormat = GR_GL_ALPHA;
        }
    }

    return true;
}

void GrGLCaps::setStencilFormatIndexForFormat(GrGLFormat format, int index) {
    SkASSERT(!this->hasStencilFormatBeenDeterminedForFormat(format));
    this->getFormatInfo(format).fStencilFormatIndex =
            index < 0 ? FormatInfo::kUnsupported_StencilFormatIndex : index;
}

void GrGLCaps::setColorTypeFormat(GrColorType colorType, GrGLFormat format) {
    int idx = static_cast<int>(colorType);
    SkASSERT(fColorTypeToFormatTable[idx] == GrGLFormat::kUnknown);
    fColorTypeToFormatTable[idx] = format;
}

void GrGLCaps::initFormatTable(const GrGLContextInfo& ctxInfo, const GrGLInterface* gli,
                               const FormatWorkarounds& formatWorkarounds) {
    GrGLStandard standard = ctxInfo.standard();
    // standard can be unused (optimized away) if SK_ASSUME_GL_ES is set
    sk_ignore_unused_variable(standard);
    GrGLVersion version = ctxInfo.version();

    uint32_t nonMSAARenderFlags = FormatInfo::kFBOColorAttachment_Flag;
    uint32_t msaaRenderFlags = nonMSAARenderFlags;
    if (kNone_MSFBOType != fMSFBOType) {
        msaaRenderFlags |= FormatInfo::kFBOColorAttachmentWithMSAA_Flag;
    }

    bool texStorageSupported = false;
    if (GR_IS_GR_GL(standard)) {
        // The EXT version can apply to either GL or GLES.
        texStorageSupported = version >= GR_GL_VER(4,2) ||
                              ctxInfo.hasExtension("GL_ARB_texture_storage") ||
                              ctxInfo.hasExtension("GL_EXT_texture_storage");
    } else if (GR_IS_GR_GL_ES(standard)) {
        texStorageSupported = version >= GR_GL_VER(3,0) ||
                              ctxInfo.hasExtension("GL_EXT_texture_storage");
    } else if (GR_IS_GR_WEBGL(standard)) {
        texStorageSupported = version >= GR_GL_VER(2,0);
    }
    if (fDriverBugWorkarounds.disable_texture_storage) {
        texStorageSupported = false;
    }
#ifdef SK_BUILD_FOR_ANDROID
    // crbug.com/945506. Telemetry reported a memory usage regression for Android Go Chrome/WebView
    // when using glTexStorage2D. This appears to affect OOP-R (so not just over command buffer).
    texStorageSupported = false;
#endif

    // ES 2.0 requires that the internal/external formats match so we can't use sized internal
    // formats for glTexImage until ES 3.0. TODO: Support sized internal formats in WebGL2.
    bool texImageSupportsSizedInternalFormat =
            (GR_IS_GR_GL(standard) || (GR_IS_GR_GL_ES(standard) && version >= GR_GL_VER(3,0)));

    // All ES versions (thus far) require sized internal formats for render buffers.
    // TODO: Always use sized internal format?
    bool renderbufferStorageSupportsSizedInternalFormat =
            GR_IS_GR_GL_ES(standard) || GR_IS_GR_WEBGL(standard);

    // This is set when we know we have both texture and render support for:
    // R 8/16/16F
    // RG 8/16/16F
    // The floating point formats above also depend on additional general floating put support on
    // the device which we query later on.
    bool textureRedSupport = false;

    if (!formatWorkarounds.fDisableTextureRedForMesa) {
        if (GR_IS_GR_GL(standard)) {
            textureRedSupport =
                    version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_ARB_texture_rg");
        } else if (GR_IS_GR_GL_ES(standard)) {
            textureRedSupport =
                    version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_EXT_texture_rg");
        }
    }   // No WebGL support

    // Check for [half] floating point texture support
    // NOTE: We disallow floating point textures on ES devices if linear filtering modes are not
    // supported. This is for simplicity, but a more granular approach is possible. Coincidentally,
    // [half] floating point textures became part of the standard in ES3.1 / OGL 3.0.
    bool hasFP16Textures = false;
    enum class HalfFPRenderTargetSupport { kNone, kRGBAOnly, kAll };
    HalfFPRenderTargetSupport halfFPRenderTargetSupport = HalfFPRenderTargetSupport::kNone;
    // for now we don't support floating point MSAA on ES
    uint32_t fpRenderFlags = (GR_IS_GR_GL(standard)) ? msaaRenderFlags : nonMSAARenderFlags;

    if (GR_IS_GR_GL(standard)) {
        // TODO: it seems like GL_ARB_texture_float GL_ARB_color_buffer_float should be taken
        // into account here
        if (version >= GR_GL_VER(3, 0)) {
            hasFP16Textures = true;
            halfFPRenderTargetSupport = HalfFPRenderTargetSupport::kAll;
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        if (version >= GR_GL_VER(3, 0)) {
            hasFP16Textures = true;
        } else if (ctxInfo.hasExtension("GL_OES_texture_float_linear") &&
                   ctxInfo.hasExtension("GL_OES_texture_float")) {
            hasFP16Textures = true;
        } else if (ctxInfo.hasExtension("GL_OES_texture_half_float_linear") &&
                   ctxInfo.hasExtension("GL_OES_texture_half_float")) {
            hasFP16Textures = true;
        }

        if (version >= GR_GL_VER(3, 2)) {
            halfFPRenderTargetSupport = HalfFPRenderTargetSupport::kAll;
        } else if (ctxInfo.hasExtension("GL_EXT_color_buffer_float")) {
            halfFPRenderTargetSupport = HalfFPRenderTargetSupport::kAll;
        } else if (ctxInfo.hasExtension("GL_EXT_color_buffer_half_float")) {
            // This extension only enables half float support rendering for RGBA.
            halfFPRenderTargetSupport = HalfFPRenderTargetSupport::kRGBAOnly;
        }
    } else if (GR_IS_GR_WEBGL(standard)) {
        if ((ctxInfo.hasExtension("GL_OES_texture_float_linear") &&
             ctxInfo.hasExtension("GL_OES_texture_float")) ||
            (ctxInfo.hasExtension("OES_texture_float_linear") &&
             ctxInfo.hasExtension("OES_texture_float"))) {
            hasFP16Textures = true;
        } else if ((ctxInfo.hasExtension("GL_OES_texture_half_float_linear") &&
                    ctxInfo.hasExtension("GL_OES_texture_half_float")) ||
                   (ctxInfo.hasExtension("OES_texture_half_float_linear") &&
                    ctxInfo.hasExtension("OES_texture_half_float"))) {
            hasFP16Textures = true;
        }

        if (ctxInfo.hasExtension("GL_WEBGL_color_buffer_float") ||
            ctxInfo.hasExtension("WEBGL_color_buffer_float")) {
            halfFPRenderTargetSupport = HalfFPRenderTargetSupport::kAll;
        } else if (ctxInfo.hasExtension("GL_EXT_color_buffer_half_float") ||
                   ctxInfo.hasExtension("EXT_color_buffer_half_float")) {
            // This extension only enables half float support rendering for RGBA.
            halfFPRenderTargetSupport = HalfFPRenderTargetSupport::kRGBAOnly;
        }
    }

    // For desktop:
    //    GL 3.0 requires support for R16 & RG16
    //    GL_ARB_texture_rg adds R16 & RG16 support for OpenGL 1.1 and above
    // For ES:
    //    GL_EXT_texture_norm16 adds support for both texturing and rendering
    //    There is also the GL_NV_image_formats extension - for further investigation
    bool r16AndRG1616Supported = false;
    if (GR_IS_GR_GL(standard)) {
        if (version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_ARB_texture_rg")) {
            r16AndRG1616Supported = true;
        }
    } else if (GR_IS_GR_GL_ES(standard)) {
        if (ctxInfo.hasExtension("GL_EXT_texture_norm16")) {
            r16AndRG1616Supported = true;
        }
    } // No WebGL support

    for (int i = 0; i < kGrColorTypeCnt; ++i) {
        fColorTypeToFormatTable[i] = GrGLFormat::kUnknown;
    }

    ///////////////////////////////////////////////////////////////////////////

    GrGLenum halfFloatType = GR_GL_HALF_FLOAT;
    if (GR_IS_GR_GL_ES(standard) && version < GR_GL_VER(3, 0)) {
        halfFloatType = GR_GL_HALF_FLOAT_OES;
    }

    // RGBA8
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRGBA8);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RGBA;
        info.fSizedInternalFormat = GR_GL_RGBA8;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RGBA8 : GR_GL_RGBA;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RGBA8 : GR_GL_RGBA;
        info.fDefaultExternalType = GR_GL_UNSIGNED_BYTE;
        info.fFlags = FormatInfo::kTextureable_Flag;
        if (GR_IS_GR_GL(standard)) {
            info.fFlags |= msaaRenderFlags;
        } else if (GR_IS_GR_GL_ES(standard)) {
            if (version >= GR_GL_VER(3,0) || ctxInfo.hasExtension("GL_OES_rgb8_rgba8") ||
                ctxInfo.hasExtension("GL_ARM_rgba8")) {
                info.fFlags |= msaaRenderFlags;
            }
        } else if (GR_IS_GR_WEBGL(standard)) {
            info.fFlags |= msaaRenderFlags;
        }

        if (texStorageSupported) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        // kRGBA_8888
        {
            uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
            info.fColorTypeInfos.emplace_back(GrColorType::kRGBA_8888, flags);
            this->setColorTypeFormat(GrColorType::kRGBA_8888, GrGLFormat::kRGBA8);
        }

        // kBGRA_8888
        {
            if (GR_IS_GR_GL(standard)) {
                if (version >= GR_GL_VER(1, 2) || ctxInfo.hasExtension("GL_EXT_bgra")) {
                    uint32_t flags = ColorTypeInfo::kUploadData_Flag |
                                     ColorTypeInfo::kRenderable_Flag;
                    info.fColorTypeInfos.emplace_back(GrColorType::kBGRA_8888, flags);
                    this->setColorTypeFormat(GrColorType::kBGRA_8888, GrGLFormat::kRGBA8);
                }
            }
        }

        // kRGB_888x
        {
            uint32_t flags = ColorTypeInfo::kUploadData_Flag;
            info.fColorTypeInfos.emplace_back(GrColorType::kRGB_888x, flags);
        }
    }

    // R8
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kR8);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RED;
        info.fSizedInternalFormat = GR_GL_R8;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_R8 : GR_GL_RED;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_R8 : GR_GL_RED;
        info.fDefaultExternalType = GR_GL_UNSIGNED_BYTE;
        if (textureRedSupport) {
            info.fFlags |= FormatInfo::kTextureable_Flag | msaaRenderFlags;
        }

        if (texStorageSupported &&
            !formatWorkarounds.fDisablePerFormatTextureStorageForCommandBufferES2) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        if (textureRedSupport) {
            // kAlpha_8
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kAlpha_8, flags, GrSwizzle("000r"));
                this->setColorTypeFormat(GrColorType::kAlpha_8, GrGLFormat::kR8);
            }

            // kGray_8
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kGray_8, flags);
                this->setColorTypeFormat(GrColorType::kGray_8, GrGLFormat::kR8);
            }
        }
    }

    // ALPHA8
    {
        bool alpha8IsValidForGL = GR_IS_GR_GL(standard) &&
                (!fIsCoreProfile || version <= GR_GL_VER(3, 0));
        bool alpha8IsValidForGLES = GR_IS_GR_GL_ES(standard) && version < GR_GL_VER(3, 0);
        bool alpha8IsValidForWebGL = GR_IS_GR_WEBGL(standard);

        FormatInfo& info = this->getFormatInfo(GrGLFormat::kALPHA8);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_ALPHA;
        info.fSizedInternalFormat = GR_GL_ALPHA8;
        if (GR_IS_GR_GL_ES(standard) || !texImageSupportsSizedInternalFormat) {
            // ES does not have sized internal format GL_ALPHA8.
            info.fInternalFormatForTexImage = GR_GL_ALPHA;
        } else {
            info.fInternalFormatForTexImage = GR_GL_ALPHA8;
        }
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_ALPHA8 : GR_GL_ALPHA;

        info.fDefaultExternalType = GR_GL_UNSIGNED_BYTE;
        if (alpha8IsValidForGL || alpha8IsValidForGLES || alpha8IsValidForWebGL) {
            info.fFlags = FormatInfo::kTextureable_Flag;
        }
        if (fAlpha8IsRenderable && alpha8IsValidForGL) {
            info.fFlags |= msaaRenderFlags;
        }
        if (texStorageSupported &&
            !formatWorkarounds.fDisablePerFormatTextureStorageForCommandBufferES2 &&
            !formatWorkarounds.fDisableNonRedSingleChannelTexStorageForANGLEGL) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        if (alpha8IsValidForGL || alpha8IsValidForGLES || alpha8IsValidForWebGL) {
            // kAlpha_8
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                if (alpha8IsValidForGL || alpha8IsValidForGLES || alpha8IsValidForWebGL) {
                    info.fColorTypeInfos.emplace_back(GrColorType::kAlpha_8, flags,
                                                      GrSwizzle("000r"));
                    int idx = static_cast<int>(GrColorType::kAlpha_8);
                    if (fColorTypeToFormatTable[idx] == GrGLFormat::kUnknown) {
                        this->setColorTypeFormat(GrColorType::kAlpha_8, GrGLFormat::kALPHA8);
                    }
                }
            }
        }
    }

    // LUMINANCE8
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kLUMINANCE8);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_LUMINANCE;
        info.fSizedInternalFormat = GR_GL_LUMINANCE8;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_LUMINANCE8 : GR_GL_LUMINANCE;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_LUMINANCE8 : GR_GL_LUMINANCE;
        info.fDefaultExternalType = GR_GL_UNSIGNED_BYTE;
        bool supportsLum = (GR_IS_GR_GL(standard) && version <= GR_GL_VER(3, 0)) ||
                           (GR_IS_GR_GL_ES(standard) && version < GR_GL_VER(3, 0)) ||
                           (GR_IS_GR_WEBGL(standard));
        if (supportsLum) {
            info.fFlags = FormatInfo::kTextureable_Flag;
        }
        if (texStorageSupported &&
            !formatWorkarounds.fDisablePerFormatTextureStorageForCommandBufferES2 &&
            !formatWorkarounds.fDisableNonRedSingleChannelTexStorageForANGLEGL) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }
        // We are not enabling attaching to an FBO for LUMINANCE8 mostly because of confusion in the
        // spec. For GLES it does not seem to ever support LUMINANCE8 being color-renderable. For GL
        // versions less than 3.0 it is provided by GL_ARB_framebuffer_object. However, the original
        // version of that extension did not add LUMINANCE8, but was added in a later revsion. So
        // even the presence of that extension does not guarantee support. GL 3.0 and higher (core
        // or compatibility) do not list LUMINANCE8 as color-renderable (which is strange since the
        // GL_ARB_framebuffer_object extension was meant to bring 3.0 functionality to lower
        // versions).

        if (supportsLum) {
            // kGray_8
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kGray_8, flags);
                int idx = static_cast<int>(GrColorType::kGray_8);
                if (fColorTypeToFormatTable[idx] == GrGLFormat::kUnknown) {
                    this->setColorTypeFormat(GrColorType::kGray_8, GrGLFormat::kLUMINANCE8);
                }
            }
        }

    }

    // BGRA8
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kBGRA8);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_BGRA;
        info.fSizedInternalFormat = GR_GL_BGRA8;
        // If BGRA is supported as an internal format it must always be specified to glTex[Sub]Image
        // as a base format. Which base format depends on which extension is used.
        if (ctxInfo.hasExtension("GL_APPLE_texture_format_BGRA8888")) {
            // GL_APPLE_texture_format_BGRA8888:
            //     ES 2.0: the extension makes BGRA an external format but not an internal format.
            //     ES 3.0: the extension explicitly states GL_BGRA8 is not a valid internal format
            //             for glTexImage (just for glTexStorage).
            info.fInternalFormatForTexImage = GR_GL_RGBA;
        } else {
            // GL_EXT_texture_format_BGRA8888:
            //      This extension adds GL_BGRA as an unsized internal format. However, it is
            //      written against ES 2.0 and therefore doesn't define a GL_BGRA8 as ES 2.0 doesn't
            //      have sized internal formats.
            info.fInternalFormatForTexImage = GR_GL_BGRA;
        }

        // We currently only use the renderbuffer format when allocating msaa renderbuffers, so we
        // are making decisions here based on that use case. The GL_EXT_texture_format_BGRA8888
        // extension adds BGRA color renderbuffer support for ES 2.0, but this does not guarantee
        // support for MSAA renderbuffers. Additionally, the renderable support was added in a later
        // revision of the extension. So it is possible for older drivers to support the extension
        // but only an early revision of it without renderable support. We have no way of
        // distinguishing between the two. The GL_APPLE_texture_format_BGRA8888 does not add support
        // for BGRA color renderbuffers at all. Ideally, for both cases we would use RGBA[8] for our
        // format for the MSAA buffer. In the GL_EXT_texture_format_BGRA8888 case we can still
        // make the resolve BGRA and which will work for glBlitFramebuffer for resolving which just
        // requires the src and dst be bindable to FBOs. However, we can't do this in the current
        // world since some devices (e.g. chromium & angle) require the formats in glBlitFramebuffer
        // to match. We don't have a way to really check this during resolve since we only actually
        // have one GrPixelConfig and one GrBackendFormat that is shared by the GrGLRenderTarget.
        // Once we break those up into different surface we can revisit doing this change.
        if (ctxInfo.hasExtension("GL_APPLE_texture_format_BGRA8888")) {
            if (renderbufferStorageSupportsSizedInternalFormat) {
                info.fInternalFormatForRenderbuffer = GR_GL_RGBA8;
            } else {
                info.fInternalFormatForRenderbuffer = GR_GL_RGBA;
            }
        } else {
            info.fInternalFormatForRenderbuffer =
                    renderbufferStorageSupportsSizedInternalFormat ? GR_GL_BGRA8 : GR_GL_BGRA;
        }

        info.fDefaultExternalType = GR_GL_UNSIGNED_BYTE;
        // TexStorage requires using a sized internal format and BGRA8 is only supported if we have
        // the GL_APPLE_texture_format_BGRA8888 extension or if we have GL_EXT_texture_storage and
        // GL_EXT_texture_format_BGRA8888.
        bool supportsBGRATexStorage = false;

        if (GR_IS_GR_GL_ES(standard)) {
            if (ctxInfo.hasExtension("GL_EXT_texture_format_BGRA8888")) {
                info.fFlags = FormatInfo::kTextureable_Flag | nonMSAARenderFlags;
                // GL_EXT_texture storage has defined interactions with
                // GL_EXT_texture_format_BGRA8888.
                if (ctxInfo.hasExtension("GL_EXT_texture_storage") &&
                    !formatWorkarounds.fDisableBGRATextureStorageForIntelWindowsES) {
                    supportsBGRATexStorage = true;
                }
            } else if (ctxInfo.hasExtension("GL_APPLE_texture_format_BGRA8888")) {
                // This APPLE extension introduces complexity on ES2. It leaves the internal format
                // as RGBA, but allows BGRA as the external format. From testing, it appears that
                // the driver remembers the external format when the texture is created (with
                // TexImage). If you then try to upload data in the other swizzle (with
                // TexSubImage), it fails. We could work around this, but it adds even more state
                // tracking to code that is already too tricky. Instead, we opt not to support BGRA
                // on ES2 with this extension. This also side-steps some ambiguous interactions with
                // the texture storage extension.
                if (version >= GR_GL_VER(3,0)) {
                    // The APPLE extension doesn't explicitly make this renderable, but
                    // internally it appears to use RGBA8, which we'll patch up below.
                    info.fFlags = FormatInfo::kTextureable_Flag | msaaRenderFlags;
                    supportsBGRATexStorage = true;
                }
            }
        }
        if (texStorageSupported && supportsBGRATexStorage) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        if (SkToBool(info.fFlags &FormatInfo::kTextureable_Flag)) {
            // kBGRA_8888
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kBGRA_8888, flags);
                this->setColorTypeFormat(GrColorType::kBGRA_8888, GrGLFormat::kBGRA8);
            }
        }
    }

    // RGB565
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRGB565);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RGB;
        info.fSizedInternalFormat = GR_GL_RGB565;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RGB565 : GR_GL_RGB;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RGB565 : GR_GL_RGB;
        info.fDefaultExternalType = GR_GL_UNSIGNED_SHORT_5_6_5;
        if (GR_IS_GR_GL(standard)) {
            if (version >= GR_GL_VER(4, 2) || ctxInfo.hasExtension("GL_ARB_ES2_compatibility")) {
                info.fFlags = FormatInfo::kTextureable_Flag | msaaRenderFlags;
            }
        } else if (GR_IS_GR_GL_ES(standard)) {
            info.fFlags = FormatInfo::kTextureable_Flag | msaaRenderFlags;
        } else if (GR_IS_GR_WEBGL(standard)) {
            info.fFlags = FormatInfo::kTextureable_Flag | msaaRenderFlags;
        }
        // 565 is not a sized internal format on desktop GL. So on desktop with
        // 565 we always use an unsized internal format to let the system pick
        // the best sized format to convert the 565 data to. Since TexStorage
        // only allows sized internal formats we disallow it.
        //
        // TODO: As of 4.2, regular GL supports 565. This logic is due for an
        // update.
        if (texStorageSupported && GR_IS_GR_GL_ES(standard)) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        if (SkToBool(info.fFlags &FormatInfo::kTextureable_Flag)) {
            // kBGR_565
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kBGR_565, flags);
                this->setColorTypeFormat(GrColorType::kBGR_565, GrGLFormat::kRGB565);
            }
        }
    }

    // RGBA16F
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRGBA16F);
        info.fFormatType = FormatType::kFloat;
        info.fBaseInternalFormat = GR_GL_RGBA;
        info.fSizedInternalFormat = GR_GL_RGBA16F;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RGBA16F : GR_GL_RGBA;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RGBA16F : GR_GL_RGBA;
        info.fDefaultExternalType = halfFloatType;
        if (hasFP16Textures) {
            info.fFlags = FormatInfo::kTextureable_Flag;
            // ES requires 3.2 or EXT_color_buffer_half_float.
            if (halfFPRenderTargetSupport != HalfFPRenderTargetSupport::kNone) {
                info.fFlags |= fpRenderFlags;
            }
        }
        if (texStorageSupported &&
            !formatWorkarounds.fDisablePerFormatTextureStorageForCommandBufferES2) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        if (hasFP16Textures) {
            uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
            // kRGBA_F16
            {
                info.fColorTypeInfos.emplace_back(GrColorType::kRGBA_F16, flags);
                this->setColorTypeFormat(GrColorType::kRGBA_F16, GrGLFormat::kRGBA16F);
            }

            // kRGBA_F16_Clamped
            {
                info.fColorTypeInfos.emplace_back(GrColorType::kRGBA_F16_Clamped, flags);
                this->setColorTypeFormat(GrColorType::kRGBA_F16_Clamped, GrGLFormat::kRGBA16F);
            }
        }
    }

    // R16F
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kR16F);
        info.fFormatType = FormatType::kFloat;
        info.fBaseInternalFormat = GR_GL_RED;
        info.fSizedInternalFormat = GR_GL_R16F;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_R16F : GR_GL_RED;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_R16F : GR_GL_RED;
        info.fDefaultExternalType = halfFloatType;
        if (textureRedSupport && hasFP16Textures) {
            info.fFlags = FormatInfo::kTextureable_Flag;
            if (halfFPRenderTargetSupport == HalfFPRenderTargetSupport::kAll) {
                info.fFlags |= fpRenderFlags;
            }
        }
        if (texStorageSupported &&
            !formatWorkarounds.fDisablePerFormatTextureStorageForCommandBufferES2) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        if (textureRedSupport && hasFP16Textures) {
            // kAlpha_F16
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kAlpha_F16, flags,
                                                  GrSwizzle("000r"));
                this->setColorTypeFormat(GrColorType::kAlpha_F16, GrGLFormat::kR16F);
            }
        }
    }

    // LUMINANCE16F
    {
        // NOTE: We disallow lum16f on ES devices if linear filtering modes are not
        // supported. This is for simplicity, but a more granular approach is possible.
        bool lum16FSupported = false;

        if (GR_IS_GR_GL(standard)) {
            if (version >= GR_GL_VER(3, 0)) {
                lum16FSupported = true;
            } else if (ctxInfo.hasExtension("GL_ARB_texture_float")) {
                lum16FSupported = true;
            }
        } else if (GR_IS_GR_GL_ES(standard)) {
            if (version >= GR_GL_VER(3, 0)) {
                lum16FSupported = true;
            } else if (ctxInfo.hasExtension("GL_OES_texture_float_linear") &&
                       ctxInfo.hasExtension("GL_OES_texture_float")) {
                lum16FSupported = true;
            } else if (ctxInfo.hasExtension("GL_OES_texture_half_float_linear") &&
                       ctxInfo.hasExtension("GL_OES_texture_half_float")) {
                lum16FSupported = true;
            }
        } // No WebGL support

        if (formatWorkarounds.fDisableLuminance16F) {
            lum16FSupported = false;
        }

        FormatInfo& info = this->getFormatInfo(GrGLFormat::kLUMINANCE16F);
        info.fFormatType = FormatType::kFloat;
        info.fBaseInternalFormat = GR_GL_LUMINANCE;
        info.fSizedInternalFormat = GR_GL_LUMINANCE16F;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_LUMINANCE16F : GR_GL_LUMINANCE;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_LUMINANCE16F
                                                               : GR_GL_LUMINANCE;
        info.fDefaultExternalType = halfFloatType;

        if (lum16FSupported) {
            info.fFlags = FormatInfo::kTextureable_Flag;
        }
        if (texStorageSupported &&
            !formatWorkarounds.fDisablePerFormatTextureStorageForCommandBufferES2) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        if (lum16FSupported) {
            // kAlpha_F16
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kAlpha_F16, flags);

                int idx = static_cast<int>(GrColorType::kAlpha_F16);
                if (fColorTypeToFormatTable[idx] == GrGLFormat::kUnknown) {
                    this->setColorTypeFormat(GrColorType::kAlpha_F16, GrGLFormat::kLUMINANCE16F);
                }
            }
        }
    }

    // RGB8
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRGB8);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RGB;
        info.fSizedInternalFormat = GR_GL_RGB8;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RGB8 : GR_GL_RGB;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RGB8 : GR_GL_RGB;
        info.fDefaultExternalType = GR_GL_UNSIGNED_BYTE;
        info.fFlags = FormatInfo::kTextureable_Flag;
        if (GR_IS_GR_GL(standard)) {
            // Even in OpenGL 4.6 GL_RGB8 is required to be color renderable but not required to be
            // a supported render buffer format. Since we usually use render buffers for MSAA on
            // non-ES GL we don't support MSAA for GL_RGB8. On 4.2+ we could check using
            // glGetInternalFormativ(GL_RENDERBUFFER, GL_RGB8, GL_INTERNALFORMAT_SUPPORTED, ...) if
            // this becomes an issue.
            // This also would probably work in mixed-samples mode where there is no MSAA color
            // buffer but we don't support that just for simplicity's sake.
            info.fFlags |= nonMSAARenderFlags;
        } else if (GR_IS_GR_GL_ES(standard)) {
            // 3.0 and the extension support this as a render buffer format.
            if (version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_OES_rgb8_rgba8")) {
                info.fFlags |= msaaRenderFlags;
            }
        } else if (GR_IS_GR_WEBGL(standard)) {
            // WebGL seems to support RBG8
            info.fFlags |= msaaRenderFlags;
        }
        if (texStorageSupported) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }
        if (formatWorkarounds.fDisableRGB8ForMali400) {
            info.fFlags = 0;
        }

        // kRGB_888x
        {
            uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
            info.fColorTypeInfos.emplace_back(GrColorType::kRGB_888x, flags);
            this->setColorTypeFormat(GrColorType::kRGB_888x, GrGLFormat::kRGB8);
        }
    }

    // RG8
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRG8);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RG;
        info.fSizedInternalFormat = GR_GL_RG8;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RG8 : GR_GL_RG;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RG8 : GR_GL_RG;
        info.fDefaultExternalType = GR_GL_UNSIGNED_BYTE;
        if (textureRedSupport) {
            info.fFlags |= FormatInfo::kTextureable_Flag | msaaRenderFlags;
            if (texStorageSupported &&
                !formatWorkarounds.fDisablePerFormatTextureStorageForCommandBufferES2) {
                info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
            }
        }

        if (textureRedSupport) {
            // kRG_88
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kRG_88, flags);
                this->setColorTypeFormat(GrColorType::kRG_88, GrGLFormat::kRG8);
            }
        }
    }

    // RGB10_A2
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRGB10_A2);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RGBA;
        info.fSizedInternalFormat = GR_GL_RGB10_A2;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RGB10_A2 : GR_GL_RGBA;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RGB10_A2 : GR_GL_RGBA;
        info.fDefaultExternalType = GR_GL_UNSIGNED_INT_2_10_10_10_REV;
        if (GR_IS_GR_GL(standard) ||
           (GR_IS_GR_GL_ES(standard) && version >= GR_GL_VER(3, 0))) {
            info.fFlags = FormatInfo::kTextureable_Flag | msaaRenderFlags;
        } else if (GR_IS_GR_GL_ES(standard) &&
                   ctxInfo.hasExtension("GL_EXT_texture_type_2_10_10_10_REV")) {
            info.fFlags = FormatInfo::kTextureable_Flag;
        } // No WebGL support
        if (texStorageSupported) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        if (SkToBool(info.fFlags &FormatInfo::kTextureable_Flag)) {
            // kRGBA_1010102
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kRGBA_1010102, flags);
                this->setColorTypeFormat(GrColorType::kRGBA_1010102, GrGLFormat::kRGB10_A2);
            }
        }
    }

    // RGBA4
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRGBA4);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RGBA;
        info.fSizedInternalFormat = GR_GL_RGBA4;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RGBA4 : GR_GL_RGBA;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RGBA4 : GR_GL_RGBA;
        info.fDefaultExternalType = GR_GL_UNSIGNED_SHORT_4_4_4_4;
        info.fFlags = FormatInfo::kTextureable_Flag;
        if (GR_IS_GR_GL(standard)) {
            if (version >= GR_GL_VER(4, 2)) {
                info.fFlags |= msaaRenderFlags;
            }
        } else if (GR_IS_GR_GL_ES(standard)) {
            info.fFlags |= msaaRenderFlags;
        } else if (GR_IS_GR_WEBGL(standard)) {
            info.fFlags |= msaaRenderFlags;
        }
        if (texStorageSupported) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        // kABGR_4444
        {
            uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
            info.fColorTypeInfos.emplace_back(GrColorType::kABGR_4444, flags);
            this->setColorTypeFormat(GrColorType::kABGR_4444, GrGLFormat::kRGBA4);
        }
    }

    // RGBA32F
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRGBA32F);
        info.fFormatType = FormatType::kFloat;
        info.fBaseInternalFormat = GR_GL_RGBA;
        info.fSizedInternalFormat = GR_GL_RGBA32F;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RGBA32F : GR_GL_RGBA;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RGBA32F : GR_GL_RGBA;
        // We don't allow texturing or rendering to this format
    }

    // SRGB8_ALPHA8
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kSRGB8_ALPHA8);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RGBA;
        info.fSizedInternalFormat = GR_GL_SRGB8_ALPHA8;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_SRGB8_ALPHA8 : GR_GL_SRGB_ALPHA;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_SRGB8_ALPHA8
                                                               : GR_GL_SRGB_ALPHA;
        info.fDefaultExternalType = GR_GL_UNSIGNED_BYTE;
        if (fSRGBSupport) {
            uint32_t srgbRenderFlags =
                    formatWorkarounds.fDisableSRGBRenderWithMSAAForMacAMD ? nonMSAARenderFlags
                                                                          : msaaRenderFlags;

            info.fFlags = FormatInfo::kTextureable_Flag | srgbRenderFlags;
        }
        if (texStorageSupported &&
            !formatWorkarounds.fDisablePerFormatTextureStorageForCommandBufferES2) {
            info.fFlags |= FormatInfo::kCanUseTexStorage_Flag;
        }

        if (fSRGBSupport) {
            // kRGBA_8888_SRGB
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kRGBA_8888_SRGB, flags);
                this->setColorTypeFormat(GrColorType::kRGBA_8888_SRGB, GrGLFormat::kSRGB8_ALPHA8);
            }
        }
    }

    // COMPRESSED_RGB8_ETC2
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kCOMPRESSED_RGB8_ETC2);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RGB;
        info.fCompressedInternalFormat = GR_GL_COMPRESSED_RGB8_ETC2;
        if (GR_IS_GR_GL(standard)) {
            if (version >= GR_GL_VER(4, 3) || ctxInfo.hasExtension("GL_ARB_ES3_compatibility")) {
                info.fFlags = FormatInfo::kTextureable_Flag;
            }
        } else if (GR_IS_GR_GL_ES(standard)) {
            if (version >= GR_GL_VER(3, 0) ||
                ctxInfo.hasExtension("GL_OES_compressed_ETC2_RGB8_texture")) {
                info.fFlags = FormatInfo::kTextureable_Flag;
            }
        } // No WebGL support

        // There are no support GrColorTypes for this format
    }

    // COMPRESSED_ETC1_RGB8
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kCOMPRESSED_ETC1_RGB8);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RGB;
        info.fCompressedInternalFormat = GR_GL_COMPRESSED_ETC1_RGB8;
        if (GR_IS_GR_GL_ES(standard)) {
            if (ctxInfo.hasExtension("GL_OES_compressed_ETC1_RGB8_texture")) {
                info.fFlags = FormatInfo::kTextureable_Flag;
            }
        } // No GL or WebGL support

        // There are no support GrColorTypes for this format
    }

    // GR_GL_R16
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kR16);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RED;
        info.fSizedInternalFormat = GR_GL_R16;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_R16 : GR_GL_RED;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_R16 : GR_GL_RED;
        info.fDefaultExternalType = GR_GL_UNSIGNED_SHORT;
        if (r16AndRG1616Supported) {
            info.fFlags = FormatInfo::kTextureable_Flag | msaaRenderFlags;
        }

        if (r16AndRG1616Supported) {
            // kR_16
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kR_16, flags);
                this->setColorTypeFormat(GrColorType::kR_16, GrGLFormat::kR16);
            }
        }
    }

    // GR_GL_RG16
    {
        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRG16);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RG;
        info.fSizedInternalFormat = GR_GL_RG16;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RG16 : GR_GL_RG;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RG16 : GR_GL_RG;
        info.fDefaultExternalType = GR_GL_UNSIGNED_SHORT;
        if (r16AndRG1616Supported) {
            info.fFlags = FormatInfo::kTextureable_Flag | msaaRenderFlags;
        }

        if (r16AndRG1616Supported) {
            // kRG_1616
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kRG_1616, flags);
                this->setColorTypeFormat(GrColorType::kRG_1616, GrGLFormat::kRG16);
            }
        }
    }

    // GR_GL_RGBA16
    {
        // For desktop:
        //    GL 3.0 requires both texture and render support for RGBA16
        // For ES:
        //    GL_EXT_texture_norm16 adds support for both texturing and rendering
        //    There is also the GL_NV_image_formats extension - for further investigation
        //
        // This is basically the same as R16F and RG16F except the GL_ARB_texture_rg extension
        // doesn't add this format
        bool rgba16161616Supported = false;
        if (GR_IS_GR_GL(standard)) {
            if (version >= GR_GL_VER(3, 0)) {
                rgba16161616Supported = true;
            }
        } else if (GR_IS_GR_GL_ES(standard)) {
            if (ctxInfo.hasExtension("GL_EXT_texture_norm16")) {
                rgba16161616Supported = true;
            }
        } // No WebGL support

        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRGBA16);
        info.fFormatType = FormatType::kNormalizedFixedPoint;
        info.fBaseInternalFormat = GR_GL_RGBA;
        info.fSizedInternalFormat = GR_GL_RGBA16;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RGBA16 : GR_GL_RGBA;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RGBA16 : GR_GL_RGBA;
        info.fDefaultExternalType = GR_GL_UNSIGNED_SHORT;
        if (rgba16161616Supported) {
            info.fFlags = FormatInfo::kTextureable_Flag | msaaRenderFlags;
        }

        if (rgba16161616Supported) {
            // kRGBA_16161616
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kRGBA_16161616, flags);
                this->setColorTypeFormat(GrColorType::kRGBA_16161616, GrGLFormat::kRGBA16);
            }
        }
    }

    // GR_GL_RG16F
    {
        bool rg16fTexturesSupported = false;
        bool rg16fRenderingSupported = false;
        // For desktop:
        //  3.0 requires both texture and render support
        //  GL_ARB_texture_rg adds both texture and render support
        // For ES:
        //  3.2 requires RG16F as both renderable and texturable
        //  3.0 only requires RG16F as texture-only
        //  GL_EXT_color_buffer_float adds texture and render support
        if (GR_IS_GR_GL(standard)) {
            if (version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_ARB_texture_rg")) {
                rg16fTexturesSupported = true;
                rg16fRenderingSupported = true;
            }
        } else if (GR_IS_GR_GL_ES(standard)) {
            if (version >= GR_GL_VER(3, 2) || ctxInfo.hasExtension("GL_EXT_color_buffer_float")) {
                rg16fTexturesSupported = true;
                rg16fRenderingSupported = true;
            } else if (version >= GR_GL_VER(3, 0)) {
                rg16fTexturesSupported = true;      // texture only
            }
        }

        FormatInfo& info = this->getFormatInfo(GrGLFormat::kRG16F);
        info.fFormatType = FormatType::kFloat;
        info.fBaseInternalFormat = GR_GL_RG;
        info.fSizedInternalFormat = GR_GL_RG16F;
        info.fInternalFormatForTexImage =
                texImageSupportsSizedInternalFormat ? GR_GL_RG16F : GR_GL_RG;
        info.fInternalFormatForRenderbuffer =
                renderbufferStorageSupportsSizedInternalFormat ? GR_GL_RG16F : GR_GL_RG;
        info.fDefaultExternalType = halfFloatType;
        if (rg16fTexturesSupported) {
            info.fFlags |= FormatInfo::kTextureable_Flag;
        }
        if (rg16fRenderingSupported) {
            info.fFlags |= fpRenderFlags;
        }
        SkASSERT(rg16fTexturesSupported || !rg16fRenderingSupported);

        if (rg16fTexturesSupported) {
            // kRG_F16
            {
                uint32_t flags = ColorTypeInfo::kUploadData_Flag | ColorTypeInfo::kRenderable_Flag;
                info.fColorTypeInfos.emplace_back(GrColorType::kRG_F16, flags);
                this->setColorTypeFormat(GrColorType::kRG_F16, GrGLFormat::kRG16F);
            }
        }
    }

    this->setupSampleCounts(ctxInfo, gli);

#ifdef SK_DEBUG
    for (int i = 0; i < kGrGLFormatCount; ++i) {
        if (GrGLFormat::kUnknown == static_cast<GrGLFormat>(i)) {
            continue;
        }
        // Make sure we didn't set fbo attachable with msaa and not fbo attachable.
        SkASSERT(!((fFormatTable[i].fFlags & FormatInfo::kFBOColorAttachmentWithMSAA_Flag) &&
                  !(fFormatTable[i].fFlags & FormatInfo::kFBOColorAttachment_Flag)));

        // Make sure we set all the formats' FormatType
        SkASSERT(fFormatTable[i].fFormatType != FormatType::kUnknown);
    }
#endif
}

void GrGLCaps::setupSampleCounts(const GrGLContextInfo& ctxInfo, const GrGLInterface* gli) {
    GrGLStandard standard = ctxInfo.standard();
    // standard can be unused (optimized away) if SK_ASSUME_GL_ES is set
    sk_ignore_unused_variable(standard);
    GrGLVersion version = ctxInfo.version();

    for (int i = 0; i < kGrGLFormatCount; ++i) {
        if (FormatInfo::kFBOColorAttachmentWithMSAA_Flag & fFormatTable[i].fFlags) {
            // We assume that MSAA rendering is supported only if we support non-MSAA rendering.
            SkASSERT(FormatInfo::kFBOColorAttachment_Flag & fFormatTable[i].fFlags);
            if ((GR_IS_GR_GL(standard) &&
                  (version >= GR_GL_VER(4,2) ||
                   ctxInfo.hasExtension("GL_ARB_internalformat_query"))) ||
                (GR_IS_GR_GL_ES(standard) && version >= GR_GL_VER(3,0))) {
                int count;
                GrGLFormat grGLFormat = static_cast<GrGLFormat>(i);
                GrGLenum glFormat = this->getRenderbufferInternalFormat(grGLFormat);
                GR_GL_GetInternalformativ(gli, GR_GL_RENDERBUFFER, glFormat,
                                          GR_GL_NUM_SAMPLE_COUNTS, 1, &count);
                if (count) {
                    std::unique_ptr<int[]> temp(new int[count]);
                    GR_GL_GetInternalformativ(gli, GR_GL_RENDERBUFFER, glFormat, GR_GL_SAMPLES,
                                              count, temp.get());
                    // GL has a concept of MSAA rasterization with a single sample but we do not.
                    if (count && temp[count - 1] == 1) {
                        --count;
                        SkASSERT(!count || temp[count -1] > 1);
                    }
                    fFormatTable[i].fColorSampleCounts.setCount(count+1);
                    // We initialize our supported values with 1 (no msaa) and reverse the order
                    // returned by GL so that the array is ascending.
                    fFormatTable[i].fColorSampleCounts[0] = 1;
                    for (int j = 0; j < count; ++j) {
                        fFormatTable[i].fColorSampleCounts[j+1] = temp[count - j - 1];
                    }
                }
            } else {
                // Fake out the table using some semi-standard counts up to the max allowed sample
                // count.
                int maxSampleCnt = 1;
                if (GrGLCaps::kES_IMG_MsToTexture_MSFBOType == fMSFBOType) {
                    GR_GL_GetIntegerv(gli, GR_GL_MAX_SAMPLES_IMG, &maxSampleCnt);
                } else if (GrGLCaps::kNone_MSFBOType != fMSFBOType) {
                    GR_GL_GetIntegerv(gli, GR_GL_MAX_SAMPLES, &maxSampleCnt);
                }
                // Chrome has a mock GL implementation that returns 0.
                maxSampleCnt = SkTMax(1, maxSampleCnt);

                static constexpr int kDefaultSamples[] = {1, 2, 4, 8};
                int count = SK_ARRAY_COUNT(kDefaultSamples);
                for (; count > 0; --count) {
                    if (kDefaultSamples[count - 1] <= maxSampleCnt) {
                        break;
                    }
                }
                if (count > 0) {
                    fFormatTable[i].fColorSampleCounts.append(count, kDefaultSamples);
                }
            }
        } else if (FormatInfo::kFBOColorAttachment_Flag & fFormatTable[i].fFlags) {
            fFormatTable[i].fColorSampleCounts.setCount(1);
            fFormatTable[i].fColorSampleCounts[0] = 1;
        }
    }
}

void GrGLCaps::initConfigTable(const GrContextOptions& contextOptions,
                               const GrGLContextInfo& ctxInfo, const GrGLInterface* gli) {
    /*
        Comments on renderability of configs on various GL versions.
          OpenGL < 3.0:
            no built in support for render targets.
            GL_EXT_framebuffer_object adds possible support for any sized format with base internal
              format RGB, RGBA and NV float formats we don't use.
              This is the following:
                R3_G3_B2, RGB4, RGB5, RGB8, RGB10, RGB12, RGB16, RGBA2, RGBA4, RGB5_A1, RGBA8
                RGB10_A2, RGBA12,RGBA16
              Though, it is hard to believe the more obscure formats such as RGBA12 would work
              since they aren't required by later standards and the driver can simply return
              FRAMEBUFFER_UNSUPPORTED for anything it doesn't allow.
            GL_ARB_framebuffer_object adds everything added by the EXT extension and additionally
              any sized internal format with a base internal format of ALPHA, LUMINANCE,
              LUMINANCE_ALPHA, INTENSITY, RED, and RG.
              This adds a lot of additional renderable sized formats, including ALPHA8.
              The GL_ARB_texture_rg brings in the RED and RG formats (8, 8I, 8UI, 16, 16I, 16UI,
              16F, 32I, 32UI, and 32F variants).
              Again, the driver has an escape hatch via FRAMEBUFFER_UNSUPPORTED.

            For both the above extensions we limit ourselves to those that are also required by
            OpenGL 3.0.

          OpenGL 3.0:
            Any format with base internal format ALPHA, RED, RG, RGB or RGBA is "color-renderable"
            but are not required to be supported as renderable textures/renderbuffer.
            Required renderable color formats:
                - RGBA32F, RGBA32I, RGBA32UI, RGBA16, RGBA16F, RGBA16I,
                  RGBA16UI, RGBA8, RGBA8I, RGBA8UI, SRGB8_ALPHA8, and
                  RGB10_A2.
                - R11F_G11F_B10F.
                - RG32F, RG32I, RG32UI, RG16, RG16F, RG16I, RG16UI, RG8, RG8I,
                  and RG8UI.
                - R32F, R32I, R32UI, R16F, R16I, R16UI, R16, R8, R8I, and R8UI.
                - ALPHA8

          OpenGL 3.1, 3.2, 3.3
            Same as 3.0 except ALPHA8 requires GL_ARB_compatibility/compatibility profile.
          OpengGL 3.3, 4.0, 4.1
            Adds RGB10_A2UI.
          OpengGL 4.2
            Adds
                - RGB5_A1, RGBA4
                - RGB565
          OpenGL 4.4
            Does away with the separate list and adds a column to the sized internal color format
            table. However, no new formats become required color renderable.

          ES 2.0
            color renderable: RGBA4, RGB5_A1, RGB565
            GL_EXT_texture_rg adds support for R8, RG8 as a color render target
            GL_OES_rgb8_rgba8 adds support for RGB8 and RGBA8
            GL_ARM_rgba8 adds support for RGBA8 (but not RGB8)
            GL_EXT_texture_format_BGRA8888 does not add renderbuffer support
            GL_CHROMIUM_renderbuffer_format_BGRA8888 adds BGRA8 as color-renderable
            GL_APPLE_texture_format_BGRA8888 does not add renderbuffer support

          ES 3.0
                - RGBA32I, RGBA32UI, RGBA16I, RGBA16UI, RGBA8, RGBA8I,
                  RGBA8UI, SRGB8_ALPHA8, RGB10_A2, RGB10_A2UI, RGBA4, and
                  RGB5_A1.
                - RGB8 and RGB565.
                - RG32I, RG32UI, RG16I, RG16UI, RG8, RG8I, and RG8UI.
                - R32I, R32UI, R16I, R16UI, R8, R8I, and R8UI
          ES 3.1
            Adds RGB10_A2, RGB10_A2UI,
          ES 3.2
            Adds R16F, RG16F, RGBA16F, R32F, RG32F, RGBA32F, R11F_G11F_B10F.
    */

    GrGLStandard standard = ctxInfo.standard();
    // standard can be unused (optimzed away) if SK_ASSUME_GL_ES is set
    sk_ignore_unused_variable(standard);
    GrGLVersion version = ctxInfo.version();

    // Correctness workarounds.
    bool disableTextureRedForMesa = false;

    if (!contextOptions.fDisableDriverCorrectnessWorkarounds) {
        // ARB_texture_rg is part of OpenGL 3.0, but osmesa doesn't support GL_RED
        // and GL_RG on FBO textures.
        disableTextureRedForMesa = kOSMesa_GrGLRenderer == ctxInfo.renderer();
    }

    bool textureRedSupport = false;

    if (!disableTextureRedForMesa) {
        if (GR_IS_GR_GL(standard)) {
            textureRedSupport =
                    version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_ARB_texture_rg");
        } else if (GR_IS_GR_GL_ES(standard)) {
            textureRedSupport =
                    version >= GR_GL_VER(3, 0) || ctxInfo.hasExtension("GL_EXT_texture_rg");
        }
    } // No WebGL support

    fConfigTable[kUnknown_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = 0;
    fConfigTable[kUnknown_GrPixelConfig].fFormats.fExternalType = 0;

    fConfigTable[kRGBA_8888_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] =
        GR_GL_RGBA;
    fConfigTable[kRGBA_8888_GrPixelConfig].fFormats.fExternalType = GR_GL_UNSIGNED_BYTE;

    // Our external RGB data always has a byte where alpha would be. When calling read pixels we
    // want to read to kRGB_888x color type and ensure that gets 0xFF written. Using GL_RGB would
    // read back unaligned 24bit RGB color values.
    fConfigTable[kRGB_888_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_RGBA;
    fConfigTable[kRGB_888_GrPixelConfig].fFormats.fExternalType = GR_GL_UNSIGNED_BYTE;

    fConfigTable[kRGB_888X_GrPixelConfig] = fConfigTable[kRGBA_8888_GrPixelConfig];

    fConfigTable[kRG_88_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] =
        GR_GL_RG;
    fConfigTable[kRG_88_GrPixelConfig].fFormats.fExternalType = GR_GL_UNSIGNED_BYTE;

    fConfigTable[kBGRA_8888_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] =
        GR_GL_BGRA;
    fConfigTable[kBGRA_8888_GrPixelConfig].fFormats.fExternalType  = GR_GL_UNSIGNED_BYTE;

    // GL does not do srgb<->rgb conversions when transferring between cpu and gpu. Thus, the
    // external format is GL_RGBA. See below for note about ES2.0 and glTex[Sub]Image.
    fConfigTable[kSRGBA_8888_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] =
        GR_GL_RGBA;
    fConfigTable[kSRGBA_8888_GrPixelConfig].fFormats.fExternalType = GR_GL_UNSIGNED_BYTE;

    fConfigTable[kRGB_565_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] =
        GR_GL_RGB;
    fConfigTable[kRGB_565_GrPixelConfig].fFormats.fExternalType = GR_GL_UNSIGNED_SHORT_5_6_5;

    fConfigTable[kRGBA_4444_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] =
        GR_GL_RGBA;
    fConfigTable[kRGBA_4444_GrPixelConfig].fFormats.fExternalType = GR_GL_UNSIGNED_SHORT_4_4_4_4;

    fConfigTable[kRGBA_1010102_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] =
        GR_GL_RGBA;
    fConfigTable[kRGBA_1010102_GrPixelConfig].fFormats.fExternalType =
        GR_GL_UNSIGNED_INT_2_10_10_10_REV;

    ConfigInfo& alphaInfo = fConfigTable[kAlpha_8_as_Alpha_GrPixelConfig];
    alphaInfo.fFormats.fExternalType = GR_GL_UNSIGNED_BYTE;
    alphaInfo.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_ALPHA;

    ConfigInfo& redInfo = fConfigTable[kAlpha_8_as_Red_GrPixelConfig];
    redInfo.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_RED;
    redInfo.fFormats.fExternalType = GR_GL_UNSIGNED_BYTE;

    if (textureRedSupport) {
        fConfigTable[kAlpha_8_GrPixelConfig] = redInfo;
    } else {
        fConfigTable[kAlpha_8_GrPixelConfig] = alphaInfo;
    }

    ConfigInfo& grayLumInfo = fConfigTable[kGray_8_as_Lum_GrPixelConfig];
    grayLumInfo.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_LUMINANCE;
    grayLumInfo.fFormats.fExternalType = GR_GL_UNSIGNED_BYTE;

    ConfigInfo& grayRedInfo = fConfigTable[kGray_8_as_Red_GrPixelConfig];
    grayRedInfo.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_RED;
    grayRedInfo.fFormats.fExternalType = GR_GL_UNSIGNED_BYTE;

    if (textureRedSupport) {
        fConfigTable[kGray_8_GrPixelConfig] = grayRedInfo;
    } else {
        fConfigTable[kGray_8_GrPixelConfig] = grayLumInfo;
    }

    ConfigInfo& rgbaF32Info = fConfigTable[kRGBA_float_GrPixelConfig];
    rgbaF32Info.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_RGBA;
    rgbaF32Info.fFormats.fExternalType = GR_GL_FLOAT;

    // single channel half formats
    {
        GrGLenum halfExternalType;
        if (GR_IS_GR_GL(standard) ||
            (GR_IS_GR_GL_ES(standard) && version >= GR_GL_VER(3, 0))) {
            halfExternalType = GR_GL_HALF_FLOAT;
        } else {
            halfExternalType = GR_GL_HALF_FLOAT_OES;
        }

        // RED16F
        {
            ConfigInfo& redHalf = fConfigTable[kAlpha_half_as_Red_GrPixelConfig];
            redHalf.fFormats.fExternalType = halfExternalType;
            redHalf.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_RED;

            if (textureRedSupport) {
                fConfigTable[kAlpha_half_GrPixelConfig] = redHalf;
            }
        }

        // LUM16F
        {
            ConfigInfo& lumHalf = fConfigTable[kAlpha_half_as_Lum_GrPixelConfig];
            lumHalf.fFormats.fExternalType = halfExternalType;
            lumHalf.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_LUMINANCE;

            if (!textureRedSupport) {
                fConfigTable[kAlpha_half_GrPixelConfig] = lumHalf;
            }
        }
    }

    fConfigTable[kRGBA_half_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] =
        GR_GL_RGBA;
    if (GR_IS_GR_GL(standard) ||
       (GR_IS_GR_GL_ES(standard) && version >= GR_GL_VER(3, 0))) {
        fConfigTable[kRGBA_half_GrPixelConfig].fFormats.fExternalType = GR_GL_HALF_FLOAT;
    } else {
        fConfigTable[kRGBA_half_GrPixelConfig].fFormats.fExternalType = GR_GL_HALF_FLOAT_OES;
    }

    // kRGBA_half_Clamped is just distinguished by clamps added to the shader. At the API level,
    // it's identical to kRGBA_half.
    fConfigTable[kRGBA_half_Clamped_GrPixelConfig] = fConfigTable[kRGBA_half_GrPixelConfig];

    // Compressed texture support

    // glCompressedTexImage2D is available on all OpenGL ES devices. It is available on standard
    // OpenGL after version 1.3. We'll assume at least that level of OpenGL support.

    // No sized/unsized internal format distinction for compressed formats, no external format.
    // Below we set the external formats and types to 0.
    fConfigTable[kRGB_ETC1_GrPixelConfig].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage]
        = 0;
    fConfigTable[kRGB_ETC1_GrPixelConfig].fFormats.fExternalType = 0;

    // 16 bit formats
    {
        {
            ConfigInfo& r16Info = fConfigTable[kR_16_GrPixelConfig];

            r16Info.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_RED;
            r16Info.fFormats.fExternalType = GR_GL_UNSIGNED_SHORT;
        }

        {
            ConfigInfo& rg1616Info = fConfigTable[kRG_1616_GrPixelConfig];

            rg1616Info.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_RG;
            rg1616Info.fFormats.fExternalType = GR_GL_UNSIGNED_SHORT;
        }

        // Experimental (for Y416)
        {
            ConfigInfo& rgba16161616Info = fConfigTable[kRGBA_16161616_GrPixelConfig];

            rgba16161616Info.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_RGBA;
            rgba16161616Info.fFormats.fExternalType = GR_GL_UNSIGNED_SHORT;
        }
    }

    // Experimental (for Y416 and mutant P016/P010)
    {
        ConfigInfo& rgHalf = fConfigTable[kRG_half_GrPixelConfig];

        rgHalf.fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage] = GR_GL_RG;
        if (GR_IS_GR_GL(standard) || (GR_IS_GR_GL_ES(standard) && version >= GR_GL_VER(3, 0))) {
            rgHalf.fFormats.fExternalType = GR_GL_HALF_FLOAT;
        } else {
            rgHalf.fFormats.fExternalType = GR_GL_HALF_FLOAT_OES;
        }
    }

    for (int i = 0; i < kGrPixelConfigCnt; ++i) {
        // Almost always we want to pass fExternalFormat[kReadPixels_ExternalFormatUsage] as the
        // <format> param to glTex[Sub]Image.
        fConfigTable[i].fFormats.fExternalFormat[kTexImage_ExternalFormatUsage] =
            fConfigTable[i].fFormats.fExternalFormat[kReadPixels_ExternalFormatUsage];
    }

    // OpenGL ES 2.0 + GL_EXT_sRGB allows GL_SRGB_ALPHA to be specified as the <format>
    // param to Tex(Sub)Image. ES 2.0 requires the <internalFormat> and <format> params to match.
    // Thus, on ES 2.0 we will use GL_SRGB_ALPHA as the <format> param.
    // On OpenGL and ES 3.0+ GL_SRGB_ALPHA does not work for the <format> param to glTexImage.
    if (GR_IS_GR_GL_ES(standard) && version == GR_GL_VER(2,0)) {
        fConfigTable[kSRGBA_8888_GrPixelConfig].fFormats.fExternalFormat[kTexImage_ExternalFormatUsage] =
            GR_GL_SRGB_ALPHA;
    }

    // On ES 2.0 we have to use GL_RGB with glTexImage as the internal/external formats must
    // be the same. Moreover, if we write kRGB_888x data to a texture format on non-ES2 we want to
    // be sure that we write 1 for alpha not whatever happens to be in the client provided the 'x'
    // slot.
    fConfigTable[kRGB_888_GrPixelConfig].fFormats.fExternalFormat[kTexImage_ExternalFormatUsage] =
        GR_GL_RGB;

#ifdef SK_DEBUG
    // Make sure we initialized everything by comparing all configs to the
    // default (un-set) values and making sure they are not equal.
    ConfigInfo defaultEntry;
    for (int i = 0; i < kGrPixelConfigCnt; ++i) {
        for (int j = 0; j < kExternalFormatUsageCnt; ++j) {
            SkASSERT(defaultEntry.fFormats.fExternalFormat[j] !=
                     fConfigTable[i].fFormats.fExternalFormat[j]);
        }
        SkASSERT(defaultEntry.fFormats.fExternalType != fConfigTable[i].fFormats.fExternalType);
    }
#endif
}

bool GrGLCaps::canCopyTexSubImage(GrPixelConfig dstConfig, bool dstHasMSAARenderBuffer,
                                  const GrTextureType* dstTypeIfTexture,
                                  GrPixelConfig srcConfig, bool srcHasMSAARenderBuffer,
                                  const GrTextureType* srcTypeIfTexture) const {
    // Table 3.9 of the ES2 spec indicates the supported formats with CopyTexSubImage
    // and BGRA isn't in the spec. There doesn't appear to be any extension that adds it. Perhaps
    // many drivers would allow it to work, but ANGLE does not.
    if (GR_IS_GR_GL_ES(fStandard) && this->bgraIsInternalFormat() &&
        (kBGRA_8888_GrPixelConfig == dstConfig || kBGRA_8888_GrPixelConfig == srcConfig)) {
        return false;
    }

    // CopyTexSubImage is invalid or doesn't copy what we want when we have msaa render buffers.
    if (dstHasMSAARenderBuffer || srcHasMSAARenderBuffer) {
        return false;
    }

    // CopyTex(Sub)Image writes to a texture and we have no way of dynamically wrapping a RT in a
    // texture.
    if (!dstTypeIfTexture) {
        return false;
    }

    // Check that we could wrap the source in an FBO, that the dst is not TEXTURE_EXTERNAL, that no
    // mirroring is required
    if (this->canConfigBeFBOColorAttachment(srcConfig) &&
        (!srcTypeIfTexture || *srcTypeIfTexture != GrTextureType::kExternal) &&
        *dstTypeIfTexture != GrTextureType::kExternal) {
        return true;
    } else {
        return false;
    }
}

bool GrGLCaps::canCopyAsBlit(GrPixelConfig dstConfig, int dstSampleCnt,
                             const GrTextureType* dstTypeIfTexture,
                             GrPixelConfig srcConfig, int srcSampleCnt,
                             const GrTextureType* srcTypeIfTexture,
                             const SkRect& srcBounds, bool srcBoundsExact,
                             const SkIRect& srcRect, const SkIPoint& dstPoint) const {
    auto blitFramebufferFlags = this->blitFramebufferSupportFlags();
    if (!this->canConfigBeFBOColorAttachment(dstConfig) ||
        !this->canConfigBeFBOColorAttachment(srcConfig)) {
        return false;
    }

    if (dstTypeIfTexture && *dstTypeIfTexture == GrTextureType::kExternal) {
        return false;
    }
    if (srcTypeIfTexture && *srcTypeIfTexture == GrTextureType::kExternal) {
        return false;
    }

    if (GrGLCaps::kNoSupport_BlitFramebufferFlag & blitFramebufferFlags) {
        return false;
    }

    if (GrGLCaps::kResolveMustBeFull_BlitFrambufferFlag & blitFramebufferFlags) {
        if (srcSampleCnt > 1) {
            if (1 == dstSampleCnt) {
                return false;
            }
            if (SkRect::Make(srcRect) != srcBounds || !srcBoundsExact) {
                return false;
            }
        }
    }

    if (GrGLCaps::kNoMSAADst_BlitFramebufferFlag & blitFramebufferFlags) {
        if (dstSampleCnt > 1) {
            return false;
        }
    }

    if (GrGLCaps::kNoFormatConversion_BlitFramebufferFlag & blitFramebufferFlags) {
        if (dstConfig != srcConfig) {
            return false;
        }
    } else if (GrGLCaps::kNoFormatConversionForMSAASrc_BlitFramebufferFlag & blitFramebufferFlags) {
        if (srcSampleCnt > 1 && dstConfig != srcConfig) {
            return false;
        }
    }

    if (GrGLCaps::kRectsMustMatchForMSAASrc_BlitFramebufferFlag & blitFramebufferFlags) {
        if (srcSampleCnt > 1) {
            if (dstPoint.fX != srcRect.fLeft || dstPoint.fY != srcRect.fTop) {
                return false;
            }
        }
    }
    return true;
}

bool GrGLCaps::canCopyAsDraw(GrPixelConfig dstConfig, bool srcIsTextureable) const {
    return this->canConfigBeFBOColorAttachment(dstConfig) && srcIsTextureable;
}

static bool has_msaa_render_buffer(const GrSurfaceProxy* surf, const GrGLCaps& glCaps) {
    const GrRenderTargetProxy* rt = surf->asRenderTargetProxy();
    if (!rt) {
        return false;
    }
    // A RT has a separate MSAA renderbuffer if:
    // 1) It's multisampled
    // 2) We're using an extension with separate MSAA renderbuffers
    // 3) It's not FBO 0, which is special and always auto-resolves
    return rt->numSamples() > 1 &&
           glCaps.usesMSAARenderBuffers() &&
           !rt->rtPriv().glRTFBOIDIs0();
}

bool GrGLCaps::onCanCopySurface(const GrSurfaceProxy* dst, const GrSurfaceProxy* src,
                                const SkIRect& srcRect, const SkIPoint& dstPoint) const {
    GrPixelConfig dstConfig = dst->config();
    GrPixelConfig srcConfig = src->config();

    int dstSampleCnt = 0;
    int srcSampleCnt = 0;
    if (const GrRenderTargetProxy* rtProxy = dst->asRenderTargetProxy()) {
        dstSampleCnt = rtProxy->numSamples();
    }
    if (const GrRenderTargetProxy* rtProxy = src->asRenderTargetProxy()) {
        srcSampleCnt = rtProxy->numSamples();
    }
    SkASSERT((dstSampleCnt > 0) == SkToBool(dst->asRenderTargetProxy()));
    SkASSERT((srcSampleCnt > 0) == SkToBool(src->asRenderTargetProxy()));

    const GrTextureProxy* dstTex = dst->asTextureProxy();
    const GrTextureProxy* srcTex = src->asTextureProxy();

    GrTextureType dstTexType;
    GrTextureType* dstTexTypePtr = nullptr;
    GrTextureType srcTexType;
    GrTextureType* srcTexTypePtr = nullptr;
    if (dstTex) {
        dstTexType = dstTex->textureType();
        dstTexTypePtr = &dstTexType;
    }
    if (srcTex) {
        srcTexType = srcTex->textureType();
        srcTexTypePtr = &srcTexType;
    }

    return this->canCopyTexSubImage(dstConfig, has_msaa_render_buffer(dst, *this), dstTexTypePtr,
                                    srcConfig, has_msaa_render_buffer(src, *this), srcTexTypePtr) ||
           this->canCopyAsBlit(dstConfig, dstSampleCnt, dstTexTypePtr, srcConfig, srcSampleCnt,
                               srcTexTypePtr, src->getBoundsRect(), src->priv().isExact(),
                               srcRect, dstPoint) ||
           this->canCopyAsDraw(dstConfig, SkToBool(srcTex));
}

GrCaps::DstCopyRestrictions GrGLCaps::getDstCopyRestrictions(const GrRenderTargetProxy* src) const {
    // If the src is a texture, we can implement the blit as a draw assuming the config is
    // renderable.
    if (src->asTextureProxy() && !this->isConfigRenderable(src->config())) {
        return {};
    }

    {
        const GrGLenum* srcTextureTarget = src->backendFormat().getGLTarget();
        SkASSERT(srcTextureTarget);
        if (*srcTextureTarget == GR_GL_TEXTURE_EXTERNAL) {
            // Not supported for FBO blit or CopyTexSubImage. Caller will have to fall back to a
            // draw (if the source is also a texture).
            return {};
        }
    }

    // We look for opportunities to use CopyTexSubImage, or fbo blit. If neither are
    // possible and we return false to fallback to creating a render target dst for render-to-
    // texture. This code prefers CopyTexSubImage to fbo blit and avoids triggering temporary fbo
    // creation. It isn't clear that avoiding temporary fbo creation is actually optimal.
    DstCopyRestrictions blitFramebufferRestrictions = {};
    if (src->numSamples() > 1 &&
        (this->blitFramebufferSupportFlags() & kResolveMustBeFull_BlitFrambufferFlag)) {
        blitFramebufferRestrictions.fRectsMustMatch = GrSurfaceProxy::RectsMustMatch::kYes;
        blitFramebufferRestrictions.fMustCopyWholeSrc = true;
        // Mirroring causes rects to mismatch later, don't allow it.
    } else if (src->numSamples() > 1 && (this->blitFramebufferSupportFlags() &
                                         kRectsMustMatchForMSAASrc_BlitFramebufferFlag)) {
        blitFramebufferRestrictions.fRectsMustMatch = GrSurfaceProxy::RectsMustMatch::kYes;
    }

    // Check for format issues with glCopyTexSubImage2D
    if (this->bgraIsInternalFormat() && kBGRA_8888_GrPixelConfig == src->config()) {
        // glCopyTexSubImage2D doesn't work with this config. If the bgra can be used with fbo blit
        // then we set up for that, otherwise fail.
        if (this->canConfigBeFBOColorAttachment(kBGRA_8888_GrPixelConfig)) {
            return blitFramebufferRestrictions;
        }
        // Caller will have to use a draw.
        return {};
    }

    {
        bool srcIsMSAARenderbuffer = src->numSamples() > 1 &&
                                     this->usesMSAARenderBuffers();
        if (srcIsMSAARenderbuffer) {
            // It's illegal to call CopyTexSubImage2D on a MSAA renderbuffer. Set up for FBO
            // blit or fail.
            if (this->canConfigBeFBOColorAttachment(src->config())) {
                return blitFramebufferRestrictions;
            }
            // Caller will have to use a draw.
            return {};
        }
    }

    // We'll do a CopyTexSubImage, no restrictions.
    return {};
}

void GrGLCaps::applyDriverCorrectnessWorkarounds(const GrGLContextInfo& ctxInfo,
                                                 const GrContextOptions& contextOptions,
                                                 GrShaderCaps* shaderCaps,
                                                 FormatWorkarounds* formatWorkarounds) {
    // A driver but on the nexus 6 causes incorrect dst copies when invalidate is called beforehand.
    // Thus we are blacklisting this extension for now on Adreno4xx devices.
    if (kAdreno430_GrGLRenderer == ctxInfo.renderer() ||
        kAdreno4xx_other_GrGLRenderer == ctxInfo.renderer() ||
        fDriverBugWorkarounds.disable_discard_framebuffer) {
        fInvalidateFBType = kNone_InvalidateFBType;
    }

    // glClearTexImage seems to have a bug in NVIDIA drivers that was fixed sometime between
    // 340.96 and 367.57.
    if (GR_IS_GR_GL(ctxInfo.standard()) &&
        ctxInfo.driver() == kNVIDIA_GrGLDriver &&
        ctxInfo.driverVersion() < GR_GL_DRIVER_VER(367, 57, 0)) {
        fClearTextureSupport = false;
    }

#ifdef SK_BUILD_FOR_MAC
    // Radeon MacBooks hit a crash in glReadPixels() when using geometry shaders.
    // http://skbug.com/8097
    if (kATI_GrGLVendor == ctxInfo.vendor()) {
        shaderCaps->fGeometryShaderSupport = false;
    }
    // On at least some MacBooks, GLSL 4.0 geometry shaders break if we use invocations.
    shaderCaps->fGSInvocationsSupport = false;
#endif

    // Qualcomm driver @103.0 has been observed to crash compiling ccpr geometry
    // shaders. @127.0 is the earliest verified driver to not crash.
    if (kQualcomm_GrGLDriver == ctxInfo.driver() &&
        ctxInfo.driverVersion() < GR_GL_DRIVER_VER(127, 0, 0)) {
        shaderCaps->fGeometryShaderSupport = false;
    }

#if defined(__has_feature)
#if defined(SK_BUILD_FOR_MAC) && __has_feature(thread_sanitizer)
    // See skbug.com/7058
    fMapBufferType = kNone_MapBufferType;
    fMapBufferFlags = kNone_MapFlags;
    fTransferBufferSupport = false;
    fTransferBufferType = kNone_TransferBufferType;
#endif
#endif

    // We found that the Galaxy J5 with an Adreno 306 running 6.0.1 has a bug where
    // GL_INVALID_OPERATION thrown by glDrawArrays when using a buffer that was mapped. The same bug
    // did not reproduce on a Nexus7 2013 with a 320 running Android M with driver 127.0. It's
    // unclear whether this really affects a wide range of devices.
    if (ctxInfo.renderer() == kAdreno3xx_GrGLRenderer &&
        ctxInfo.driverVersion() > GR_GL_DRIVER_VER(127, 0, 0)) {
        fMapBufferType = kNone_MapBufferType;
        fMapBufferFlags = kNone_MapFlags;
        fTransferBufferSupport = false;
        fTransferBufferType = kNone_TransferBufferType;
    }

    // TODO: re-enable for ANGLE
    if (kANGLE_GrGLDriver == ctxInfo.driver()) {
        fTransferBufferSupport = false;
        fTransferBufferType = kNone_TransferBufferType;
    }

    // Using MIPs on this GPU seems to be a source of trouble.
    if (kPowerVR54x_GrGLRenderer == ctxInfo.renderer()) {
        fMipMapSupport = false;
    }

#ifndef SK_BUILD_FOR_IOS
    if (kPowerVR54x_GrGLRenderer == ctxInfo.renderer() ||
        kPowerVRRogue_GrGLRenderer == ctxInfo.renderer() ||
        (kAdreno3xx_GrGLRenderer == ctxInfo.renderer() &&
         ctxInfo.driver() != kChromium_GrGLDriver)) {
        fPerformColorClearsAsDraws = true;
    }
#endif

    // A lot of GPUs have trouble with full screen clears (skbug.com/7195)
    if (kAMDRadeonHD7xxx_GrGLRenderer == ctxInfo.renderer() ||
        kAMDRadeonR9M4xx_GrGLRenderer == ctxInfo.renderer()) {
        fPerformColorClearsAsDraws = true;
    }

#ifdef SK_BUILD_FOR_MAC
    // crbug.com/768134 - On MacBook Pros, the Intel Iris Pro doesn't always perform
    // full screen clears
    // crbug.com/773107 - On MacBook Pros, a wide range of Intel GPUs don't always
    // perform full screen clears.
    // Update on 4/4/2018 - This appears to be fixed on driver 10.30.12 on a macOS 10.13.2 on a
    // Retina MBP Early 2015 with Iris 6100. It is possibly fixed on earlier drivers as well.
    if (kIntel_GrGLVendor == ctxInfo.vendor() &&
        ctxInfo.driverVersion() < GR_GL_DRIVER_VER(10, 30, 12)) {
        fPerformColorClearsAsDraws = true;
    }
    // crbug.com/969609 - NVIDIA on Mac sometimes segfaults during glClear in chrome. It seems
    // mostly concentrated in 10.13/14, GT 650Ms, driver 12+. But there are instances of older
    // drivers and GTX 775s, so we'll start with a broader workaround.
    if (kNVIDIA_GrGLVendor == ctxInfo.vendor()) {
        fPerformColorClearsAsDraws = true;
    }
#endif

    // See crbug.com/755871. This could probably be narrowed to just partial clears as the driver
    // bugs seems to involve clearing too much and not skipping the clear.
    // See crbug.com/768134. This is also needed for full clears and was seen on an nVidia K620
    // but only for D3D11 ANGLE.
    if (GrGLANGLEBackend::kD3D11 == ctxInfo.angleBackend()) {
        fPerformColorClearsAsDraws = true;
    }

    if (kAdreno430_GrGLRenderer == ctxInfo.renderer() ||
        kAdreno4xx_other_GrGLRenderer == ctxInfo.renderer()) {
        // This is known to be fixed sometime between driver 145.0 and 219.0
        if (ctxInfo.driverVersion() <= GR_GL_DRIVER_VER(219, 0, 0)) {
            fPerformStencilClearsAsDraws = true;
        }
        fDisallowTexSubImageForUnormConfigTexturesEverBoundToFBO = true;
    }

    if (fDriverBugWorkarounds.gl_clear_broken) {
        fPerformColorClearsAsDraws = true;
        fPerformStencilClearsAsDraws = true;
    }

    // This was reproduced on the following configurations:
    // - A Galaxy J5 (Adreno 306) running Android 6 with driver 140.0
    // - A Nexus 7 2013 (Adreno 320) running Android 5 with driver 104.0
    // - A Nexus 7 2013 (Adreno 320) running Android 6 with driver 127.0
    // - A Nexus 5 (Adreno 330) running Android 6 with driver 127.0
    // and not produced on:
    // - A Nexus 7 2013 (Adreno 320) running Android 4 with driver 53.0
    // The particular lines that get dropped from test images varies across different devices.
    if (kAdreno3xx_GrGLRenderer == ctxInfo.renderer() &&
        ctxInfo.driverVersion() > GR_GL_DRIVER_VER(53, 0, 0)) {
        fRequiresCullFaceEnableDisableWhenDrawingLinesAfterNonLines = true;
    }

    // This was reproduced on a Pixel 1, but the unit test + config + options that exercise it are
    // only tested on very specific bots. The driver claims that ReadPixels is an invalid operation
    // when reading from an auto-resolving MSAA framebuffer that has stencil attached.
    if (kQualcomm_GrGLDriver == ctxInfo.driver()) {
        fDetachStencilFromMSAABuffersBeforeReadPixels = true;
    }

    // TODO: Don't apply this on iOS?
    if (kPowerVRRogue_GrGLRenderer == ctxInfo.renderer()) {
        // Our Chromebook with kPowerVRRogue_GrGLRenderer crashes on large instanced draws. The
        // current minimum number of instances observed to crash is somewhere between 2^14 and 2^15.
        // Keep the number of instances below 1000, just to be safe.
        fMaxInstancesPerDrawWithoutCrashing = 999;
    } else if (fDriverBugWorkarounds.disallow_large_instanced_draw) {
        fMaxInstancesPerDrawWithoutCrashing = 0x4000000;
    }

    // Texture uploads sometimes seem to be ignored to textures bound to FBOS on Tegra3.
    if (kTegra_PreK1_GrGLRenderer == ctxInfo.renderer()) {
        fDisallowTexSubImageForUnormConfigTexturesEverBoundToFBO = true;
        fUseDrawInsteadOfAllRenderTargetWrites = true;
    }

#ifdef SK_BUILD_FOR_MAC
    static constexpr bool isMAC = true;
#else
    static constexpr bool isMAC = false;
#endif

    // We support manual mip-map generation (via iterative downsampling draw calls). This fixes
    // bugs on some cards/drivers that produce incorrect mip-maps for sRGB textures when using
    // glGenerateMipmap. Our implementation requires mip-level sampling control. Additionally,
    // it can be much slower (especially on mobile GPUs), so we opt-in only when necessary:
    if (fMipMapLevelAndLodControlSupport &&
        (contextOptions.fDoManualMipmapping ||
         (kIntel_GrGLVendor == ctxInfo.vendor()) ||
         (kNVIDIA_GrGLDriver == ctxInfo.driver() && isMAC) ||
         (kATI_GrGLVendor == ctxInfo.vendor()))) {
        fDoManualMipmapping = true;
    }

    // See http://crbug.com/710443
#ifdef SK_BUILD_FOR_MAC
    if (kIntelBroadwell_GrGLRenderer == ctxInfo.renderer()) {
        fClearToBoundaryValuesIsBroken = true;
    }
#endif
    if (kQualcomm_GrGLVendor == ctxInfo.vendor()) {
        fDrawArraysBaseVertexIsBroken = true;
    }

    // Currently the extension is advertised but fb fetch is broken on 500 series Adrenos like the
    // Galaxy S7.
    // TODO: Once this is fixed we can update the check here to look at a driver version number too.
    if (kAdreno5xx_GrGLRenderer == ctxInfo.renderer()) {
        shaderCaps->fFBFetchSupport = false;
    }

    // On the NexusS and GalaxyNexus, the use of 'any' causes the compilation error "Calls to any
    // function that may require a gradient calculation inside a conditional block may return
    // undefined results". This appears to be an issue with the 'any' call since even the simple
    // "result=black; if (any()) result=white;" code fails to compile. This issue comes into play
    // from our GrTextureDomain processor.
    shaderCaps->fCanUseAnyFunctionInShader = kImagination_GrGLVendor != ctxInfo.vendor();

    // Known issue on at least some Intel platforms:
    // http://code.google.com/p/skia/issues/detail?id=946
    if (kIntel_GrGLVendor == ctxInfo.vendor()) {
        shaderCaps->fFragCoordConventionsExtensionString = nullptr;
    }

    if (kTegra_PreK1_GrGLRenderer == ctxInfo.renderer()) {
        // The Tegra3 compiler will sometimes never return if we have min(abs(x), 1.0),
        // so we must do the abs first in a separate expression.
        shaderCaps->fCanUseMinAndAbsTogether = false;

        // Tegra3 fract() seems to trigger undefined behavior for negative values, so we
        // must avoid this condition.
        shaderCaps->fCanUseFractForNegativeValues = false;
    }

    // On Intel GPU there is an issue where it reads the second argument to atan "- %s.x" as an int
    // thus must us -1.0 * %s.x to work correctly
    if (kIntel_GrGLVendor == ctxInfo.vendor()) {
        shaderCaps->fMustForceNegatedAtanParamToFloat = true;
    }

    // On some Intel GPUs there is an issue where the driver outputs bogus values in the shader
    // when floor and abs are called on the same line. Thus we must execute an Op between them to
    // make sure the compiler doesn't re-inline them even if we break the calls apart.
    if (kIntel_GrGLVendor == ctxInfo.vendor()) {
        shaderCaps->fMustDoOpBetweenFloorAndAbs = true;
    }

    // On Adreno devices with framebuffer fetch support, there is a bug where they always return
    // the original dst color when reading the outColor even after being written to. By using a
    // local outColor we can work around this bug.
    if (shaderCaps->fFBFetchSupport && kQualcomm_GrGLVendor == ctxInfo.vendor()) {
        shaderCaps->fRequiresLocalOutputColorForFBFetch = true;
    }

    // Newer Mali GPUs do incorrect static analysis in specific situations: If there is uniform
    // color, and that uniform contains an opaque color, and the output of the shader is only based
    // on that uniform plus soemthing un-trackable (like a texture read), the compiler will deduce
    // that the shader always outputs opaque values. In that case, it appears to remove the shader
    // based blending code it normally injects, turning SrcOver into Src. To fix this, we always
    // insert an extra bit of math on the uniform that confuses the compiler just enough...
    if (kMaliT_GrGLRenderer == ctxInfo.renderer()) {
        shaderCaps->fMustObfuscateUniformColor = true;
    }
#ifdef SK_BUILD_FOR_WIN
    // Check for ANGLE on Windows, so we can workaround a bug in D3D itself (anglebug.com/2098).
    //
    // Basically, if a shader has a construct like:
    //
    // float x = someCondition ? someValue : 0;
    // float2 result = (0 == x) ? float2(x, x)
    //                          : float2(2 * x / x, 0);
    //
    // ... the compiler will produce an error 'NaN and infinity literals not allowed', even though
    // we've explicitly guarded the division with a check against zero. This manifests in much
    // more complex ways in some of our shaders, so we use this caps bit to add an epsilon value
    // to the denominator of divisions, even when we've added checks that the denominator isn't 0.
    if (kANGLE_GrGLDriver == ctxInfo.driver() || kChromium_GrGLDriver == ctxInfo.driver()) {
        shaderCaps->fMustGuardDivisionEvenAfterExplicitZeroCheck = true;
    }
#endif

    // We've seen Adreno 3xx devices produce incorrect (flipped) values for gl_FragCoord, in some
    // (rare) situations. It's sporadic, and mostly on older drivers. Additionally, old Adreno
    // compilers (see crbug.com/skia/4078) crash when accessing .zw of gl_FragCoord, so just bypass
    // using gl_FragCoord at all to get around it.
    if (kAdreno3xx_GrGLRenderer == ctxInfo.renderer()) {
        shaderCaps->fCanUseFragCoord = false;
    }

    // gl_FragCoord has an incorrect subpixel offset on legacy Tegra hardware.
    if (kTegra_PreK1_GrGLRenderer == ctxInfo.renderer()) {
        shaderCaps->fCanUseFragCoord = false;
    }

    // On Mali G71, mediump ints don't appear capable of representing every integer beyond +/-2048.
    // (Are they implemented with fp16?)
    if (kARM_GrGLVendor == ctxInfo.vendor()) {
        shaderCaps->fIncompleteShortIntPrecision = true;
    }

    if (fDriverBugWorkarounds.add_and_true_to_loop_condition) {
        shaderCaps->fAddAndTrueToLoopCondition = true;
    }

    if (fDriverBugWorkarounds.unfold_short_circuit_as_ternary_operation) {
        shaderCaps->fUnfoldShortCircuitAsTernary = true;
    }

    if (fDriverBugWorkarounds.emulate_abs_int_function) {
        shaderCaps->fEmulateAbsIntFunction = true;
    }

    if (fDriverBugWorkarounds.rewrite_do_while_loops) {
        shaderCaps->fRewriteDoWhileLoops = true;
    }

    if (fDriverBugWorkarounds.remove_pow_with_constant_exponent) {
        shaderCaps->fRemovePowWithConstantExponent = true;
    }

    if (kAdreno3xx_GrGLRenderer == ctxInfo.renderer() ||
        kAdreno4xx_other_GrGLRenderer == ctxInfo.renderer()) {
        shaderCaps->fMustWriteToFragColor = true;
    }

    // Disabling advanced blend on various platforms with major known issues. We also block Chrome
    // for now until its own blacklists can be updated.
    if (kAdreno430_GrGLRenderer == ctxInfo.renderer() ||
        kAdreno4xx_other_GrGLRenderer == ctxInfo.renderer() ||
        kAdreno5xx_GrGLRenderer == ctxInfo.renderer() ||
        kIntel_GrGLDriver == ctxInfo.driver() ||
        kChromium_GrGLDriver == ctxInfo.driver()) {
        fBlendEquationSupport = kBasic_BlendEquationSupport;
        shaderCaps->fAdvBlendEqInteraction = GrShaderCaps::kNotSupported_AdvBlendEqInteraction;
    }

    // Non-coherent advanced blend has an issue on NVIDIA pre 337.00.
    if (kNVIDIA_GrGLDriver == ctxInfo.driver() &&
        ctxInfo.driverVersion() < GR_GL_DRIVER_VER(337, 00, 0) &&
        kAdvanced_BlendEquationSupport == fBlendEquationSupport) {
        fBlendEquationSupport = kBasic_BlendEquationSupport;
        shaderCaps->fAdvBlendEqInteraction = GrShaderCaps::kNotSupported_AdvBlendEqInteraction;
    }

    if (fDriverBugWorkarounds.disable_blend_equation_advanced) {
        fBlendEquationSupport = kBasic_BlendEquationSupport;
        shaderCaps->fAdvBlendEqInteraction = GrShaderCaps::kNotSupported_AdvBlendEqInteraction;
    }

    if (this->advancedBlendEquationSupport()) {
        if (kNVIDIA_GrGLDriver == ctxInfo.driver() &&
            ctxInfo.driverVersion() < GR_GL_DRIVER_VER(355, 00, 0)) {
            // Blacklist color-dodge and color-burn on pre-355.00 NVIDIA.
            fAdvBlendEqBlacklist |= (1 << kColorDodge_GrBlendEquation) |
                                    (1 << kColorBurn_GrBlendEquation);
        }
        if (kARM_GrGLVendor == ctxInfo.vendor()) {
            // Blacklist color-burn on ARM until the fix is released.
            fAdvBlendEqBlacklist |= (1 << kColorBurn_GrBlendEquation);
        }
    }

    // Workaround NVIDIA bug related to glInvalidateFramebuffer and mixed samples.
    if (fMultisampleDisableSupport &&
        this->shaderCaps()->dualSourceBlendingSupport() &&
        this->shaderCaps()->pathRenderingSupport() &&
        fMixedSamplesSupport &&
#if GR_TEST_UTILS
        (contextOptions.fGpuPathRenderers & GpuPathRenderers::kStencilAndCover) &&
#endif
        (kNVIDIA_GrGLDriver == ctxInfo.driver() ||
         kChromium_GrGLDriver == ctxInfo.driver())) {
            fInvalidateFBType = kNone_InvalidateFBType;
    }

    // Many ES3 drivers only advertise the ES2 image_external extension, but support the _essl3
    // extension, and require that it be enabled to work with ESSL3. Other devices require the ES2
    // extension to be enabled, even when using ESSL3. Enabling both extensions fixes both cases.
    // skbug.com/7713
    if (ctxInfo.hasExtension("GL_OES_EGL_image_external") &&
        ctxInfo.glslGeneration() >= k330_GrGLSLGeneration &&
        !shaderCaps->fExternalTextureSupport) { // i.e. Missing the _essl3 extension
        shaderCaps->fExternalTextureSupport = true;
        shaderCaps->fExternalTextureExtensionString = "GL_OES_EGL_image_external";
        shaderCaps->fSecondExternalTextureExtensionString = "GL_OES_EGL_image_external_essl3";
    }

#ifdef SK_BUILD_FOR_IOS
    // iOS drivers appear to implement TexSubImage by creating a staging buffer, and copying
    // UNPACK_ROW_LENGTH * height bytes. That's unsafe in several scenarios, and the simplest fix
    // is to just blacklist the feature.
    // https://github.com/flutter/flutter/issues/16718
    // https://bugreport.apple.com/web/?problemID=39948888
    fWritePixelsRowBytesSupport = false;
#endif

    // CCPR edge AA is busted on Mesa, Sandy Bridge/Valley View (Bay Trail).
    // http://skbug.com/8162
    if (kMesa_GrGLDriver == ctxInfo.driver() &&
        (kIntelSandyBridge_GrGLRenderer == ctxInfo.renderer() ||
         kIntelIvyBridge_GrGLRenderer == ctxInfo.renderer() ||
         kIntelValleyView_GrGLRenderer == ctxInfo.renderer())) {
        fDriverBlacklistCCPR = true;
    }

    // Temporarily disable the MSAA implementation of CCPR on various platforms while we work out
    // specific issues.
    if (kATI_GrGLVendor == ctxInfo.vendor() ||  // Radeon drops stencil draws that use sample mask.
        kQualcomm_GrGLVendor == ctxInfo.vendor() /* Pixel2 crashes in nanobench. */) {
        fDriverBlacklistMSAACCPR = true;
    }

#ifdef SK_BUILD_FOR_ANDROID
    // Older versions of Android have problems with setting GL_TEXTURE_BASE_LEVEL or
    // GL_TEXTURE_MAX_LEVEL on GL_TEXTURE_EXTERTNAL_OES textures. We just leave them as is and hope
    // the client never changes them either.
    fDontSetBaseOrMaxLevelForExternalTextures = true;
#endif

    // PowerVRGX6250 drops every pixel if we modify the sample mask while color writes are disabled.
    if (kPowerVRRogue_GrGLRenderer == ctxInfo.renderer()) {
        fNeverDisableColorWrites = true;
        shaderCaps->fMustWriteToFragColor = true;
    }

    // It appears that Qualcomm drivers don't actually support
    // GL_NV_shader_noperspective_interpolation in ES 3.00 or 3.10 shaders, only 3.20.
    // https://crbug.com/986581
    if (kQualcomm_GrGLVendor == ctxInfo.vendor() &&
        k320es_GrGLSLGeneration != ctxInfo.glslGeneration()) {
        shaderCaps->fNoPerspectiveInterpolationSupport = false;
    }

    // We disable srgb write control for Adreno4xx devices.
    // see: https://bug.skia.org/5329
    if (kAdreno430_GrGLRenderer == ctxInfo.renderer() ||
        kAdreno4xx_other_GrGLRenderer == ctxInfo.renderer()) {
        fSRGBWriteControl = false;
    }

    // ARB_texture_rg is part of OpenGL 3.0, but osmesa doesn't support GL_RED
    // and GL_RG on FBO textures.
    formatWorkarounds->fDisableTextureRedForMesa = kOSMesa_GrGLRenderer == ctxInfo.renderer();

    // MacPro devices with AMD cards fail to create MSAA sRGB render buffers.
#if defined(SK_BUILD_FOR_MAC)
    formatWorkarounds->fDisableSRGBRenderWithMSAAForMacAMD = kATI_GrGLVendor == ctxInfo.vendor();
#endif

    // ES2 Command Buffer has several TexStorage restrictions. It appears to fail for any format
    // not explicitly allowed by the original GL_EXT_texture_storage, particularly those from
    // other extensions even when later revisions define the interactions.
    formatWorkarounds->fDisablePerFormatTextureStorageForCommandBufferES2 =
            kChromium_GrGLDriver == ctxInfo.driver() && ctxInfo.version() < GR_GL_VER(3, 0);

    // Angle with es2->GL has a bug where it will hang trying to call TexSubImage on GL_R8
    // formats on miplevels > 0. We already disable texturing on gles > 2.0 so just need to
    // check that we are not going to OpenGL.
    formatWorkarounds->fDisableNonRedSingleChannelTexStorageForANGLEGL =
            GrGLANGLEBackend::kOpenGL == ctxInfo.angleBackend();

#if defined(SK_BUILD_FOR_WIN)
    // On Intel Windows ES contexts it seems that using texture storage with BGRA causes
    // problems with cross-context SkImages.
    formatWorkarounds->fDisableBGRATextureStorageForIntelWindowsES =
            kIntel_GrGLDriver == ctxInfo.driver() && GR_IS_GR_GL_ES(ctxInfo.standard());
#endif

    // Mali-400 fails ReadPixels tests, mostly with non-0xFF alpha values when read as GL_RGBA8.
    formatWorkarounds->fDisableRGB8ForMali400 = kMali4xx_GrGLRenderer == ctxInfo.renderer();

    // On the Intel Iris 6100, interacting with LUM16F seems to confuse the driver. After
    // writing to/reading from a LUM16F texture reads from/writes to other formats behave
    // erratically.
    formatWorkarounds->fDisableLuminance16F = kIntelBroadwell_GrGLRenderer == ctxInfo.renderer();
}

void GrGLCaps::onApplyOptionsOverrides(const GrContextOptions& options) {
    if (options.fDisableDriverCorrectnessWorkarounds) {
        SkASSERT(!fDoManualMipmapping);
        SkASSERT(!fClearToBoundaryValuesIsBroken);
        SkASSERT(0 == fMaxInstancesPerDrawWithoutCrashing);
        SkASSERT(!fDrawArraysBaseVertexIsBroken);
        SkASSERT(!fDisallowTexSubImageForUnormConfigTexturesEverBoundToFBO);
        SkASSERT(!fUseDrawInsteadOfAllRenderTargetWrites);
        SkASSERT(!fRequiresCullFaceEnableDisableWhenDrawingLinesAfterNonLines);
        SkASSERT(!fDetachStencilFromMSAABuffersBeforeReadPixels);
        SkASSERT(!fDontSetBaseOrMaxLevelForExternalTextures);
        SkASSERT(!fNeverDisableColorWrites);
    }
    if (options.fDoManualMipmapping) {
        fDoManualMipmapping = true;
    }
    if (options.fDisallowGLSLBinaryCaching) {
        fProgramBinarySupport = false;
    }
#if GR_TEST_UTILS
    if (options.fCacheSKSL) {
        fProgramBinarySupport = false;
    }
#endif
}

bool GrGLCaps::onSurfaceSupportsWritePixels(const GrSurface* surface) const {
    if (fDisallowTexSubImageForUnormConfigTexturesEverBoundToFBO) {
        if (auto tex = static_cast<const GrGLTexture*>(surface->asTexture())) {
            if (tex->hasBaseLevelBeenBoundToFBO()) {
                return false;
            }
        }
    }    if (auto rt = surface->asRenderTarget()) {
        if (fUseDrawInsteadOfAllRenderTargetWrites) {
            return false;
        }
        if (rt->numSamples() > 1 && this->usesMSAARenderBuffers()) {
            return false;
        }
        return SkToBool(surface->asTexture());
    }
    return true;
}

GrCaps::SurfaceReadPixelsSupport GrGLCaps::surfaceSupportsReadPixels(
        const GrSurface* surface) const {
    if (auto tex = static_cast<const GrGLTexture*>(surface->asTexture())) {
        // We don't support reading pixels directly from EXTERNAL textures as it would require
        // binding the texture to a FBO.
        if (tex->target() == GR_GL_TEXTURE_EXTERNAL) {
            return SurfaceReadPixelsSupport::kCopyToTexture2D;
        }
    }
    return SurfaceReadPixelsSupport::kSupported;
}

GrCaps::SupportedRead GrGLCaps::supportedReadPixelsColorType(
        GrColorType srcColorType, const GrBackendFormat& srcBackendFormat,
        GrColorType dstColorType) const {
    // For now, we mostly report the read back format that is required by the ES spec without
    // checking for implementation allowed formats or consider laxer rules in non-ES GL. TODO: Relax
    // this as makes sense to increase performance and correctness.
    const GrGLenum* glFormat = srcBackendFormat.getGLFormat();
    if (!glFormat) {
        return {GrSwizzle{}, GrColorType::kUnknown};
    }
    GrGLFormat srcFormat = GrGLBackendFormatToGLFormat(srcBackendFormat);

    auto swizzle = this->getFormatInfo(srcFormat).rgbaReadSwizzle(srcColorType);
    switch (this->getFormatInfo(srcFormat).fFormatType) {
        case FormatType::kUnknown:
            return {GrSwizzle{}, GrColorType::kUnknown};
        case FormatType::kNormalizedFixedPoint:
            if (srcColorType == GrColorType::kRGB_888x && srcFormat == GrGLFormat::kRGBA8 &&
                GrColorTypeHasAlpha(dstColorType)) {
                // This can skip an unnecessary conversion.
                return {swizzle, GrColorType::kRGB_888x};
            } else if (srcFormat == GrGLFormat::kSRGB8_ALPHA8) {
                return {swizzle, GrColorType::kRGBA_8888_SRGB};
            }
            return {swizzle, GrColorType::kRGBA_8888};
        case FormatType::kFloat:
            return {swizzle, GrColorType::kRGBA_F32};
    }
    SkUNREACHABLE;
    return {GrSwizzle{}, GrColorType::kUnknown};
}

bool GrGLCaps::onIsWindowRectanglesSupportedForRT(const GrBackendRenderTarget& backendRT) const {
    GrGLFramebufferInfo fbInfo;
    SkAssertResult(backendRT.getGLFramebufferInfo(&fbInfo));
    // Window Rectangles are not supported for FBO 0;
    return fbInfo.fFBOID != 0;
}

bool GrGLCaps::isFormatSRGB(const GrBackendFormat& format) const {
    return GrGLBackendFormatToGLFormat(format) == GrGLFormat::kSRGB8_ALPHA8;
}

bool GrGLCaps::isFormatTexturable(GrColorType ct, GrGLFormat format) const {
    const FormatInfo& info = this->getFormatInfo(format);
    // Currently we conflate texturable to mean the format itself is texturable in a draw and that
    // we are able to upload data of the passed in colortype to it.
    return SkToBool(info.fFlags & FormatInfo::kTextureable_Flag) &&
           SkToBool(info.colorTypeFlags(ct) & ColorTypeInfo::kUploadData_Flag);
}

bool GrGLCaps::isFormatTexturable(GrColorType ct, const GrBackendFormat& format) const {
    return this->isFormatTexturable(ct, GrGLBackendFormatToGLFormat(format));
}

int GrGLCaps::getRenderTargetSampleCount(int requestedCount, GrColorType grCT,
                                         GrGLFormat format) const {
    const FormatInfo& info = this->getFormatInfo(format);
    if (!SkToBool(info.colorTypeFlags(grCT) & ColorTypeInfo::kRenderable_Flag)) {
        return 0;
    }

    int count = info.fColorSampleCounts.count();
    if (!count) {
        return 0;
    }

    requestedCount = SkTMax(1, requestedCount);
    if (1 == requestedCount) {
        return info.fColorSampleCounts[0] == 1 ? 1 : 0;
    }

    for (int i = 0; i < count; ++i) {
        if (info.fColorSampleCounts[i] >= requestedCount) {
            int count = info.fColorSampleCounts[i];
            if (fDriverBugWorkarounds.max_msaa_sample_count_4) {
                count = SkTMin(count, 4);
            }
            return count;
        }
    }
    return 0;

}

int GrGLCaps::maxRenderTargetSampleCount(GrColorType grCT, GrGLFormat format) const {
    const FormatInfo& info = this->getFormatInfo(format);
    if (!SkToBool(info.colorTypeFlags(grCT) & ColorTypeInfo::kRenderable_Flag)) {
        return 0;
    }
    const auto& table = info.fColorSampleCounts;
    if (!table.count()) {
        return 0;
    }
    int count = table[table.count() - 1];
    if (fDriverBugWorkarounds.max_msaa_sample_count_4) {
        count = SkTMin(count, 4);
    }
    return count;
}

bool GrGLCaps::canFormatBeFBOColorAttachment(GrGLFormat format) const {
    return SkToBool(this->getFormatInfo(format).fFlags & FormatInfo::kFBOColorAttachment_Flag);
}

bool GrGLCaps::isFormatCopyable(GrColorType ct, const GrBackendFormat& format) const {
    // In GL we have three ways to be able to copy. CopyTexImage, blit, and draw. CopyTexImage
    // requires the src to be an FBO attachment, blit requires both src and dst to be FBO
    // attachments, and draw requires the dst to be an FBO attachment. Thus to copy from and to
    // the same config, we need that config to be bindable to an FBO.
    return this->canFormatBeFBOColorAttachment(GrGLBackendFormatToGLFormat(format));
}

bool GrGLCaps::formatSupportsTexStorage(GrGLFormat format) const {
    return SkToBool(this->getFormatInfo(format).fFlags & FormatInfo::kCanUseTexStorage_Flag);
}

GrGLFormat GrGLCaps::pixelConfigToFormat(GrPixelConfig config) const {
    switch (config) {
        case kRGB_888X_GrPixelConfig:
            return GrGLFormat::kRGBA8;
        case kAlpha_8_as_Alpha_GrPixelConfig:
            return GrGLFormat::kALPHA8;
        case kAlpha_8_as_Red_GrPixelConfig:
            return GrGLFormat::kR8;
        case kGray_8_as_Lum_GrPixelConfig:
            return GrGLFormat::kLUMINANCE8;
        case kGray_8_as_Red_GrPixelConfig:
            return GrGLFormat::kR8;
        case kAlpha_half_as_Lum_GrPixelConfig:
            return GrGLFormat::kLUMINANCE16F;
        case kAlpha_half_as_Red_GrPixelConfig:
            return GrGLFormat::kR16F;
        case kRGB_ETC1_GrPixelConfig: {
            auto info = this->getFormatInfo(GrGLFormat::kCOMPRESSED_ETC1_RGB8);
            bool usesETC1 = SkToBool(info.fFlags & FormatInfo::kTextureable_Flag);
            return usesETC1 ? GrGLFormat::kCOMPRESSED_ETC1_RGB8
                            : GrGLFormat::kCOMPRESSED_RGB8_ETC2;
        }
        case kUnknown_GrPixelConfig:
        case kAlpha_8_GrPixelConfig:
        case kGray_8_GrPixelConfig:
        case kRGB_565_GrPixelConfig:
        case kRGBA_4444_GrPixelConfig:
        case kRGBA_8888_GrPixelConfig:
        case kRGB_888_GrPixelConfig:
        case kRG_88_GrPixelConfig:
        case kBGRA_8888_GrPixelConfig:
        case kSRGBA_8888_GrPixelConfig:
        case kRGBA_1010102_GrPixelConfig:
        case kRGBA_float_GrPixelConfig:
        case kAlpha_half_GrPixelConfig:
        case kRGBA_half_GrPixelConfig:
        case kRGBA_half_Clamped_GrPixelConfig:
        case kR_16_GrPixelConfig:
        case kRG_1616_GrPixelConfig:
        case kRGBA_16161616_GrPixelConfig:
        case kRG_half_GrPixelConfig:
            return this->getFormatFromColorType(GrPixelConfigToColorType(config));
    }
    SkUNREACHABLE;
    return GrGLFormat::kUnknown;
}

// A near clone of format_color_type_valid_pair
static GrPixelConfig validate_sized_format(GrGLenum format, GrColorType ct, GrGLStandard standard) {
    switch (ct) {
        case GrColorType::kUnknown:
            return kUnknown_GrPixelConfig;
        case GrColorType::kAlpha_8:
            if (GR_GL_ALPHA8 == format) {
                return kAlpha_8_as_Alpha_GrPixelConfig;
            } else if (GR_GL_R8 == format) {
                return kAlpha_8_as_Red_GrPixelConfig;
            }
            break;
        case GrColorType::kBGR_565:
            if (GR_GL_RGB565 == format) {
                return kRGB_565_GrPixelConfig;
            }
            break;
        case GrColorType::kABGR_4444:
            if (GR_GL_RGBA4 == format) {
                return kRGBA_4444_GrPixelConfig;
            }
            break;
        case GrColorType::kRGBA_8888:
            if (GR_GL_RGBA8 == format) {
                return kRGBA_8888_GrPixelConfig;
            }
            break;
        case GrColorType::kRGBA_8888_SRGB:
            if (GR_GL_SRGB8_ALPHA8 == format) {
                return kSRGBA_8888_GrPixelConfig;
            }
            break;
        case GrColorType::kRGB_888x:
            if (GR_GL_RGB8 == format) {
                return kRGB_888_GrPixelConfig;
            } else if (GR_GL_RGBA8 == format) {
                return kRGB_888X_GrPixelConfig;
            }
            break;
        case GrColorType::kRG_88:
            if (GR_GL_RG8 == format) {
                return kRG_88_GrPixelConfig;
            }
            break;
        case GrColorType::kBGRA_8888:
            if (GR_GL_RGBA8 == format) {
                if (GR_IS_GR_GL(standard)) {
                    return kBGRA_8888_GrPixelConfig;
                }
            } else if (GR_GL_BGRA8 == format) {
                if (GR_IS_GR_GL_ES(standard) || GR_IS_GR_WEBGL(standard)) {
                    return kBGRA_8888_GrPixelConfig;
                }
            }
            break;
        case GrColorType::kRGBA_1010102:
            if (GR_GL_RGB10_A2 == format) {
                return kRGBA_1010102_GrPixelConfig;
            }
            break;
        case GrColorType::kGray_8:
            if (GR_GL_LUMINANCE8 == format) {
                return kGray_8_as_Lum_GrPixelConfig;
            } else if (GR_GL_R8 == format) {
                return kGray_8_as_Red_GrPixelConfig;
            }
            break;
        case GrColorType::kAlpha_F16:
            if (GR_GL_LUMINANCE16F == format) {
                return kAlpha_half_as_Lum_GrPixelConfig;
            } else if (GR_GL_R16F == format) {
                return kAlpha_half_as_Red_GrPixelConfig;
            }
            break;
        case GrColorType::kRGBA_F16:
            if (GR_GL_RGBA16F == format) {
                return kRGBA_half_GrPixelConfig;
            }
            break;
        case GrColorType::kRGBA_F16_Clamped:
            if (GR_GL_RGBA16F == format) {
                return kRGBA_half_Clamped_GrPixelConfig;
            }
            break;
        case GrColorType::kRGBA_F32:
            if (GR_GL_RGBA32F == format) {
                return kRGBA_float_GrPixelConfig;
            }
            break;
        case GrColorType::kR_16:
            if (GR_GL_R16 == format) {
                return kR_16_GrPixelConfig;
            }
            break;
        case GrColorType::kRG_1616:
            if (GR_GL_RG16 == format) {
                return kRG_1616_GrPixelConfig;
            }
            break;
        case GrColorType::kRGBA_16161616:
            if (GR_GL_RGBA16 == format) {
                return kRGBA_16161616_GrPixelConfig;
            }
            break;
        case GrColorType::kRG_F16:
            if (GR_GL_RG16F == format) {
                return kRG_half_GrPixelConfig;
            }
            break;
    }

    SkDebugf("Unknown pixel config 0x%x\n", format);
    return kUnknown_GrPixelConfig;
}

GrPixelConfig GrGLCaps::validateBackendRenderTarget(const GrBackendRenderTarget& rt,
                                                    GrColorType ct) const {
    GrGLFramebufferInfo fbInfo;
    if (!rt.getGLFramebufferInfo(&fbInfo)) {
        return kUnknown_GrPixelConfig;
    }
    return validate_sized_format(fbInfo.fFormat, ct, fStandard);
}

bool GrGLCaps::onAreColorTypeAndFormatCompatible(GrColorType ct,
                                                 const GrBackendFormat& format) const {
    const GrGLenum* glFormat = format.getGLFormat();
    if (!glFormat) {
        return false;
    }

    return kUnknown_GrPixelConfig != validate_sized_format(*glFormat, ct, fStandard);
}

GrPixelConfig GrGLCaps::onGetConfigFromBackendFormat(const GrBackendFormat& format,
                                                     GrColorType ct) const {
    const GrGLenum* glFormat = format.getGLFormat();
    if (!glFormat) {
        return kUnknown_GrPixelConfig;
    }
    return validate_sized_format(*glFormat, ct, fStandard);
}

static GrPixelConfig get_yuva_config(GrGLenum format) {
    GrPixelConfig config = kUnknown_GrPixelConfig;

    switch (format) {
        case GR_GL_LUMINANCE8:
            config = kGray_8_as_Lum_GrPixelConfig;
            break;
        case GR_GL_ALPHA8:
            config = kAlpha_8_as_Alpha_GrPixelConfig;
            break;
        case GR_GL_R8:
            config = kGray_8_as_Red_GrPixelConfig;
            break;
        case GR_GL_RG8:
            config = kRG_88_GrPixelConfig;
            break;
        case GR_GL_RGBA8:
            config = kRGBA_8888_GrPixelConfig;
            break;
        case GR_GL_RGB8:
            config = kRGB_888_GrPixelConfig;
            break;
        case GR_GL_BGRA8:
            config = kBGRA_8888_GrPixelConfig;
            break;
        case GR_GL_RGB10_A2:
            config = kRGBA_1010102_GrPixelConfig;
            break;
        case GR_GL_LUMINANCE16F:
            config = kAlpha_half_as_Lum_GrPixelConfig;
            break;
        case GR_GL_R16F:
            config = kAlpha_half_as_Red_GrPixelConfig;
            break;
        case GR_GL_R16:
            config = kR_16_GrPixelConfig;
            break;
        case GR_GL_RG16:
            config = kRG_1616_GrPixelConfig;
            break;
        // Experimental (for Y416 and mutant P016/P010)
        case GR_GL_RGBA16:
            config = kRGBA_16161616_GrPixelConfig;
            break;
        case GR_GL_RG16F:
            config = kRG_half_GrPixelConfig;
            break;
    }

    return config;
}

GrPixelConfig GrGLCaps::getYUVAConfigFromBackendFormat(const GrBackendFormat& format) const {
    const GrGLenum* glFormat = format.getGLFormat();
    if (!glFormat) {
        return kUnknown_GrPixelConfig;
    }
    return get_yuva_config(*glFormat);
}

GrColorType GrGLCaps::getYUVAColorTypeFromBackendFormat(const GrBackendFormat& format) const {
    GrGLFormat grGLFormat = GrGLBackendFormatToGLFormat(format);

    switch (grGLFormat) {
        case GrGLFormat::kLUMINANCE8:   // fall through
        case GrGLFormat::kR8:           return GrColorType::kGray_8;
        case GrGLFormat::kALPHA8:       return GrColorType::kAlpha_8;
        case GrGLFormat::kRG8:          return GrColorType::kRG_88;
        case GrGLFormat::kRGBA8:        return GrColorType::kRGBA_8888;
        case GrGLFormat::kRGB8:         return GrColorType::kRGB_888x;
        case GrGLFormat::kBGRA8:        return GrColorType::kBGRA_8888;
        case GrGLFormat::kRGB10_A2:     return GrColorType::kRGBA_1010102;
        case GrGLFormat::kLUMINANCE16F: // fall through
        case GrGLFormat::kR16F:         return GrColorType::kAlpha_F16;
        case GrGLFormat::kR16:          return GrColorType::kR_16;
        case GrGLFormat::kRG16:         return GrColorType::kRG_1616;
        // Experimental (for Y416 and mutant P016/P010)
        case GrGLFormat::kRGBA16:       return GrColorType::kRGBA_16161616;
        case GrGLFormat::kRG16F:        return GrColorType::kRG_F16;
        default:                        return GrColorType::kUnknown;
    }

    SkUNREACHABLE;
}

GrBackendFormat GrGLCaps::getBackendFormatFromColorType(GrColorType ct) const {
    auto format = this->getFormatFromColorType(ct);
    if (format == GrGLFormat::kUnknown) {
        return GrBackendFormat();
    }
    return GrBackendFormat::MakeGL(this->getSizedInternalFormat(format), GR_GL_TEXTURE_2D);
}

GrBackendFormat GrGLCaps::getBackendFormatFromCompressionType(
        SkImage::CompressionType compressionType) const {
    switch (compressionType) {
        case SkImage::kETC1_CompressionType:
            return GrBackendFormat::MakeGL(GR_GL_COMPRESSED_ETC1_RGB8, GR_GL_TEXTURE_2D);
    }
    SK_ABORT("Invalid compression type");
    return {};
}

bool GrGLCaps::canClearTextureOnCreation() const { return fClearTextureSupport; }

#ifdef SK_DEBUG
static bool format_color_type_valid_pair(GrGLenum format, GrColorType colorType) {
    switch (colorType) {
        case GrColorType::kUnknown:
            return false;
        case GrColorType::kAlpha_8:
            return GR_GL_ALPHA8 == format || GR_GL_R8 == format;
        case GrColorType::kBGR_565:
            return GR_GL_RGB565 == format;
        case GrColorType::kABGR_4444:
            return GR_GL_RGBA4 == format;
        case GrColorType::kRGBA_8888:
            return GR_GL_RGBA8 == format;
        case GrColorType::kRGBA_8888_SRGB:
            return GR_GL_SRGB8_ALPHA8 == format;
        case GrColorType::kRGB_888x:
            GR_STATIC_ASSERT(GrCompressionTypeClosestColorType(SkImage::kETC1_CompressionType) ==
                             GrColorType::kRGB_888x);
            return GR_GL_RGB8 == format || GR_GL_RGBA8 == format ||
                   GR_GL_COMPRESSED_RGB8_ETC2 == format || GR_GL_COMPRESSED_ETC1_RGB8 == format;
        case GrColorType::kRG_88:
            return GR_GL_RG8 == format;
        case GrColorType::kBGRA_8888:
            return GR_GL_RGBA8 == format || GR_GL_BGRA8 == format || GR_GL_SRGB8_ALPHA8 == format;
        case GrColorType::kRGBA_1010102:
            return GR_GL_RGB10_A2 == format;
        case GrColorType::kGray_8:
            return GR_GL_LUMINANCE8 == format || GR_GL_R8 == format;
        case GrColorType::kAlpha_F16:
            return GR_GL_R16F == format || GR_GL_LUMINANCE16F == format;
        case GrColorType::kRGBA_F16:
            return GR_GL_RGBA16F == format;
        case GrColorType::kRGBA_F16_Clamped:
            return GR_GL_RGBA16F == format;
        case GrColorType::kRGBA_F32:
            return GR_GL_RGBA32F == format;
        case GrColorType::kR_16:
            return GR_GL_R16 == format;
        case GrColorType::kRG_1616:
            return GR_GL_RG16 == format;
        // Experimental (for Y416 and mutant P016/P010)
        case GrColorType::kRGBA_16161616:
            return GR_GL_RGBA16 == format;
        case GrColorType::kRG_F16:
            return GR_GL_RG16F == format;

    }
    SK_ABORT("Unknown color type");
    return false;
}
#endif

static GrSwizzle get_swizzle(const GrBackendFormat& format, GrColorType colorType,
                             bool forOutput) {
    SkASSERT(format.getGLFormat());
    GrGLenum glFormat = *format.getGLFormat();

    SkASSERT(format_color_type_valid_pair(glFormat, colorType));

    // When picking a swizzle for output, we will pick to use RGBA if possible so that it creates
    // less variations of shaders and those shaders can be used by different configs.
    switch (colorType) {
        case GrColorType::kAlpha_8:
            if (glFormat == GR_GL_ALPHA8) {
                if (!forOutput) {
                    return GrSwizzle::AAAA();
                }
            } else {
                SkASSERT(glFormat == GR_GL_R8);
                if (forOutput) {
                    return GrSwizzle::AAAA();
                } else {
                    return GrSwizzle::RRRR();
                }
            }
            break;
        case GrColorType::kAlpha_F16:
            SkASSERT(glFormat == GR_GL_R16F || glFormat == GR_GL_LUMINANCE16F);
            if (forOutput) {
                return GrSwizzle::AAAA();
            } else {
                return GrSwizzle::RRRR();
            }
            break;
        case GrColorType::kGray_8:
            if (glFormat == GR_GL_R8) {
                if (!forOutput) {
                    return GrSwizzle::RRRA();
                }
            }
            break;
        case GrColorType::kRGB_888x:
            if (!forOutput) {
                return GrSwizzle::RGB1();
            }
            break;
        default:
            return GrSwizzle::RGBA();
    }

    return GrSwizzle::RGBA();
}

GrSwizzle GrGLCaps::getTextureSwizzle(const GrBackendFormat& format, GrColorType colorType) const {
    return get_swizzle(format, colorType, false);
}
GrSwizzle GrGLCaps::getOutputSwizzle(const GrBackendFormat& format, GrColorType colorType) const {
    return get_swizzle(format, colorType, true);
}

size_t GrGLCaps::onTransferFromOffsetAlignment(GrColorType bufferColorType) const {
    // This implementation is highly coupled with decisions made in initConfigTable and GrGLGpu's
    // read pixels implementation and may not be correct for all types. TODO(bsalomon): This will be
    // cleaned up/unified as part of removing GrPixelConfig.
    // There are known problems with 24 vs 32 bit BPP with this color type. Just fail for now.
    if (GrColorType::kRGB_888x == bufferColorType) {
        return 0;
    }
    auto config = GrColorTypeToPixelConfig(bufferColorType);
    // This switch is derived from a table titled "Pixel data type parameter values and the
    // corresponding GL data types" in the OpenGL spec (Table 8.2 in OpenGL 4.5).
    switch (fConfigTable[config].fFormats.fExternalType) {
        case GR_GL_UNSIGNED_BYTE:                   return sizeof(GrGLubyte);
        case GR_GL_BYTE:                            return sizeof(GrGLbyte);
        case GR_GL_UNSIGNED_SHORT:                  return sizeof(GrGLushort);
        case GR_GL_SHORT:                           return sizeof(GrGLshort);
        case GR_GL_UNSIGNED_INT:                    return sizeof(GrGLuint);
        case GR_GL_INT:                             return sizeof(GrGLint);
        case GR_GL_HALF_FLOAT:                      return sizeof(GrGLhalf);
        case GR_GL_FLOAT:                           return sizeof(GrGLfloat);
        case GR_GL_UNSIGNED_SHORT_5_6_5:            return sizeof(GrGLushort);
        case GR_GL_UNSIGNED_SHORT_4_4_4_4:          return sizeof(GrGLushort);
        case GR_GL_UNSIGNED_SHORT_5_5_5_1:          return sizeof(GrGLushort);
        case GR_GL_UNSIGNED_INT_2_10_10_10_REV:     return sizeof(GrGLuint);
#if 0  // GL types we currently don't use. Here for future reference.
        case GR_GL_UNSIGNED_BYTE_3_3_2:             return sizeof(GrGLubyte);
        case GR_GL_UNSIGNED_BYTE_2_3_3_REV:         return sizeof(GrGLubyte);
        case GR_GL_UNSIGNED_SHORT_5_6_5_REV:        return sizeof(GrGLushort);
        case GR_GL_UNSIGNED_SHORT_4_4_4_4_REV:      return sizeof(GrGLushort);
        case GR_GL_UNSIGNED_SHORT_1_5_5_5_REV:      return sizeof(GrGLushort);
        case GR_GL_UNSIGNED_INT_8_8_8_8:            return sizeof(GrGLuint);
        case GR_GL_UNSIGNED_INT_8_8_8_8_REV:        return sizeof(GrGLuint);
        case GR_GL_UNSIGNED_INT_10_10_10_2:         return sizeof(GrGLuint);
        case GR_GL_UNSIGNED_INT_24_8:               return sizeof(GrGLuint);
        case GR_GL_UNSIGNED_INT_10F_11F_11F_REV:    return sizeof(GrGLuint);
        case GR_GL_UNSIGNED_INT_5_9_9_9_REV:        return sizeof(GrGLuint);
        case GR_GL_FLOAT_32_UNSIGNED_INT_24_8_REV:  return sizeof(GrGLuint);
#endif
        default:                                    return 0;
    }
}
