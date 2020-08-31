/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/effects/GrTextureEffect.h"

#include "src/gpu/GrTexture.h"
#include "src/gpu/GrTexturePriv.h"
#include "src/gpu/effects/generated/GrMatrixEffect.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/sksl/SkSLCPP.h"
#include "src/sksl/SkSLUtil.h"

using Mode = GrSamplerState::WrapMode;
using Filter = GrSamplerState::Filter;

struct GrTextureEffect::Sampling {
    GrSamplerState fHWSampler;
    ShaderMode fShaderModes[2] = {ShaderMode::kNone, ShaderMode::kNone};
    SkRect fShaderSubset = {0, 0, 0, 0};
    SkRect fShaderClamp  = {0, 0, 0, 0};
    float fBorder[4] = {0, 0, 0, 0};
    Sampling(GrSamplerState::Filter filter) : fHWSampler(filter) {}
    Sampling(const GrSurfaceProxy& proxy,
             GrSamplerState sampler,
             const SkRect&,
             const SkRect*,
             const float border[4],
             const GrCaps&);
    inline bool hasBorderAlpha() const;
};

GrTextureEffect::Sampling::Sampling(const GrSurfaceProxy& proxy,
                                    GrSamplerState sampler,
                                    const SkRect& subset,
                                    const SkRect* domain,
                                    const float border[4],
                                    const GrCaps& caps) {
    struct Span {
        float fA = 0.f, fB = 0.f;

        Span makeInset(float o) const {
            Span r = {fA + o, fB - o};
            if (r.fA > r.fB) {
                r.fA = r.fB = (r.fA + r.fB) / 2;
            }
            return r;
        }

        bool contains(Span r) const { return fA <= r.fA && fB >= r.fB; }
    };
    struct Result1D {
        ShaderMode fShaderMode;
        Span fShaderSubset;
        Span fShaderClamp;
        Mode fHWMode;
    };

    auto type = proxy.asTextureProxy()->textureType();
    auto filter = sampler.filter();

    auto resolve = [type, &caps, filter, &border](int size, Mode mode, Span subset, Span domain) {
        Result1D r;
        bool canDoModeInHW = true;
        // TODO: Use HW border color when available.
        if (mode == Mode::kClampToBorder &&
            (!caps.clampToBorderSupport() || border[0] || border[1] || border[2] || border[3])) {
            canDoModeInHW = false;
        } else if (mode != Mode::kClamp && !caps.npotTextureTileSupport() && !SkIsPow2(size)) {
            canDoModeInHW = false;
        } else if (type != GrTextureType::k2D &&
                   !(mode == Mode::kClamp || mode == Mode::kClampToBorder)) {
            canDoModeInHW = false;
        }
        if (canDoModeInHW && size > 0 && subset.fA <= 0 && subset.fB >= size) {
            r.fShaderMode = ShaderMode::kNone;
            r.fHWMode = mode;
            r.fShaderSubset = r.fShaderClamp = {0, 0};
            return r;
        }

        r.fShaderSubset = subset;
        bool domainIsSafe = false;
        if (filter == Filter::kNearest) {
            Span isubset{sk_float_floor(subset.fA), sk_float_ceil(subset.fB)};
            if (domain.fA > isubset.fA && domain.fB < isubset.fB) {
                domainIsSafe = true;
            }
            // This inset prevents sampling neighboring texels that could occur when
            // texture coords fall exactly at texel boundaries (depending on precision
            // and GPU-specific snapping at the boundary).
            r.fShaderClamp = isubset.makeInset(0.5f);
        } else {
            r.fShaderClamp = subset.makeInset(0.5f);
            if (r.fShaderClamp.contains(domain)) {
                domainIsSafe = true;
            }
        }
        if (domainIsSafe) {
            // The domain of coords that will be used won't access texels outside of the subset.
            // So the wrap mode effectively doesn't matter. We use kClamp since it is always
            // supported.
            r.fShaderMode = ShaderMode::kNone;
            r.fHWMode = Mode::kClamp;
            r.fShaderSubset = r.fShaderClamp = {0, 0};
            return r;
        }
        r.fShaderMode = static_cast<ShaderMode>(mode);
        r.fHWMode = Mode::kClamp;
        return r;
    };

    SkISize dim = proxy.isFullyLazy() ? SkISize{-1, -1} : proxy.backingStoreDimensions();

    Span subsetX{subset.fLeft, subset.fRight};
    auto domainX = domain ? Span{domain->fLeft, domain->fRight}
                          : Span{SK_FloatNegativeInfinity, SK_FloatInfinity};
    auto x = resolve(dim.width(), sampler.wrapModeX(), subsetX, domainX);

    Span subsetY{subset.fTop, subset.fBottom};
    auto domainY = domain ? Span{domain->fTop, domain->fBottom}
                          : Span{SK_FloatNegativeInfinity, SK_FloatInfinity};
    auto y = resolve(dim.height(), sampler.wrapModeY(), subsetY, domainY);

    fHWSampler = {x.fHWMode, y.fHWMode, filter};
    fShaderModes[0] = x.fShaderMode;
    fShaderModes[1] = y.fShaderMode;
    fShaderSubset = {x.fShaderSubset.fA, y.fShaderSubset.fA,
                     x.fShaderSubset.fB, y.fShaderSubset.fB};
    fShaderClamp = {x.fShaderClamp.fA, y.fShaderClamp.fA,
                    x.fShaderClamp.fB, y.fShaderClamp.fB};
    std::copy_n(border, 4, fBorder);
}

bool GrTextureEffect::Sampling::hasBorderAlpha() const {
    if (fHWSampler.wrapModeX() == GrSamplerState::WrapMode::kClampToBorder ||
        fHWSampler.wrapModeY() == GrSamplerState::WrapMode::kClampToBorder) {
        return true;
    }
    if (fShaderModes[0] == ShaderMode::kClampToBorder ||
        fShaderModes[1] == ShaderMode::kClampToBorder) {
        return fBorder[3] < 1.f;
    }
    return false;
}

static void get_matrix(const SkMatrix& preMatrix, const GrSurfaceProxyView& view,
                       SkMatrix* outMatrix, bool* outLazyProxyNormalization) {
    SkMatrix combined = preMatrix;
    bool normalize = view.proxy()->backendFormat().textureType() != GrTextureType::kRectangle;
    if (normalize) {
        if (view.proxy()->isFullyLazy()) {
            *outLazyProxyNormalization = true;
        } else {
            SkMatrixPriv::PostIDiv(&combined, view.proxy()->backingStoreDimensions().fWidth,
                                              view.proxy()->backingStoreDimensions().fHeight);
            *outLazyProxyNormalization = false;
        }
    } else {
        *outLazyProxyNormalization = false;
    }
    if (view.origin() == kBottomLeft_GrSurfaceOrigin) {
        if (normalize) {
            // combined.postScale(1,-1);
            // combined.postTranslate(0,1);
            combined.set(SkMatrix::kMSkewY,
                         combined[SkMatrix::kMPersp0] - combined[SkMatrix::kMSkewY]);
            combined.set(SkMatrix::kMScaleY,
                         combined[SkMatrix::kMPersp1] - combined[SkMatrix::kMScaleY]);
            combined.set(SkMatrix::kMTransY,
                         combined[SkMatrix::kMPersp2] - combined[SkMatrix::kMTransY]);
        } else {
            // combined.postScale(1, -1);
            // combined.postTranslate(0,1);
            SkScalar h = view.proxy()->backingStoreDimensions().fHeight;
            combined.set(SkMatrix::kMSkewY,
                         h * combined[SkMatrix::kMPersp0] - combined[SkMatrix::kMSkewY]);
            combined.set(SkMatrix::kMScaleY,
                         h * combined[SkMatrix::kMPersp1] - combined[SkMatrix::kMScaleY]);
            combined.set(SkMatrix::kMTransY,
                         h * combined[SkMatrix::kMPersp2] - combined[SkMatrix::kMTransY]);
        }
    }
    *outMatrix = combined;
}

std::unique_ptr<GrFragmentProcessor> GrTextureEffect::Make(GrSurfaceProxyView view,
                                                           SkAlphaType alphaType,
                                                           const SkMatrix& matrix,
                                                           Filter filter) {
    SkMatrix final;
    bool lazyProxyNormalization;
    get_matrix(matrix, view, &final, &lazyProxyNormalization);
    return GrMatrixEffect::Apply(final, std::unique_ptr<GrFragmentProcessor>(
                                                      new GrTextureEffect(std::move(view),
                                                                          alphaType,
                                                                          Sampling(filter),
                                                                          lazyProxyNormalization)));
}

std::unique_ptr<GrFragmentProcessor> GrTextureEffect::Make(GrSurfaceProxyView view,
                                                           SkAlphaType alphaType,
                                                           const SkMatrix& matrix,
                                                           GrSamplerState sampler,
                                                           const GrCaps& caps,
                                                           const float border[4]) {
    Sampling sampling(*view.proxy(), sampler, SkRect::Make(view.proxy()->dimensions()), nullptr,
                      border, caps);
    SkMatrix final;
    bool lazyProxyNormalization;
    get_matrix(matrix, view, &final, &lazyProxyNormalization);
    return GrMatrixEffect::Apply(final, std::unique_ptr<GrFragmentProcessor>(
                                                      new GrTextureEffect(std::move(view),
                                                                          alphaType,
                                                                          sampling,
                                                                          lazyProxyNormalization)));
}

std::unique_ptr<GrFragmentProcessor> GrTextureEffect::MakeSubset(GrSurfaceProxyView view,
                                                                 SkAlphaType alphaType,
                                                                 const SkMatrix& matrix,
                                                                 GrSamplerState sampler,
                                                                 const SkRect& subset,
                                                                 const GrCaps& caps,
                                                                 const float border[4]) {
    Sampling sampling(*view.proxy(), sampler, subset, nullptr, border, caps);
    SkMatrix final;
    bool lazyProxyNormalization;
    get_matrix(matrix, view, &final, &lazyProxyNormalization);
    return GrMatrixEffect::Apply(final, std::unique_ptr<GrFragmentProcessor>(
                                                      new GrTextureEffect(std::move(view),
                                                                          alphaType,
                                                                          sampling,
                                                                          lazyProxyNormalization)));
}

std::unique_ptr<GrFragmentProcessor> GrTextureEffect::MakeSubset(GrSurfaceProxyView view,
                                                                 SkAlphaType alphaType,
                                                                 const SkMatrix& matrix,
                                                                 GrSamplerState sampler,
                                                                 const SkRect& subset,
                                                                 const SkRect& domain,
                                                                 const GrCaps& caps,
                                                                 const float border[4]) {
    Sampling sampling(*view.proxy(), sampler, subset, &domain, border, caps);
    SkMatrix final;
    bool lazyProxyNormalization;
    get_matrix(matrix, view, &final, &lazyProxyNormalization);
    return GrMatrixEffect::Apply(final, std::unique_ptr<GrFragmentProcessor>(
                                                      new GrTextureEffect(std::move(view),
                                                                          alphaType,
                                                                          sampling,
                                                                          lazyProxyNormalization)));
}

GrTextureEffect::FilterLogic GrTextureEffect::GetFilterLogic(ShaderMode mode,
                                                             GrSamplerState::Filter filter) {
    switch (mode) {
        case ShaderMode::kMirrorRepeat:
        case ShaderMode::kNone:
        case ShaderMode::kClamp:
            return FilterLogic::kNone;
        case ShaderMode::kRepeat:
            switch (filter) {
                case GrSamplerState::Filter::kNearest:
                    return FilterLogic::kNone;
                case GrSamplerState::Filter::kBilerp:
                    return FilterLogic::kRepeatBilerp;
                case GrSamplerState::Filter::kMipMap:
                    return FilterLogic::kRepeatMipMap;
            }
            SkUNREACHABLE;
        case ShaderMode::kClampToBorder:
            return filter > GrSamplerState::Filter::kNearest ? FilterLogic::kClampToBorderFilter
                                                             : FilterLogic::kClampToBorderNearest;
    }
    SkUNREACHABLE;
}

GrGLSLFragmentProcessor* GrTextureEffect::onCreateGLSLInstance() const {
    class Impl : public GrGLSLFragmentProcessor {
        UniformHandle fSubsetUni;
        UniformHandle fClampUni;
        UniformHandle fNormUni;
        UniformHandle fBorderUni;

    public:
        void emitCode(EmitArgs& args) override {
            auto& te = args.fFp.cast<GrTextureEffect>();
            SkString coords;
            if (args.fFp.isSampledWithExplicitCoords()) {
                coords = "_coords";
            } else {
                coords = args.fTransformedCoords[0].fVaryingPoint.c_str();
            }
            auto* fb = args.fFragBuilder;
            if (te.sampleMatrix().fKind == SkSL::SampleMatrix::Kind::kMixed) {
                args.fUniformHandler->writeUniformMappings(te.sampleMatrix().fOwner, fb);
                coords = SkStringPrintf("(%s * _matrix * float3(%s, 1)).xy",
                                        te.sampleMatrix().fExpression.c_str(),
                                        coords.c_str());
            }
            if (te.fShaderModes[0] == ShaderMode::kNone &&
                te.fShaderModes[1] == ShaderMode::kNone) {
                fb->codeAppendf("%s = ", args.fOutputColor);
                if (te.fLazyProxyNormalization) {
                    const char* norm = nullptr;
                    fNormUni = args.fUniformHandler->addUniform(&te, kFragment_GrShaderFlag,
                                                                kFloat4_GrSLType, "norm", &norm);
                    fb->appendTextureLookupAndBlend(args.fInputColor, SkBlendMode::kModulate,
                                                    args.fTexSamplers[0],
                                                    SkStringPrintf("%s * %s.zw", coords.c_str(),
                                                                   norm).c_str());
                } else {
                    fb->appendTextureLookupAndBlend(args.fInputColor, SkBlendMode::kModulate,
                                                    args.fTexSamplers[0], coords.c_str());
                }
                fb->codeAppendf(";");
            } else {
                // Tripping this assert means we have a normalized fully lazy proxy with a
                // non-default ShaderMode. There's nothing fundamentally wrong with doing that, but
                // it hasn't been tested and this code path probably won't handle normalization
                // properly in that case.
                SkASSERT(!te.fLazyProxyNormalization);
                // Here is the basic flow of the various ShaderModes are implemented in a series of
                // steps. Not all the steps apply to all the modes. We try to emit only the steps
                // that are necessary for the given x/y shader modes.
                //
                // 0) Start with interpolated coordinates (unnormalize if doing anything
                //    complicated).
                // 1) Map the coordinates into the subset range [Repeat and MirrorRepeat], or pass
                //    through output of 0).
                // 2) Clamp the coordinates to a 0.5 inset of the subset rect [Clamp, Repeat, and
                //    MirrorRepeat always or ClampToBorder only when filtering] or pass through
                //    output of 1). The clamp rect collapses to a line or point it if the subset
                //    rect is less than one pixel wide/tall.
                // 3) Look up texture with output of 2) [All]
                // 3) Use the difference between 1) and 2) to apply filtering at edge [Repeat or
                //    ClampToBorder]. In the Repeat case this requires extra texture lookups on the
                //    other side of the subset (up to 3 more reads). Or if ClampToBorder and not
                //    filtering do a hard less than/greater than test with the subset rect.

                // Convert possible projective texture coordinates into non-homogeneous half2.
                fb->codeAppendf(
                        "float2 inCoord = %s;",
                        fb->ensureCoords2D(args.fTransformedCoords[0].fVaryingPoint,
                                           te.sampleMatrix()).c_str());

                const auto& m = te.fShaderModes;
                GrTextureType textureType = te.fSampler.proxy()->backendFormat().textureType();
                bool normCoords = textureType != GrTextureType::kRectangle;
                auto filter = te.fSampler.samplerState().filter();
                FilterLogic filterLogic[2] = {GetFilterLogic(m[0], filter),
                                              GetFilterLogic(m[1], filter)};

                const char* borderName = nullptr;
                if (te.fShaderModes[0] == ShaderMode::kClampToBorder ||
                    te.fShaderModes[1] == ShaderMode::kClampToBorder) {
                    fBorderUni = args.fUniformHandler->addUniform(
                            &te, kFragment_GrShaderFlag, kHalf4_GrSLType, "border", &borderName);
                }
                auto modeUsesSubset = [](ShaderMode m) {
                    return m == ShaderMode::kRepeat || m == ShaderMode::kMirrorRepeat ||
                           m == ShaderMode::kClampToBorder;
                };

                auto modeUsesClamp = [filter](ShaderMode m) {
                    return m != ShaderMode::kNone &&
                           (m != ShaderMode::kClampToBorder || filter > Filter::kNearest);
                };

                bool useSubset[2] = {modeUsesSubset(m[0]), modeUsesSubset(m[1])};
                bool useClamp [2] = {modeUsesClamp (m[0]), modeUsesClamp (m[1])};

                const char* subsetName = nullptr;
                if (useSubset[0] || useSubset[1]) {
                    fSubsetUni = args.fUniformHandler->addUniform(
                            &te, kFragment_GrShaderFlag, kFloat4_GrSLType, "subset", &subsetName);
                }

                const char* clampName = nullptr;
                if (useClamp[0] || useClamp[1]) {
                    fClampUni = args.fUniformHandler->addUniform(
                            &te, kFragment_GrShaderFlag, kFloat4_GrSLType, "clamp", &clampName);
                }

                // To keep things a little simpler, when we have filtering logic in the shader we
                // operate on unnormalized texture coordinates. We add a uniform that stores
                // {w, h, 1/w, 1/h} in a float4.
                const char* norm = nullptr;
                if (normCoords && (filterLogic[0] != FilterLogic::kNone ||
                                   filterLogic[1] != FilterLogic::kNone)) {
                    // TODO: Detect support for textureSize() or polyfill textureSize() in SkSL and
                    // always use?
                    fNormUni = args.fUniformHandler->addUniform(&te, kFragment_GrShaderFlag,
                                                                kFloat4_GrSLType, "norm", &norm);
                    // TODO: Remove the normalization from the CoordTransform to skip unnormalizing
                    // step here.
                    fb->codeAppendf("inCoord *= %s.xy;", norm);
                }

                // Generates a string to read at a coordinate, normalizing coords if necessary.
                auto read = [&](const char* coord) {
                    SkString result;
                    SkString normCoord;
                    if (norm) {
                        normCoord.printf("(%s) * %s.zw", coord, norm);
                    } else {
                        normCoord = coord;
                    }
                    fb->appendTextureLookup(&result, args.fTexSamplers[0], normCoord.c_str());
                    return result;
                };

                // Implements coord wrapping for kRepeat and kMirrorRepeat
                auto subsetCoord = [&](ShaderMode mode,
                                       const char* coordSwizzle,
                                       const char* subsetStartSwizzle,
                                       const char* subsetStopSwizzle,
                                       const char* extraCoord,
                                       const char* coordWeight) {
                    switch (mode) {
                        // These modes either don't use the subset rect or don't need to map the
                        // coords to be within the subset.
                        case ShaderMode::kNone:
                        case ShaderMode::kClampToBorder:
                        case ShaderMode::kClamp:
                            fb->codeAppendf("subsetCoord.%s = inCoord.%s;", coordSwizzle,
                                            coordSwizzle);
                            break;
                        case ShaderMode::kRepeat:
                            if (filter == Filter::kMipMap) {
                                // The approach here is to generate two sets of texture coords that
                                // are both "moving" at the same speed (if not direction) as
                                // inCoords. We accomplish that by using two out of phase mirror
                                // repeat coords. We will always sample using both coords but the
                                // read from the upward sloping one is selected using a weight
                                // that transitions from one set to the other near the reflection
                                // point. Like the coords, the weight is a saw-tooth function,
                                // phase-shifted, vertically translated, and then clamped to 0..1.
                                // TODO: Skip this and use textureGrad() when available.
                                SkASSERT(extraCoord);
                                SkASSERT(coordWeight);
                                fb->codeAppend("{");
                                fb->codeAppendf("float w = %s.%s - %s.%s;", subsetName,
                                                subsetStopSwizzle, subsetName, subsetStartSwizzle);
                                fb->codeAppendf("float w2 = 2 * w;");
                                fb->codeAppendf("float d = inCoord.%s - %s.%s;", coordSwizzle,
                                                subsetName, subsetStartSwizzle);
                                fb->codeAppend("float m = mod(d, w2);");
                                fb->codeAppend("float o = mix(m, w2 - m, step(w, m));");
                                fb->codeAppendf("subsetCoord.%s = o + %s.%s;", coordSwizzle,
                                                subsetName, subsetStartSwizzle);
                                fb->codeAppendf("%s = w - o + %s.%s;", extraCoord, subsetName,
                                                subsetStartSwizzle);
                                // coordWeight is used as the third param of mix() to blend between a
                                // sample taken using subsetCoord and a sample at extraCoord.
                                fb->codeAppend("float hw = w/2;");
                                fb->codeAppend("float n = mod(d - hw, w2);");
                                fb->codeAppendf(
                                        "%s = saturate(half(mix(n, w2 - n, step(w, n)) - hw + "
                                        "0.5));",
                                        coordWeight);
                                fb->codeAppend("}");
                            } else {
                                fb->codeAppendf(
                                        "subsetCoord.%s = mod(inCoord.%s - %s.%s, %s.%s - %s.%s) + "
                                        "%s.%s;",
                                        coordSwizzle, coordSwizzle, subsetName, subsetStartSwizzle,
                                        subsetName, subsetStopSwizzle, subsetName,
                                        subsetStartSwizzle, subsetName, subsetStartSwizzle);
                            }
                            break;
                        case ShaderMode::kMirrorRepeat: {
                            fb->codeAppend("{");
                            fb->codeAppendf("float w = %s.%s - %s.%s;", subsetName,
                                            subsetStopSwizzle, subsetName, subsetStartSwizzle);
                            fb->codeAppendf("float w2 = 2 * w;");
                            fb->codeAppendf("float m = mod(inCoord.%s - %s.%s, w2);", coordSwizzle,
                                            subsetName, subsetStartSwizzle);
                            fb->codeAppendf("subsetCoord.%s = mix(m, w2 - m, step(w, m)) + %s.%s;",
                                            coordSwizzle, subsetName, subsetStartSwizzle);
                            fb->codeAppend("}");
                            break;
                        }
                    }
                };

                auto clampCoord = [&](bool clamp,
                                      const char* coordSwizzle,
                                      const char* clampStartSwizzle,
                                      const char* clampStopSwizzle) {
                    if (clamp) {
                        fb->codeAppendf("clampedCoord.%s = clamp(subsetCoord.%s, %s.%s, %s.%s);",
                                        coordSwizzle, coordSwizzle, clampName, clampStartSwizzle,
                                        clampName, clampStopSwizzle);
                    } else {
                        fb->codeAppendf("clampedCoord.%s = subsetCoord.%s;", coordSwizzle,
                                        coordSwizzle);
                    }
                };

                // Insert vars for extra coords and blending weights for kRepeatMipMap.
                const char* extraRepeatCoordX  = nullptr;
                const char* repeatCoordWeightX = nullptr;
                const char* extraRepeatCoordY  = nullptr;
                const char* repeatCoordWeightY = nullptr;
                if (filterLogic[0] == FilterLogic::kRepeatMipMap) {
                    fb->codeAppend("float extraRepeatCoordX; half repeatCoordWeightX;");
                    extraRepeatCoordX   = "extraRepeatCoordX";
                    repeatCoordWeightX  = "repeatCoordWeightX";
                }
                if (filterLogic[1] == FilterLogic::kRepeatMipMap) {
                    fb->codeAppend("float extraRepeatCoordY; half repeatCoordWeightY;");
                    extraRepeatCoordY   = "extraRepeatCoordY";
                    repeatCoordWeightY  = "repeatCoordWeightY";
                }

                // Apply subset rect and clamp rect to coords.
                fb->codeAppend("float2 subsetCoord;");
                subsetCoord(te.fShaderModes[0], "x", "x", "z", extraRepeatCoordX,
                            repeatCoordWeightX);
                subsetCoord(te.fShaderModes[1], "y", "y", "w", extraRepeatCoordY,
                            repeatCoordWeightY);
                fb->codeAppend("float2 clampedCoord;");
                clampCoord(useClamp[0], "x", "x", "z");
                clampCoord(useClamp[1], "y", "y", "w");

                // Additional clamping for the extra coords for kRepeatMipMap.
                if (filterLogic[0] == FilterLogic::kRepeatMipMap) {
                    fb->codeAppendf("extraRepeatCoordX = clamp(extraRepeatCoordX, %s.x, %s.z);",
                                    clampName, clampName);
                }
                if (filterLogic[1] == FilterLogic::kRepeatMipMap) {
                    fb->codeAppendf("extraRepeatCoordY = clamp(extraRepeatCoordY, %s.y, %s.w);",
                                    clampName, clampName);
                }

                // Do the 2 or 4 texture reads for kRepeatMipMap and then apply the weight(s)
                // to blend between them. If neither direction is kRepeatMipMap do a single
                // read at clampedCoord.
                if (filterLogic[0] == FilterLogic::kRepeatMipMap &&
                    filterLogic[1] == FilterLogic::kRepeatMipMap) {
                    fb->codeAppendf(
                            "half4 textureColor ="
                            "   mix(mix(%s, %s, repeatCoordWeightX),"
                            "       mix(%s, %s, repeatCoordWeightX),"
                            "       repeatCoordWeightY);",
                            read("clampedCoord").c_str(),
                            read("float2(extraRepeatCoordX, clampedCoord.y)").c_str(),
                            read("float2(clampedCoord.x, extraRepeatCoordY)").c_str(),
                            read("float2(extraRepeatCoordX, extraRepeatCoordY)").c_str());

                } else if (filterLogic[0] == FilterLogic::kRepeatMipMap) {
                    fb->codeAppendf("half4 textureColor = mix(%s, %s, repeatCoordWeightX);",
                                    read("clampedCoord").c_str(),
                                    read("float2(extraRepeatCoordX, clampedCoord.y)").c_str());
                } else if (filterLogic[1] == FilterLogic::kRepeatMipMap) {
                    fb->codeAppendf("half4 textureColor = mix(%s, %s, repeatCoordWeightY);",
                                    read("clampedCoord").c_str(),
                                    read("float2(clampedCoord.x, extraRepeatCoordY)").c_str());
                } else {
                    fb->codeAppendf("half4 textureColor = %s;", read("clampedCoord").c_str());
                }

                // Strings for extra texture reads used only in kRepeatBilerp
                SkString repeatBilerpReadX;
                SkString repeatBilerpReadY;

                // Calculate the amount the coord moved for clamping. This will be used
                // to implement shader-based filtering for kClampToBorder and kRepeat.

                if (filterLogic[0] == FilterLogic::kRepeatBilerp ||
                    filterLogic[0] == FilterLogic::kClampToBorderFilter) {
                    fb->codeAppend("half errX = half(subsetCoord.x - clampedCoord.x);");
                    fb->codeAppendf("float repeatCoordX = errX > 0 ? %s.x : %s.z;", clampName,
                                    clampName);
                    repeatBilerpReadX = read("float2(repeatCoordX, clampedCoord.y)");
                }
                if (filterLogic[1] == FilterLogic::kRepeatBilerp ||
                    filterLogic[1] == FilterLogic::kClampToBorderFilter) {
                    fb->codeAppend("half errY = half(subsetCoord.y - clampedCoord.y);");
                    fb->codeAppendf("float repeatCoordY = errY > 0 ? %s.y : %s.w;", clampName,
                                    clampName);
                    repeatBilerpReadY = read("float2(clampedCoord.x, repeatCoordY)");
                }

                // Add logic for kRepeatBilerp. Do 1 or 3 more texture reads depending
                // on whether both modes are kRepeat and whether we're near a single subset edge
                // or a corner. Then blend the multiple reads using the err values calculated
                // above.
                const char* ifStr = "if";
                if (filterLogic[0] == FilterLogic::kRepeatBilerp &&
                    filterLogic[1] == FilterLogic::kRepeatBilerp) {
                    auto repeatBilerpReadXY = read("float2(repeatCoordX, repeatCoordY)");
                    fb->codeAppendf(
                            "if (errX != 0 && errY != 0) {"
                            "    errX = abs(errX);"
                            "    textureColor = mix(mix(textureColor, %s, errX),"
                            "                       mix(%s, %s, errX),"
                            "                       abs(errY));"
                            "}",
                            repeatBilerpReadX.c_str(), repeatBilerpReadY.c_str(),
                            repeatBilerpReadXY.c_str());
                    ifStr = "else if";
                }
                if (filterLogic[0] == FilterLogic::kRepeatBilerp) {
                    fb->codeAppendf(
                            "%s (errX != 0) {"
                            "    textureColor = mix(textureColor, %s, abs(errX));"
                            "}",
                            ifStr, repeatBilerpReadX.c_str());
                }
                if (filterLogic[1] == FilterLogic::kRepeatBilerp) {
                    fb->codeAppendf(
                            "%s (errY != 0) {"
                            "    textureColor = mix(textureColor, %s, abs(errY));"
                            "}",
                            ifStr, repeatBilerpReadY.c_str());
                }

                // Do soft edge shader filtering against border color for kClampToBorderFilter using
                // the err values calculated above.
                if (filterLogic[0] == FilterLogic::kClampToBorderFilter) {
                    fb->codeAppendf("textureColor = mix(textureColor, %s, min(abs(errX), 1));",
                                    borderName);
                }
                if (filterLogic[1] == FilterLogic::kClampToBorderFilter) {
                    fb->codeAppendf("textureColor = mix(textureColor, %s, min(abs(errY), 1));",
                                    borderName);
                }

                // Do hard-edge shader transition to border color for kClampToBorderNearest at the
                // subset boundaries.
                if (filterLogic[0] == FilterLogic::kClampToBorderNearest) {
                    fb->codeAppendf(
                            "if (inCoord.x < %s.x || inCoord.x > %s.z) {"
                            "    textureColor = %s;"
                            "}",
                            subsetName, subsetName, borderName);
                }
                if (filterLogic[1] == FilterLogic::kClampToBorderNearest) {
                    fb->codeAppendf(
                            "if (inCoord.y < %s.y || inCoord.y > %s.w) {"
                            "    textureColor = %s;"
                            "}",
                            subsetName, subsetName, borderName);
                }
                fb->codeAppendf("%s = %s * textureColor;", args.fOutputColor, args.fInputColor);
            }
        }

    protected:
        void onSetData(const GrGLSLProgramDataManager& pdm,
                       const GrFragmentProcessor& fp) override {
            const auto& te = fp.cast<GrTextureEffect>();

            const float w = te.fSampler.peekTexture()->width();
            const float h = te.fSampler.peekTexture()->height();
            const auto& s = te.fSubset;
            const auto& c = te.fClamp;

            auto type = te.fSampler.peekTexture()->texturePriv().textureType();

            float norm[4] = {w, h, 1.f/w, 1.f/h};

            if (fNormUni.isValid()) {
                pdm.set4fv(fNormUni, 1, norm);
                SkASSERT(type != GrTextureType::kRectangle);
            }

            auto pushRect = [&](float rect[4], UniformHandle uni) {
                if (te.fSampler.view().origin() == kBottomLeft_GrSurfaceOrigin) {
                    rect[1] = h - rect[1];
                    rect[3] = h - rect[3];
                    std::swap(rect[1], rect[3]);
                }
                if (!fNormUni.isValid() && type != GrTextureType::kRectangle) {
                    rect[0] *= norm[2];
                    rect[2] *= norm[2];
                    rect[1] *= norm[3];
                    rect[3] *= norm[3];
                }
                pdm.set4fv(uni, 1, rect);
            };

            if (fSubsetUni.isValid()) {
                float subset[] = {s.fLeft, s.fTop, s.fRight, s.fBottom};
                pushRect(subset, fSubsetUni);
            }
            if (fClampUni.isValid()) {
                float subset[] = {c.fLeft, c.fTop, c.fRight, c.fBottom};
                pushRect(subset, fClampUni);
            }
            if (fBorderUni.isValid()) {
                pdm.set4fv(fBorderUni, 1, te.fBorder);
            }
        }
    };
    return new Impl;
}

void GrTextureEffect::onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder* b) const {
    auto m0 = static_cast<uint32_t>(fShaderModes[0]);
    auto m1 = static_cast<uint32_t>(fShaderModes[1]);
    auto filter = fSampler.samplerState().filter();
    auto l0 = static_cast<uint32_t>(GetFilterLogic(fShaderModes[0], filter));
    auto l1 = static_cast<uint32_t>(GetFilterLogic(fShaderModes[1], filter));
    b->add32((l0 << 24) | (l1 << 16) | (m0 << 8) | m1);
}

bool GrTextureEffect::onIsEqual(const GrFragmentProcessor& other) const {
    auto& that = other.cast<GrTextureEffect>();
    if (fShaderModes[0] != that.fShaderModes[0] || fShaderModes[1] != that.fShaderModes[1]) {
        return false;
    }
    if (fSubset != that.fSubset) {
        return false;
    }
    if ((fShaderModes[0] == ShaderMode::kClampToBorder ||
         fShaderModes[1] == ShaderMode::kClampToBorder) &&
        !std::equal(fBorder, fBorder + 4, that.fBorder)) {
        return false;
    }
    return true;
}

GrTextureEffect::GrTextureEffect(GrSurfaceProxyView view, SkAlphaType alphaType,
                                 const Sampling& sampling, bool lazyProxyNormalization)
        : GrFragmentProcessor(kGrTextureEffect_ClassID,
                              ModulateForSamplerOptFlags(alphaType, sampling.hasBorderAlpha()))
        , fCoordTransform(SkMatrix::I())
        , fSampler(std::move(view), sampling.fHWSampler)
        , fSubset(sampling.fShaderSubset)
        , fClamp(sampling.fShaderClamp)
        , fShaderModes{sampling.fShaderModes[0], sampling.fShaderModes[1]}
        , fLazyProxyNormalization(lazyProxyNormalization) {
    // We always compare the range even when it isn't used so assert we have canonical don't care
    // values.
    SkASSERT(fShaderModes[0] != ShaderMode::kNone || (fSubset.fLeft == 0 && fSubset.fRight == 0));
    SkASSERT(fShaderModes[1] != ShaderMode::kNone || (fSubset.fTop == 0 && fSubset.fBottom == 0));
    this->setTextureSamplerCnt(1);
    this->addCoordTransform(&fCoordTransform);
    std::copy_n(sampling.fBorder, 4, fBorder);
}

GrTextureEffect::GrTextureEffect(const GrTextureEffect& src)
        : INHERITED(kGrTextureEffect_ClassID, src.optimizationFlags())
        , fCoordTransform(src.fCoordTransform)
        , fSampler(src.fSampler)
        , fSubset(src.fSubset)
        , fClamp(src.fClamp)
        , fShaderModes{src.fShaderModes[0], src.fShaderModes[1]}
        , fLazyProxyNormalization(src.fLazyProxyNormalization) {
    std::copy_n(src.fBorder, 4, fBorder);
    this->setTextureSamplerCnt(1);
    this->addCoordTransform(&fCoordTransform);
}

std::unique_ptr<GrFragmentProcessor> GrTextureEffect::clone() const {
    return std::unique_ptr<GrFragmentProcessor>(new GrTextureEffect(*this));
}

const GrFragmentProcessor::TextureSampler& GrTextureEffect::onTextureSampler(int) const {
    return fSampler;
}

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrTextureEffect);
#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> GrTextureEffect::TestCreate(GrProcessorTestData* testData) {
    auto [view, ct, at] = testData->randomView();
    Mode wrapModes[2];
    GrTest::TestWrapModes(testData->fRandom, wrapModes);

    Filter filter;
    if (view.asTextureProxy()->mipMapped() == GrMipMapped::kYes) {
        switch (testData->fRandom->nextULessThan(3)) {
            case 0:
                filter = Filter::kNearest;
                break;
            case 1:
                filter = Filter::kBilerp;
                break;
            default:
                filter = Filter::kMipMap;
                break;
        }
    } else {
        filter = testData->fRandom->nextBool() ? Filter::kBilerp : Filter::kNearest;
    }
    GrSamplerState params(wrapModes, filter);

    const SkMatrix& matrix = GrTest::TestMatrix(testData->fRandom);
    return GrTextureEffect::Make(std::move(view), at, matrix, params, *testData->caps());
}
#endif
