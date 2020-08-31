/*
 * Copyright 2012 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/effects/SkLightingImageFilter.h"

#include "include/core/SkBitmap.h"
#include "include/core/SkPoint3.h"
#include "include/core/SkTypes.h"
#include "include/private/SkColorData.h"
#include "src/core/SkImageFilter_Base.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkSpecialImage.h"
#include "src/core/SkWriteBuffer.h"

#if SK_SUPPORT_GPU
#include "include/private/GrRecordingContext.h"
#include "src/gpu/GrCaps.h"
#include "src/gpu/GrFixedClip.h"
#include "src/gpu/GrFragmentProcessor.h"
#include "src/gpu/GrPaint.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrRenderTargetContext.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/GrTextureProxy.h"

#include "src/gpu/SkGr.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramDataManager.h"
#include "src/gpu/glsl/GrGLSLUniformHandler.h"

class GrGLDiffuseLightingEffect;
class GrGLSpecularLightingEffect;

// For brevity
typedef GrGLSLProgramDataManager::UniformHandle UniformHandle;
#endif

const SkScalar gOneThird = SkIntToScalar(1) / 3;
const SkScalar gTwoThirds = SkIntToScalar(2) / 3;
const SkScalar gOneHalf = 0.5f;
const SkScalar gOneQuarter = 0.25f;

#if SK_SUPPORT_GPU
static void setUniformPoint3(const GrGLSLProgramDataManager& pdman, UniformHandle uni,
                             const SkPoint3& point) {
    static_assert(sizeof(SkPoint3) == 3 * sizeof(float));
    pdman.set3fv(uni, 1, &point.fX);
}

static void setUniformNormal3(const GrGLSLProgramDataManager& pdman, UniformHandle uni,
                              const SkPoint3& point) {
    setUniformPoint3(pdman, uni, point);
}
#endif

// Shift matrix components to the left, as we advance pixels to the right.
static inline void shiftMatrixLeft(int m[9]) {
    m[0] = m[1];
    m[3] = m[4];
    m[6] = m[7];
    m[1] = m[2];
    m[4] = m[5];
    m[7] = m[8];
}

static inline void fast_normalize(SkPoint3* vector) {
    // add a tiny bit so we don't have to worry about divide-by-zero
    SkScalar magSq = vector->dot(*vector) + SK_ScalarNearlyZero;
#if defined(_MSC_VER) && _MSC_VER >= 1920
    // Visual Studio 2019 has some kind of code-generation bug in release builds involving the
    // lighting math in this file. Using the portable rsqrt avoids the issue. This issue appears
    // to be specific to the collection of (inline) functions in this file that call into this
    // function, not with sk_float_rsqrt itself.
    SkScalar scale = sk_float_rsqrt_portable(magSq);
#else
    SkScalar scale = sk_float_rsqrt(magSq);
#endif
    vector->fX *= scale;
    vector->fY *= scale;
    vector->fZ *= scale;
}

static SkPoint3 read_point3(SkReadBuffer& buffer) {
    SkPoint3 point;
    point.fX = buffer.readScalar();
    point.fY = buffer.readScalar();
    point.fZ = buffer.readScalar();
    buffer.validate(SkScalarIsFinite(point.fX) &&
                    SkScalarIsFinite(point.fY) &&
                    SkScalarIsFinite(point.fZ));
    return point;
};

static void write_point3(const SkPoint3& point, SkWriteBuffer& buffer) {
    buffer.writeScalar(point.fX);
    buffer.writeScalar(point.fY);
    buffer.writeScalar(point.fZ);
};

class GrGLLight;
class SkImageFilterLight : public SkRefCnt {
public:
    enum LightType {
        kDistant_LightType,
        kPoint_LightType,
        kSpot_LightType,

        kLast_LightType = kSpot_LightType
    };
    virtual LightType type() const = 0;
    const SkPoint3& color() const { return fColor; }
    virtual GrGLLight* createGLLight() const = 0;
    virtual bool isEqual(const SkImageFilterLight& other) const {
        return fColor == other.fColor;
    }
    virtual SkImageFilterLight* transform(const SkMatrix& matrix) const = 0;

    // Defined below SkLight's subclasses.
    void flattenLight(SkWriteBuffer& buffer) const;
    static SkImageFilterLight* UnflattenLight(SkReadBuffer& buffer);

    virtual SkPoint3 surfaceToLight(int x, int y, int z, SkScalar surfaceScale) const = 0;
    virtual SkPoint3 lightColor(const SkPoint3& surfaceToLight) const = 0;

protected:
    SkImageFilterLight(SkColor color) {
        fColor = SkPoint3::Make(SkIntToScalar(SkColorGetR(color)),
                                SkIntToScalar(SkColorGetG(color)),
                                SkIntToScalar(SkColorGetB(color)));
    }
    SkImageFilterLight(const SkPoint3& color) : fColor(color) {}

    SkImageFilterLight(SkReadBuffer& buffer) {
        fColor = read_point3(buffer);
    }

    virtual void onFlattenLight(SkWriteBuffer& buffer) const = 0;


private:
    typedef SkRefCnt INHERITED;
    SkPoint3 fColor;
};

class BaseLightingType {
public:
    BaseLightingType() {}
    virtual ~BaseLightingType() {}

    virtual SkPMColor light(const SkPoint3& normal, const SkPoint3& surfaceTolight,
                            const SkPoint3& lightColor) const= 0;
};

class DiffuseLightingType : public BaseLightingType {
public:
    DiffuseLightingType(SkScalar kd)
        : fKD(kd) {}
    SkPMColor light(const SkPoint3& normal, const SkPoint3& surfaceTolight,
                    const SkPoint3& lightColor) const override {
        SkScalar colorScale = fKD * normal.dot(surfaceTolight);
        colorScale = SkTPin(colorScale, 0.0f, SK_Scalar1);
        SkPoint3 color = lightColor.makeScale(colorScale);
        return SkPackARGB32(255,
                            SkTPin(SkScalarRoundToInt(color.fX), 0, 255),
                            SkTPin(SkScalarRoundToInt(color.fY), 0, 255),
                            SkTPin(SkScalarRoundToInt(color.fZ), 0, 255));
    }
private:
    SkScalar fKD;
};

static SkScalar max_component(const SkPoint3& p) {
    return p.x() > p.y() ? (p.x() > p.z() ? p.x() : p.z()) : (p.y() > p.z() ? p.y() : p.z());
}

class SpecularLightingType : public BaseLightingType {
public:
    SpecularLightingType(SkScalar ks, SkScalar shininess)
        : fKS(ks), fShininess(shininess) {}
    SkPMColor light(const SkPoint3& normal, const SkPoint3& surfaceTolight,
                    const SkPoint3& lightColor) const override {
        SkPoint3 halfDir(surfaceTolight);
        halfDir.fZ += SK_Scalar1;        // eye position is always (0, 0, 1)
        fast_normalize(&halfDir);
        SkScalar colorScale = fKS * SkScalarPow(normal.dot(halfDir), fShininess);
        colorScale = SkTPin(colorScale, 0.0f, SK_Scalar1);
        SkPoint3 color = lightColor.makeScale(colorScale);
        return SkPackARGB32(SkTPin(SkScalarRoundToInt(max_component(color)), 0, 255),
                            SkTPin(SkScalarRoundToInt(color.fX), 0, 255),
                            SkTPin(SkScalarRoundToInt(color.fY), 0, 255),
                            SkTPin(SkScalarRoundToInt(color.fZ), 0, 255));
    }
private:
    SkScalar fKS;
    SkScalar fShininess;
};

static inline SkScalar sobel(int a, int b, int c, int d, int e, int f, SkScalar scale) {
    return (-a + b - 2 * c + 2 * d -e + f) * scale;
}

static inline SkPoint3 pointToNormal(SkScalar x, SkScalar y, SkScalar surfaceScale) {
    SkPoint3 vector = SkPoint3::Make(-x * surfaceScale, -y * surfaceScale, 1);
    fast_normalize(&vector);
    return vector;
}

static inline SkPoint3 topLeftNormal(int m[9], SkScalar surfaceScale) {
    return pointToNormal(sobel(0, 0, m[4], m[5], m[7], m[8], gTwoThirds),
                         sobel(0, 0, m[4], m[7], m[5], m[8], gTwoThirds),
                         surfaceScale);
}

static inline SkPoint3 topNormal(int m[9], SkScalar surfaceScale) {
    return pointToNormal(sobel(   0,    0, m[3], m[5], m[6], m[8], gOneThird),
                         sobel(m[3], m[6], m[4], m[7], m[5], m[8], gOneHalf),
                         surfaceScale);
}

static inline SkPoint3 topRightNormal(int m[9], SkScalar surfaceScale) {
    return pointToNormal(sobel(   0,    0, m[3], m[4], m[6], m[7], gTwoThirds),
                         sobel(m[3], m[6], m[4], m[7],    0,    0, gTwoThirds),
                         surfaceScale);
}

static inline SkPoint3 leftNormal(int m[9], SkScalar surfaceScale) {
    return pointToNormal(sobel(m[1], m[2], m[4], m[5], m[7], m[8], gOneHalf),
                         sobel(   0,    0, m[1], m[7], m[2], m[8], gOneThird),
                         surfaceScale);
}


static inline SkPoint3 interiorNormal(int m[9], SkScalar surfaceScale) {
    return pointToNormal(sobel(m[0], m[2], m[3], m[5], m[6], m[8], gOneQuarter),
                         sobel(m[0], m[6], m[1], m[7], m[2], m[8], gOneQuarter),
                         surfaceScale);
}

static inline SkPoint3 rightNormal(int m[9], SkScalar surfaceScale) {
    return pointToNormal(sobel(m[0], m[1], m[3], m[4], m[6], m[7], gOneHalf),
                         sobel(m[0], m[6], m[1], m[7],    0,    0, gOneThird),
                         surfaceScale);
}

static inline SkPoint3 bottomLeftNormal(int m[9], SkScalar surfaceScale) {
    return pointToNormal(sobel(m[1], m[2], m[4], m[5],    0,    0, gTwoThirds),
                         sobel(   0,    0, m[1], m[4], m[2], m[5], gTwoThirds),
                         surfaceScale);
}

static inline SkPoint3 bottomNormal(int m[9], SkScalar surfaceScale) {
    return pointToNormal(sobel(m[0], m[2], m[3], m[5],    0,    0, gOneThird),
                         sobel(m[0], m[3], m[1], m[4], m[2], m[5], gOneHalf),
                         surfaceScale);
}

static inline SkPoint3 bottomRightNormal(int m[9], SkScalar surfaceScale) {
    return pointToNormal(sobel(m[0], m[1], m[3], m[4], 0,  0, gTwoThirds),
                         sobel(m[0], m[3], m[1], m[4], 0,  0, gTwoThirds),
                         surfaceScale);
}


class UncheckedPixelFetcher {
public:
    static inline uint32_t Fetch(const SkBitmap& src, int x, int y, const SkIRect& bounds) {
        return SkGetPackedA32(*src.getAddr32(x, y));
    }
};

// The DecalPixelFetcher is used when the destination crop rect exceeds the input bitmap bounds.
class DecalPixelFetcher {
public:
    static inline uint32_t Fetch(const SkBitmap& src, int x, int y, const SkIRect& bounds) {
        if (x < bounds.fLeft || x >= bounds.fRight || y < bounds.fTop || y >= bounds.fBottom) {
            return 0;
        } else {
            return SkGetPackedA32(*src.getAddr32(x, y));
        }
    }
};

template <class PixelFetcher>
static void lightBitmap(const BaseLightingType& lightingType,
                 const SkImageFilterLight* l,
                 const SkBitmap& src,
                 SkBitmap* dst,
                 SkScalar surfaceScale,
                 const SkIRect& bounds) {
    SkASSERT(dst->width() == bounds.width() && dst->height() == bounds.height());
    int left = bounds.left(), right = bounds.right();
    int bottom = bounds.bottom();
    int y = bounds.top();
    SkIRect srcBounds = src.bounds();
    SkPMColor* dptr = dst->getAddr32(0, 0);
    {
        int x = left;
        int m[9];
        m[4] = PixelFetcher::Fetch(src, x,     y,     srcBounds);
        m[5] = PixelFetcher::Fetch(src, x + 1, y,     srcBounds);
        m[7] = PixelFetcher::Fetch(src, x,     y + 1, srcBounds);
        m[8] = PixelFetcher::Fetch(src, x + 1, y + 1, srcBounds);
        SkPoint3 surfaceToLight = l->surfaceToLight(x, y, m[4], surfaceScale);
        *dptr++ = lightingType.light(topLeftNormal(m, surfaceScale), surfaceToLight,
                                     l->lightColor(surfaceToLight));
        for (++x; x < right - 1; ++x)
        {
            shiftMatrixLeft(m);
            m[5] = PixelFetcher::Fetch(src, x + 1, y,     srcBounds);
            m[8] = PixelFetcher::Fetch(src, x + 1, y + 1, srcBounds);
            surfaceToLight = l->surfaceToLight(x, y, m[4], surfaceScale);
            *dptr++ = lightingType.light(topNormal(m, surfaceScale), surfaceToLight,
                                         l->lightColor(surfaceToLight));
        }
        shiftMatrixLeft(m);
        surfaceToLight = l->surfaceToLight(x, y, m[4], surfaceScale);
        *dptr++ = lightingType.light(topRightNormal(m, surfaceScale), surfaceToLight,
                                     l->lightColor(surfaceToLight));
    }

    for (++y; y < bottom - 1; ++y) {
        int x = left;
        int m[9];
        m[1] = PixelFetcher::Fetch(src, x,     y - 1, srcBounds);
        m[2] = PixelFetcher::Fetch(src, x + 1, y - 1, srcBounds);
        m[4] = PixelFetcher::Fetch(src, x,     y,     srcBounds);
        m[5] = PixelFetcher::Fetch(src, x + 1, y,     srcBounds);
        m[7] = PixelFetcher::Fetch(src, x,     y + 1, srcBounds);
        m[8] = PixelFetcher::Fetch(src, x + 1, y + 1, srcBounds);
        SkPoint3 surfaceToLight = l->surfaceToLight(x, y, m[4], surfaceScale);
        *dptr++ = lightingType.light(leftNormal(m, surfaceScale), surfaceToLight,
                                     l->lightColor(surfaceToLight));
        for (++x; x < right - 1; ++x) {
            shiftMatrixLeft(m);
            m[2] = PixelFetcher::Fetch(src, x + 1, y - 1, srcBounds);
            m[5] = PixelFetcher::Fetch(src, x + 1, y,     srcBounds);
            m[8] = PixelFetcher::Fetch(src, x + 1, y + 1, srcBounds);
            surfaceToLight = l->surfaceToLight(x, y, m[4], surfaceScale);
            *dptr++ = lightingType.light(interiorNormal(m, surfaceScale), surfaceToLight,
                                         l->lightColor(surfaceToLight));
        }
        shiftMatrixLeft(m);
        surfaceToLight = l->surfaceToLight(x, y, m[4], surfaceScale);
        *dptr++ = lightingType.light(rightNormal(m, surfaceScale), surfaceToLight,
                                     l->lightColor(surfaceToLight));
    }

    {
        int x = left;
        int m[9];
        m[1] = PixelFetcher::Fetch(src, x,     bottom - 2, srcBounds);
        m[2] = PixelFetcher::Fetch(src, x + 1, bottom - 2, srcBounds);
        m[4] = PixelFetcher::Fetch(src, x,     bottom - 1, srcBounds);
        m[5] = PixelFetcher::Fetch(src, x + 1, bottom - 1, srcBounds);
        SkPoint3 surfaceToLight = l->surfaceToLight(x, y, m[4], surfaceScale);
        *dptr++ = lightingType.light(bottomLeftNormal(m, surfaceScale), surfaceToLight,
                                     l->lightColor(surfaceToLight));
        for (++x; x < right - 1; ++x)
        {
            shiftMatrixLeft(m);
            m[2] = PixelFetcher::Fetch(src, x + 1, bottom - 2, srcBounds);
            m[5] = PixelFetcher::Fetch(src, x + 1, bottom - 1, srcBounds);
            surfaceToLight = l->surfaceToLight(x, y, m[4], surfaceScale);
            *dptr++ = lightingType.light(bottomNormal(m, surfaceScale), surfaceToLight,
                                         l->lightColor(surfaceToLight));
        }
        shiftMatrixLeft(m);
        surfaceToLight = l->surfaceToLight(x, y, m[4], surfaceScale);
        *dptr++ = lightingType.light(bottomRightNormal(m, surfaceScale), surfaceToLight,
                                     l->lightColor(surfaceToLight));
    }
}

static void lightBitmap(const BaseLightingType& lightingType,
                 const SkImageFilterLight* light,
                 const SkBitmap& src,
                 SkBitmap* dst,
                 SkScalar surfaceScale,
                 const SkIRect& bounds) {
    if (src.bounds().contains(bounds)) {
        lightBitmap<UncheckedPixelFetcher>(
            lightingType, light, src, dst, surfaceScale, bounds);
    } else {
        lightBitmap<DecalPixelFetcher>(
            lightingType, light, src, dst, surfaceScale, bounds);
    }
}

enum BoundaryMode {
    kTopLeft_BoundaryMode,
    kTop_BoundaryMode,
    kTopRight_BoundaryMode,
    kLeft_BoundaryMode,
    kInterior_BoundaryMode,
    kRight_BoundaryMode,
    kBottomLeft_BoundaryMode,
    kBottom_BoundaryMode,
    kBottomRight_BoundaryMode,

    kBoundaryModeCount,
};

class SkLightingImageFilterInternal : public SkImageFilter_Base {
protected:
    SkLightingImageFilterInternal(sk_sp<SkImageFilterLight> light,
                                  SkScalar surfaceScale,
                                  sk_sp<SkImageFilter> input,
                                  const CropRect* cropRect)
            : INHERITED(&input, 1, cropRect)
            , fLight(std::move(light))
            , fSurfaceScale(surfaceScale / 255) {}

    void flatten(SkWriteBuffer& buffer) const override {
        this->INHERITED::flatten(buffer);
        fLight->flattenLight(buffer);
        buffer.writeScalar(fSurfaceScale * 255);
    }

    bool affectsTransparentBlack() const override { return true; }

    const SkImageFilterLight* light() const { return fLight.get(); }
    inline sk_sp<const SkImageFilterLight> refLight() const { return fLight; }
    SkScalar surfaceScale() const { return fSurfaceScale; }

#if SK_SUPPORT_GPU
    sk_sp<SkSpecialImage> filterImageGPU(const Context& ctx,
                                         SkSpecialImage* input,
                                         const SkIRect& bounds,
                                         const SkMatrix& matrix) const;
    virtual std::unique_ptr<GrFragmentProcessor> makeFragmentProcessor(GrSurfaceProxyView,
                                                                       const SkMatrix&,
                                                                       const SkIRect* srcBounds,
                                                                       BoundaryMode boundaryMode,
                                                                       const GrCaps&) const = 0;
#endif

private:
#if SK_SUPPORT_GPU
    void drawRect(GrRenderTargetContext*,
                  GrSurfaceProxyView srcView,
                  const SkMatrix& matrix,
                  const GrClip& clip,
                  const SkRect& dstRect,
                  BoundaryMode boundaryMode,
                  const SkIRect* srcBounds,
                  const SkIRect& bounds) const;
#endif

    sk_sp<SkImageFilterLight> fLight;
    SkScalar fSurfaceScale;

    typedef SkImageFilter_Base INHERITED;
};

#if SK_SUPPORT_GPU
void SkLightingImageFilterInternal::drawRect(GrRenderTargetContext* renderTargetContext,
                                             GrSurfaceProxyView srcView,
                                             const SkMatrix& matrix,
                                             const GrClip& clip,
                                             const SkRect& dstRect,
                                             BoundaryMode boundaryMode,
                                             const SkIRect* srcBounds,
                                             const SkIRect& bounds) const {
    SkRect srcRect = dstRect.makeOffset(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()));
    GrPaint paint;
    auto fp = this->makeFragmentProcessor(std::move(srcView), matrix, srcBounds, boundaryMode,
                                          *renderTargetContext->caps());
    paint.addColorFragmentProcessor(std::move(fp));
    paint.setPorterDuffXPFactory(SkBlendMode::kSrc);
    renderTargetContext->fillRectToRect(clip, std::move(paint), GrAA::kNo, SkMatrix::I(), dstRect,
                                        srcRect);
}

sk_sp<SkSpecialImage> SkLightingImageFilterInternal::filterImageGPU(
                                                   const Context& ctx,
                                                   SkSpecialImage* input,
                                                   const SkIRect& offsetBounds,
                                                   const SkMatrix& matrix) const {
    SkASSERT(ctx.gpuBacked());

    auto context = ctx.getContext();

    GrSurfaceProxyView inputView = input->view(context);
    SkASSERT(inputView.asTextureProxy());

    auto renderTargetContext = GrRenderTargetContext::Make(
            context, ctx.grColorType(), ctx.refColorSpace(), SkBackingFit::kApprox,
            offsetBounds.size(), 1, GrMipMapped::kNo, inputView.proxy()->isProtected(),
            kBottomLeft_GrSurfaceOrigin);
    if (!renderTargetContext) {
        return nullptr;
    }

    SkIRect dstIRect = SkIRect::MakeWH(offsetBounds.width(), offsetBounds.height());
    SkRect dstRect = SkRect::Make(dstIRect);

    // setup new clip
    GrFixedClip clip(dstIRect);

    const SkIRect inputBounds = SkIRect::MakeWH(input->width(), input->height());
    SkRect topLeft = SkRect::MakeXYWH(0, 0, 1, 1);
    SkRect top = SkRect::MakeXYWH(1, 0, dstRect.width() - 2, 1);
    SkRect topRight = SkRect::MakeXYWH(dstRect.width() - 1, 0, 1, 1);
    SkRect left = SkRect::MakeXYWH(0, 1, 1, dstRect.height() - 2);
    SkRect interior = dstRect.makeInset(1, 1);
    SkRect right = SkRect::MakeXYWH(dstRect.width() - 1, 1, 1, dstRect.height() - 2);
    SkRect bottomLeft = SkRect::MakeXYWH(0, dstRect.height() - 1, 1, 1);
    SkRect bottom = SkRect::MakeXYWH(1, dstRect.height() - 1, dstRect.width() - 2, 1);
    SkRect bottomRight = SkRect::MakeXYWH(dstRect.width() - 1, dstRect.height() - 1, 1, 1);

    const SkIRect* pSrcBounds = inputBounds.contains(offsetBounds) ? nullptr : &inputBounds;
    this->drawRect(renderTargetContext.get(), inputView, matrix, clip, topLeft,
                   kTopLeft_BoundaryMode, pSrcBounds, offsetBounds);
    this->drawRect(renderTargetContext.get(), inputView, matrix, clip, top,
                   kTop_BoundaryMode, pSrcBounds, offsetBounds);
    this->drawRect(renderTargetContext.get(), inputView, matrix, clip, topRight,
                   kTopRight_BoundaryMode, pSrcBounds, offsetBounds);
    this->drawRect(renderTargetContext.get(), inputView, matrix, clip, left,
                   kLeft_BoundaryMode, pSrcBounds, offsetBounds);
    this->drawRect(renderTargetContext.get(), inputView, matrix, clip, interior,
                   kInterior_BoundaryMode, pSrcBounds, offsetBounds);
    this->drawRect(renderTargetContext.get(), inputView, matrix, clip, right,
                   kRight_BoundaryMode, pSrcBounds, offsetBounds);
    this->drawRect(renderTargetContext.get(), inputView, matrix, clip, bottomLeft,
                   kBottomLeft_BoundaryMode, pSrcBounds, offsetBounds);
    this->drawRect(renderTargetContext.get(), inputView, matrix, clip, bottom,
                   kBottom_BoundaryMode, pSrcBounds, offsetBounds);
    this->drawRect(renderTargetContext.get(), std::move(inputView), matrix, clip, bottomRight,
                   kBottomRight_BoundaryMode, pSrcBounds, offsetBounds);

    return SkSpecialImage::MakeDeferredFromGpu(
            context,
            SkIRect::MakeWH(offsetBounds.width(), offsetBounds.height()),
            kNeedNewImageUniqueID_SpecialImage,
            renderTargetContext->readSurfaceView(),
            renderTargetContext->colorInfo().colorType(),
            renderTargetContext->colorInfo().refColorSpace());
}
#endif

class SkDiffuseLightingImageFilter : public SkLightingImageFilterInternal {
public:
    static sk_sp<SkImageFilter> Make(sk_sp<SkImageFilterLight> light,
                                     SkScalar surfaceScale,
                                     SkScalar kd,
                                     sk_sp<SkImageFilter>,
                                     const CropRect*);

    SkScalar kd() const { return fKD; }

protected:
    SkDiffuseLightingImageFilter(sk_sp<SkImageFilterLight> light, SkScalar surfaceScale,
                                 SkScalar kd,
                                 sk_sp<SkImageFilter> input, const CropRect* cropRect);
    void flatten(SkWriteBuffer& buffer) const override;

    sk_sp<SkSpecialImage> onFilterImage(const Context&, SkIPoint* offset) const override;

#if SK_SUPPORT_GPU
    std::unique_ptr<GrFragmentProcessor> makeFragmentProcessor(GrSurfaceProxyView,
                                                               const SkMatrix&,
                                                               const SkIRect* bounds,
                                                               BoundaryMode,
                                                               const GrCaps&) const override;
#endif

private:
    SK_FLATTENABLE_HOOKS(SkDiffuseLightingImageFilter)
    friend class SkLightingImageFilter;
    SkScalar fKD;

    typedef SkLightingImageFilterInternal INHERITED;
};

class SkSpecularLightingImageFilter : public SkLightingImageFilterInternal {
public:
    static sk_sp<SkImageFilter> Make(sk_sp<SkImageFilterLight> light,
                                     SkScalar surfaceScale,
                                     SkScalar ks, SkScalar shininess,
                                     sk_sp<SkImageFilter>, const CropRect*);

    SkScalar ks() const { return fKS; }
    SkScalar shininess() const { return fShininess; }

protected:
    SkSpecularLightingImageFilter(sk_sp<SkImageFilterLight> light,
                                  SkScalar surfaceScale, SkScalar ks,
                                  SkScalar shininess,
                                  sk_sp<SkImageFilter> input, const CropRect*);
    void flatten(SkWriteBuffer& buffer) const override;

    sk_sp<SkSpecialImage> onFilterImage(const Context&, SkIPoint* offset) const override;

#if SK_SUPPORT_GPU
    std::unique_ptr<GrFragmentProcessor> makeFragmentProcessor(GrSurfaceProxyView,
                                                               const SkMatrix&,
                                                               const SkIRect* bounds,
                                                               BoundaryMode,
                                                               const GrCaps&) const override;
#endif

private:
    SK_FLATTENABLE_HOOKS(SkSpecularLightingImageFilter)

    SkScalar fKS;
    SkScalar fShininess;
    friend class SkLightingImageFilter;
    typedef SkLightingImageFilterInternal INHERITED;
};

#if SK_SUPPORT_GPU

class GrLightingEffect : public GrFragmentProcessor {
public:
    const SkImageFilterLight* light() const { return fLight.get(); }
    SkScalar surfaceScale() const { return fSurfaceScale; }
    const SkMatrix& filterMatrix() const { return fFilterMatrix; }
    BoundaryMode boundaryMode() const { return fBoundaryMode; }

protected:
    GrLightingEffect(ClassID classID,
                     GrSurfaceProxyView,
                     sk_sp<const SkImageFilterLight> light,
                     SkScalar surfaceScale,
                     const SkMatrix& matrix,
                     BoundaryMode boundaryMode,
                     const SkIRect* srcBounds,
                     const GrCaps& caps);

    explicit GrLightingEffect(const GrLightingEffect& that);

    bool onIsEqual(const GrFragmentProcessor&) const override;

private:
    // We really just want the unaltered local coords, but the only way to get that right now is
    // an identity coord transform.
    GrCoordTransform fCoordTransform = {};
    sk_sp<const SkImageFilterLight> fLight;
    SkScalar fSurfaceScale;
    SkMatrix fFilterMatrix;
    BoundaryMode fBoundaryMode;

    typedef GrFragmentProcessor INHERITED;
};

class GrDiffuseLightingEffect : public GrLightingEffect {
public:
    static std::unique_ptr<GrFragmentProcessor> Make(GrSurfaceProxyView view,
                                                     sk_sp<const SkImageFilterLight> light,
                                                     SkScalar surfaceScale,
                                                     const SkMatrix& matrix,
                                                     SkScalar kd,
                                                     BoundaryMode boundaryMode,
                                                     const SkIRect* srcBounds,
                                                     const GrCaps& caps) {
        return std::unique_ptr<GrFragmentProcessor>(
                new GrDiffuseLightingEffect(std::move(view), std::move(light), surfaceScale, matrix,
                                            kd, boundaryMode, srcBounds, caps));
    }

    const char* name() const override { return "DiffuseLighting"; }

    std::unique_ptr<GrFragmentProcessor> clone() const override {
        return std::unique_ptr<GrFragmentProcessor>(new GrDiffuseLightingEffect(*this));
    }

    SkScalar kd() const { return fKD; }

private:
    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override;

    void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override;

    bool onIsEqual(const GrFragmentProcessor&) const override;

    GrDiffuseLightingEffect(GrSurfaceProxyView view,
                            sk_sp<const SkImageFilterLight> light,
                            SkScalar surfaceScale,
                            const SkMatrix& matrix,
                            SkScalar kd,
                            BoundaryMode boundaryMode,
                            const SkIRect* srcBounds,
                            const GrCaps& caps);

    explicit GrDiffuseLightingEffect(const GrDiffuseLightingEffect& that);

    GR_DECLARE_FRAGMENT_PROCESSOR_TEST
    SkScalar fKD;

    typedef GrLightingEffect INHERITED;
};

class GrSpecularLightingEffect : public GrLightingEffect {
public:
    static std::unique_ptr<GrFragmentProcessor> Make(GrSurfaceProxyView view,
                                                     sk_sp<const SkImageFilterLight> light,
                                                     SkScalar surfaceScale,
                                                     const SkMatrix& matrix,
                                                     SkScalar ks,
                                                     SkScalar shininess,
                                                     BoundaryMode boundaryMode,
                                                     const SkIRect* srcBounds,
                                                     const GrCaps& caps) {
        return std::unique_ptr<GrFragmentProcessor>(
                new GrSpecularLightingEffect(std::move(view), std::move(light), surfaceScale,
                                             matrix, ks, shininess, boundaryMode, srcBounds, caps));
    }

    const char* name() const override { return "SpecularLighting"; }

    std::unique_ptr<GrFragmentProcessor> clone() const override {
        return std::unique_ptr<GrFragmentProcessor>(new GrSpecularLightingEffect(*this));
    }

    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override;

    SkScalar ks() const { return fKS; }
    SkScalar shininess() const { return fShininess; }

private:
    void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override;

    bool onIsEqual(const GrFragmentProcessor&) const override;

    GrSpecularLightingEffect(GrSurfaceProxyView,
                             sk_sp<const SkImageFilterLight> light,
                             SkScalar surfaceScale,
                             const SkMatrix& matrix,
                             SkScalar ks,
                             SkScalar shininess,
                             BoundaryMode boundaryMode,
                             const SkIRect* srcBounds,
                             const GrCaps&);

    explicit GrSpecularLightingEffect(const GrSpecularLightingEffect&);

    GR_DECLARE_FRAGMENT_PROCESSOR_TEST
    SkScalar fKS;
    SkScalar fShininess;

    typedef GrLightingEffect INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

class GrGLLight {
public:
    virtual ~GrGLLight() {}

    /**
     * This is called by GrGLLightingEffect::emitCode() before either of the two virtual functions
     * below. It adds a half3 uniform visible in the FS that represents the constant light color.
     */
    void emitLightColorUniform(const GrFragmentProcessor*, GrGLSLUniformHandler*);

    /**
     * These two functions are called from GrGLLightingEffect's emitCode() function.
     * emitSurfaceToLight places an expression in param out that is the vector from the surface to
     * the light. The expression will be used in the FS. emitLightColor writes an expression into
     * the FS that is the color of the light. Either function may add functions and/or uniforms to
     * the FS. The default of emitLightColor appends the name of the constant light color uniform
     * and so this function only needs to be overridden if the light color varies spatially.
     */
    virtual void emitSurfaceToLight(const GrFragmentProcessor*,
                                    GrGLSLUniformHandler*,
                                    GrGLSLFPFragmentBuilder*,
                                    const char* z) = 0;
    virtual void emitLightColor(const GrFragmentProcessor*,
                                GrGLSLUniformHandler*,
                                GrGLSLFPFragmentBuilder*,
                                const char *surfaceToLight);

    // This is called from GrGLLightingEffect's setData(). Subclasses of GrGLLight must call
    // INHERITED::setData().
    virtual void setData(const GrGLSLProgramDataManager&, const SkImageFilterLight* light) const;

protected:
    /**
     * Gets the constant light color uniform. Subclasses can use this in their emitLightColor
     * function.
     */
    UniformHandle lightColorUni() const { return fColorUni; }

private:
    UniformHandle fColorUni;

    typedef SkRefCnt INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

class GrGLDistantLight : public GrGLLight {
public:
    ~GrGLDistantLight() override {}
    void setData(const GrGLSLProgramDataManager&, const SkImageFilterLight* light) const override;
    void emitSurfaceToLight(const GrFragmentProcessor*, GrGLSLUniformHandler*,
                            GrGLSLFPFragmentBuilder*, const char* z) override;

private:
    typedef GrGLLight INHERITED;
    UniformHandle fDirectionUni;
};

///////////////////////////////////////////////////////////////////////////////

class GrGLPointLight : public GrGLLight {
public:
    ~GrGLPointLight() override {}
    void setData(const GrGLSLProgramDataManager&, const SkImageFilterLight* light) const override;
    void emitSurfaceToLight(const GrFragmentProcessor*, GrGLSLUniformHandler*,
                            GrGLSLFPFragmentBuilder*, const char* z) override;

private:
    typedef GrGLLight INHERITED;
    UniformHandle fLocationUni;
};

///////////////////////////////////////////////////////////////////////////////

class GrGLSpotLight : public GrGLLight {
public:
    ~GrGLSpotLight() override {}
    void setData(const GrGLSLProgramDataManager&, const SkImageFilterLight* light) const override;
    void emitSurfaceToLight(const GrFragmentProcessor*, GrGLSLUniformHandler*,
                            GrGLSLFPFragmentBuilder*, const char* z) override;
    void emitLightColor(const GrFragmentProcessor*,
                        GrGLSLUniformHandler*,
                        GrGLSLFPFragmentBuilder*,
                        const char *surfaceToLight) override;

private:
    typedef GrGLLight INHERITED;

    SkString        fLightColorFunc;
    UniformHandle   fLocationUni;
    UniformHandle   fExponentUni;
    UniformHandle   fCosOuterConeAngleUni;
    UniformHandle   fCosInnerConeAngleUni;
    UniformHandle   fConeScaleUni;
    UniformHandle   fSUni;
};
#else

class GrGLLight;

#endif

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

class SkDistantLight : public SkImageFilterLight {
public:
    SkDistantLight(const SkPoint3& direction, SkColor color)
      : INHERITED(color), fDirection(direction) {
    }

    SkPoint3 surfaceToLight(int x, int y, int z, SkScalar surfaceScale) const override {
        return fDirection;
    }
    SkPoint3 lightColor(const SkPoint3&) const override { return this->color(); }
    LightType type() const override { return kDistant_LightType; }
    const SkPoint3& direction() const { return fDirection; }
    GrGLLight* createGLLight() const override {
#if SK_SUPPORT_GPU
        return new GrGLDistantLight;
#else
        SkDEBUGFAIL("Should not call in GPU-less build");
        return nullptr;
#endif
    }

    bool isEqual(const SkImageFilterLight& other) const override {
        if (other.type() != kDistant_LightType) {
            return false;
        }

        const SkDistantLight& o = static_cast<const SkDistantLight&>(other);
        return INHERITED::isEqual(other) &&
               fDirection == o.fDirection;
    }

    SkDistantLight(SkReadBuffer& buffer) : INHERITED(buffer) {
        fDirection = read_point3(buffer);
    }

protected:
    SkDistantLight(const SkPoint3& direction, const SkPoint3& color)
      : INHERITED(color), fDirection(direction) {
    }
    SkImageFilterLight* transform(const SkMatrix& matrix) const override {
        return new SkDistantLight(direction(), color());
    }
    void onFlattenLight(SkWriteBuffer& buffer) const override {
        write_point3(fDirection, buffer);
    }

private:
    SkPoint3 fDirection;

    typedef SkImageFilterLight INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

class SkPointLight : public SkImageFilterLight {
public:
    SkPointLight(const SkPoint3& location, SkColor color)
     : INHERITED(color), fLocation(location) {}

    SkPoint3 surfaceToLight(int x, int y, int z, SkScalar surfaceScale) const override {
        SkPoint3 direction = SkPoint3::Make(fLocation.fX - SkIntToScalar(x),
                                            fLocation.fY - SkIntToScalar(y),
                                            fLocation.fZ - SkIntToScalar(z) * surfaceScale);
        fast_normalize(&direction);
        return direction;
    }
    SkPoint3 lightColor(const SkPoint3&) const override { return this->color(); }
    LightType type() const override { return kPoint_LightType; }
    const SkPoint3& location() const { return fLocation; }
    GrGLLight* createGLLight() const override {
#if SK_SUPPORT_GPU
        return new GrGLPointLight;
#else
        SkDEBUGFAIL("Should not call in GPU-less build");
        return nullptr;
#endif
    }

    bool isEqual(const SkImageFilterLight& other) const override {
        if (other.type() != kPoint_LightType) {
            return false;
        }
        const SkPointLight& o = static_cast<const SkPointLight&>(other);
        return INHERITED::isEqual(other) &&
               fLocation == o.fLocation;
    }
    SkImageFilterLight* transform(const SkMatrix& matrix) const override {
        SkPoint location2 = SkPoint::Make(fLocation.fX, fLocation.fY);
        matrix.mapPoints(&location2, 1);
        // Use X scale and Y scale on Z and average the result
        SkPoint locationZ = SkPoint::Make(fLocation.fZ, fLocation.fZ);
        matrix.mapVectors(&locationZ, 1);
        SkPoint3 location = SkPoint3::Make(location2.fX,
                                           location2.fY,
                                           SkScalarAve(locationZ.fX, locationZ.fY));
        return new SkPointLight(location, color());
    }

    SkPointLight(SkReadBuffer& buffer) : INHERITED(buffer) {
        fLocation = read_point3(buffer);
    }

protected:
    SkPointLight(const SkPoint3& location, const SkPoint3& color)
     : INHERITED(color), fLocation(location) {}
    void onFlattenLight(SkWriteBuffer& buffer) const override {
        write_point3(fLocation, buffer);
    }

private:
    SkPoint3 fLocation;

    typedef SkImageFilterLight INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

class SkSpotLight : public SkImageFilterLight {
public:
    SkSpotLight(const SkPoint3& location,
                const SkPoint3& target,
                SkScalar specularExponent,
                SkScalar cutoffAngle,
                SkColor color)
     : INHERITED(color),
       fLocation(location),
       fTarget(target),
       fSpecularExponent(SkTPin(specularExponent, kSpecularExponentMin, kSpecularExponentMax))
    {
       fS = target - location;
       fast_normalize(&fS);
       fCosOuterConeAngle = SkScalarCos(SkDegreesToRadians(cutoffAngle));
       const SkScalar antiAliasThreshold = 0.016f;
       fCosInnerConeAngle = fCosOuterConeAngle + antiAliasThreshold;
       fConeScale = SkScalarInvert(antiAliasThreshold);
    }

    SkImageFilterLight* transform(const SkMatrix& matrix) const override {
        SkPoint location2 = SkPoint::Make(fLocation.fX, fLocation.fY);
        matrix.mapPoints(&location2, 1);
        // Use X scale and Y scale on Z and average the result
        SkPoint locationZ = SkPoint::Make(fLocation.fZ, fLocation.fZ);
        matrix.mapVectors(&locationZ, 1);
        SkPoint3 location = SkPoint3::Make(location2.fX, location2.fY,
                                           SkScalarAve(locationZ.fX, locationZ.fY));
        SkPoint target2 = SkPoint::Make(fTarget.fX, fTarget.fY);
        matrix.mapPoints(&target2, 1);
        SkPoint targetZ = SkPoint::Make(fTarget.fZ, fTarget.fZ);
        matrix.mapVectors(&targetZ, 1);
        SkPoint3 target = SkPoint3::Make(target2.fX, target2.fY,
                                         SkScalarAve(targetZ.fX, targetZ.fY));
        SkPoint3 s = target - location;
        fast_normalize(&s);
        return new SkSpotLight(location,
                               target,
                               fSpecularExponent,
                               fCosOuterConeAngle,
                               fCosInnerConeAngle,
                               fConeScale,
                               s,
                               color());
    }

    SkPoint3 surfaceToLight(int x, int y, int z, SkScalar surfaceScale) const override {
        SkPoint3 direction = SkPoint3::Make(fLocation.fX - SkIntToScalar(x),
                                            fLocation.fY - SkIntToScalar(y),
                                            fLocation.fZ - SkIntToScalar(z) * surfaceScale);
        fast_normalize(&direction);
        return direction;
    }
    SkPoint3 lightColor(const SkPoint3& surfaceToLight) const override {
        SkScalar cosAngle = -surfaceToLight.dot(fS);
        SkScalar scale = 0;
        if (cosAngle >= fCosOuterConeAngle) {
            scale = SkScalarPow(cosAngle, fSpecularExponent);
            if (cosAngle < fCosInnerConeAngle) {
                scale *= (cosAngle - fCosOuterConeAngle) * fConeScale;
            }
        }
        return this->color().makeScale(scale);
    }
    GrGLLight* createGLLight() const override {
#if SK_SUPPORT_GPU
        return new GrGLSpotLight;
#else
        SkDEBUGFAIL("Should not call in GPU-less build");
        return nullptr;
#endif
    }
    LightType type() const override { return kSpot_LightType; }
    const SkPoint3& location() const { return fLocation; }
    const SkPoint3& target() const { return fTarget; }
    SkScalar specularExponent() const { return fSpecularExponent; }
    SkScalar cosInnerConeAngle() const { return fCosInnerConeAngle; }
    SkScalar cosOuterConeAngle() const { return fCosOuterConeAngle; }
    SkScalar coneScale() const { return fConeScale; }
    const SkPoint3& s() const { return fS; }

    SkSpotLight(SkReadBuffer& buffer) : INHERITED(buffer) {
        fLocation = read_point3(buffer);
        fTarget = read_point3(buffer);
        fSpecularExponent = buffer.readScalar();
        fCosOuterConeAngle = buffer.readScalar();
        fCosInnerConeAngle = buffer.readScalar();
        fConeScale = buffer.readScalar();
        fS = read_point3(buffer);
        buffer.validate(SkScalarIsFinite(fSpecularExponent) &&
                        SkScalarIsFinite(fCosOuterConeAngle) &&
                        SkScalarIsFinite(fCosInnerConeAngle) &&
                        SkScalarIsFinite(fConeScale));
    }
protected:
    SkSpotLight(const SkPoint3& location,
                const SkPoint3& target,
                SkScalar specularExponent,
                SkScalar cosOuterConeAngle,
                SkScalar cosInnerConeAngle,
                SkScalar coneScale,
                const SkPoint3& s,
                const SkPoint3& color)
     : INHERITED(color),
       fLocation(location),
       fTarget(target),
       fSpecularExponent(specularExponent),
       fCosOuterConeAngle(cosOuterConeAngle),
       fCosInnerConeAngle(cosInnerConeAngle),
       fConeScale(coneScale),
       fS(s)
    {
    }
    void onFlattenLight(SkWriteBuffer& buffer) const override {
        write_point3(fLocation, buffer);
        write_point3(fTarget, buffer);
        buffer.writeScalar(fSpecularExponent);
        buffer.writeScalar(fCosOuterConeAngle);
        buffer.writeScalar(fCosInnerConeAngle);
        buffer.writeScalar(fConeScale);
        write_point3(fS, buffer);
    }

    bool isEqual(const SkImageFilterLight& other) const override {
        if (other.type() != kSpot_LightType) {
            return false;
        }

        const SkSpotLight& o = static_cast<const SkSpotLight&>(other);
        return INHERITED::isEqual(other) &&
               fLocation == o.fLocation &&
               fTarget == o.fTarget &&
               fSpecularExponent == o.fSpecularExponent &&
               fCosOuterConeAngle == o.fCosOuterConeAngle;
    }

private:
    static const SkScalar kSpecularExponentMin;
    static const SkScalar kSpecularExponentMax;

    SkPoint3 fLocation;
    SkPoint3 fTarget;
    SkScalar fSpecularExponent;
    SkScalar fCosOuterConeAngle;
    SkScalar fCosInnerConeAngle;
    SkScalar fConeScale;
    SkPoint3 fS;

    typedef SkImageFilterLight INHERITED;
};

// According to the spec, the specular term should be in the range [1, 128] :
// http://www.w3.org/TR/SVG/filters.html#feSpecularLightingSpecularExponentAttribute
const SkScalar SkSpotLight::kSpecularExponentMin = 1.0f;
const SkScalar SkSpotLight::kSpecularExponentMax = 128.0f;

///////////////////////////////////////////////////////////////////////////////

void SkImageFilterLight::flattenLight(SkWriteBuffer& buffer) const {
    // Write type first, then baseclass, then subclass.
    buffer.writeInt(this->type());
    write_point3(fColor, buffer);
    this->onFlattenLight(buffer);
}

/*static*/ SkImageFilterLight* SkImageFilterLight::UnflattenLight(SkReadBuffer& buffer) {
    SkImageFilterLight::LightType type = buffer.read32LE(SkImageFilterLight::kLast_LightType);

    switch (type) {
        // Each of these constructors must first call SkLight's, so we'll read the baseclass
        // then subclass, same order as flattenLight.
        case SkImageFilterLight::kDistant_LightType:
            return new SkDistantLight(buffer);
        case SkImageFilterLight::kPoint_LightType:
            return new SkPointLight(buffer);
        case SkImageFilterLight::kSpot_LightType:
            return new SkSpotLight(buffer);
        default:
            // Should never get here due to prior check of SkSafeRange
            SkDEBUGFAIL("Unknown LightType.");
            return nullptr;
    }
}
///////////////////////////////////////////////////////////////////////////////

sk_sp<SkImageFilter> SkLightingImageFilter::MakeDistantLitDiffuse(
        const SkPoint3& direction, SkColor lightColor, SkScalar surfaceScale, SkScalar kd,
        sk_sp<SkImageFilter> input, const SkImageFilter::CropRect* cropRect) {
    sk_sp<SkImageFilterLight> light(new SkDistantLight(direction, lightColor));
    return SkDiffuseLightingImageFilter::Make(std::move(light), surfaceScale, kd,
                                              std::move(input), cropRect);
}

sk_sp<SkImageFilter> SkLightingImageFilter::MakePointLitDiffuse(
        const SkPoint3& location, SkColor lightColor, SkScalar surfaceScale, SkScalar kd,
        sk_sp<SkImageFilter> input, const SkImageFilter::CropRect* cropRect) {
    sk_sp<SkImageFilterLight> light(new SkPointLight(location, lightColor));
    return SkDiffuseLightingImageFilter::Make(std::move(light), surfaceScale, kd,
                                              std::move(input), cropRect);
}

sk_sp<SkImageFilter> SkLightingImageFilter::MakeSpotLitDiffuse(
        const SkPoint3& location, const SkPoint3& target, SkScalar specularExponent,
        SkScalar cutoffAngle, SkColor lightColor, SkScalar surfaceScale, SkScalar kd,
        sk_sp<SkImageFilter> input, const SkImageFilter::CropRect* cropRect) {
    sk_sp<SkImageFilterLight> light(
            new SkSpotLight(location, target, specularExponent, cutoffAngle, lightColor));
    return SkDiffuseLightingImageFilter::Make(std::move(light), surfaceScale, kd,
                                              std::move(input), cropRect);
}

sk_sp<SkImageFilter> SkLightingImageFilter::MakeDistantLitSpecular(
        const SkPoint3& direction,  SkColor lightColor,  SkScalar surfaceScale, SkScalar ks,
        SkScalar shininess, sk_sp<SkImageFilter> input, const SkImageFilter::CropRect* cropRect) {
    sk_sp<SkImageFilterLight> light(new SkDistantLight(direction, lightColor));
    return SkSpecularLightingImageFilter::Make(std::move(light), surfaceScale, ks, shininess,
                                               std::move(input), cropRect);
}

sk_sp<SkImageFilter> SkLightingImageFilter::MakePointLitSpecular(
        const SkPoint3& location, SkColor lightColor, SkScalar surfaceScale, SkScalar ks,
        SkScalar shininess, sk_sp<SkImageFilter> input, const SkImageFilter::CropRect* cropRect) {
    sk_sp<SkImageFilterLight> light(new SkPointLight(location, lightColor));
    return SkSpecularLightingImageFilter::Make(std::move(light), surfaceScale, ks, shininess,
                                               std::move(input), cropRect);
}

sk_sp<SkImageFilter> SkLightingImageFilter::MakeSpotLitSpecular(
        const SkPoint3& location, const SkPoint3& target, SkScalar specularExponent,
        SkScalar cutoffAngle, SkColor lightColor, SkScalar surfaceScale, SkScalar ks,
        SkScalar shininess, sk_sp<SkImageFilter> input, const SkImageFilter::CropRect* cropRect) {
    sk_sp<SkImageFilterLight> light(
            new SkSpotLight(location, target, specularExponent, cutoffAngle, lightColor));
    return SkSpecularLightingImageFilter::Make(std::move(light), surfaceScale, ks, shininess,
                                               std::move(input), cropRect);
}

///////////////////////////////////////////////////////////////////////////////

sk_sp<SkImageFilter> SkDiffuseLightingImageFilter::Make(sk_sp<SkImageFilterLight> light,
                                                        SkScalar surfaceScale,
                                                        SkScalar kd,
                                                        sk_sp<SkImageFilter> input,
                                                        const CropRect* cropRect) {
    if (!light) {
        return nullptr;
    }
    if (!SkScalarIsFinite(surfaceScale) || !SkScalarIsFinite(kd)) {
        return nullptr;
    }
    // According to the spec, kd can be any non-negative number :
    // http://www.w3.org/TR/SVG/filters.html#feDiffuseLightingElement
    if (kd < 0) {
        return nullptr;
    }
    return sk_sp<SkImageFilter>(new SkDiffuseLightingImageFilter(std::move(light), surfaceScale,
                                                                 kd, std::move(input), cropRect));
}

SkDiffuseLightingImageFilter::SkDiffuseLightingImageFilter(sk_sp<SkImageFilterLight> light,
                                                           SkScalar surfaceScale,
                                                           SkScalar kd,
                                                           sk_sp<SkImageFilter> input,
                                                           const CropRect* cropRect)
    : INHERITED(std::move(light), surfaceScale, std::move(input), cropRect)
    , fKD(kd) {
}

sk_sp<SkFlattenable> SkDiffuseLightingImageFilter::CreateProc(SkReadBuffer& buffer) {
    SK_IMAGEFILTER_UNFLATTEN_COMMON(common, 1);

    sk_sp<SkImageFilterLight> light(SkImageFilterLight::UnflattenLight(buffer));
    SkScalar surfaceScale = buffer.readScalar();
    SkScalar kd = buffer.readScalar();

    return Make(std::move(light), surfaceScale, kd, common.getInput(0), &common.cropRect());
}

void SkDiffuseLightingImageFilter::flatten(SkWriteBuffer& buffer) const {
    this->INHERITED::flatten(buffer);
    buffer.writeScalar(fKD);
}

sk_sp<SkSpecialImage> SkDiffuseLightingImageFilter::onFilterImage(const Context& ctx,
                                                                  SkIPoint* offset) const {
    SkIPoint inputOffset = SkIPoint::Make(0, 0);
    sk_sp<SkSpecialImage> input(this->filterInput(0, ctx, &inputOffset));
    if (!input) {
        return nullptr;
    }

    const SkIRect inputBounds = SkIRect::MakeXYWH(inputOffset.x(), inputOffset.y(),
                                                  input->width(), input->height());
    SkIRect bounds;
    if (!this->applyCropRect(ctx, inputBounds, &bounds)) {
        return nullptr;
    }

    offset->fX = bounds.left();
    offset->fY = bounds.top();
    bounds.offset(-inputOffset);

#if SK_SUPPORT_GPU
    if (ctx.gpuBacked()) {
        SkMatrix matrix(ctx.ctm());
        matrix.postTranslate(SkIntToScalar(-offset->fX), SkIntToScalar(-offset->fY));

        return this->filterImageGPU(ctx, input.get(), bounds, matrix);
    }
#endif

    if (bounds.width() < 2 || bounds.height() < 2) {
        return nullptr;
    }

    SkBitmap inputBM;

    if (!input->getROPixels(&inputBM)) {
        return nullptr;
    }

    if (inputBM.colorType() != kN32_SkColorType) {
        return nullptr;
    }

    if (!inputBM.getPixels()) {
        return nullptr;
    }

    const SkImageInfo info = SkImageInfo::MakeN32Premul(bounds.width(), bounds.height());

    SkBitmap dst;
    if (!dst.tryAllocPixels(info)) {
        return nullptr;
    }

    SkMatrix matrix(ctx.ctm());
    matrix.postTranslate(SkIntToScalar(-inputOffset.x()), SkIntToScalar(-inputOffset.y()));

    sk_sp<SkImageFilterLight> transformedLight(light()->transform(matrix));

    DiffuseLightingType lightingType(fKD);
    lightBitmap(lightingType,
                                                             transformedLight.get(),
                                                             inputBM,
                                                             &dst,
                                                             surfaceScale(),
                                                             bounds);

    return SkSpecialImage::MakeFromRaster(SkIRect::MakeWH(bounds.width(), bounds.height()),
                                          dst);
}

#if SK_SUPPORT_GPU
std::unique_ptr<GrFragmentProcessor> SkDiffuseLightingImageFilter::makeFragmentProcessor(
        GrSurfaceProxyView view,
        const SkMatrix& matrix,
        const SkIRect* srcBounds,
        BoundaryMode boundaryMode,
        const GrCaps& caps) const {
    SkScalar scale = this->surfaceScale() * 255;
    return GrDiffuseLightingEffect::Make(std::move(view), this->refLight(), scale, matrix,
                                         this->kd(), boundaryMode, srcBounds, caps);
}
#endif

///////////////////////////////////////////////////////////////////////////////

sk_sp<SkImageFilter> SkSpecularLightingImageFilter::Make(sk_sp<SkImageFilterLight> light,
                                                         SkScalar surfaceScale,
                                                         SkScalar ks,
                                                         SkScalar shininess,
                                                         sk_sp<SkImageFilter> input,
                                                         const CropRect* cropRect) {
    if (!light) {
        return nullptr;
    }
    if (!SkScalarIsFinite(surfaceScale) || !SkScalarIsFinite(ks) || !SkScalarIsFinite(shininess)) {
        return nullptr;
    }
    // According to the spec, ks can be any non-negative number :
    // http://www.w3.org/TR/SVG/filters.html#feSpecularLightingElement
    if (ks < 0) {
        return nullptr;
    }
    return sk_sp<SkImageFilter>(new SkSpecularLightingImageFilter(std::move(light), surfaceScale,
                                                                  ks, shininess,
                                                                  std::move(input), cropRect));
}

SkSpecularLightingImageFilter::SkSpecularLightingImageFilter(sk_sp<SkImageFilterLight> light,
                                                             SkScalar surfaceScale,
                                                             SkScalar ks,
                                                             SkScalar shininess,
                                                             sk_sp<SkImageFilter> input,
                                                             const CropRect* cropRect)
    : INHERITED(std::move(light), surfaceScale, std::move(input), cropRect)
    , fKS(ks)
    , fShininess(shininess) {
}

sk_sp<SkFlattenable> SkSpecularLightingImageFilter::CreateProc(SkReadBuffer& buffer) {
    SK_IMAGEFILTER_UNFLATTEN_COMMON(common, 1);
    sk_sp<SkImageFilterLight> light(SkImageFilterLight::UnflattenLight(buffer));
    SkScalar surfaceScale = buffer.readScalar();
    SkScalar ks = buffer.readScalar();
    SkScalar shine = buffer.readScalar();

    return Make(std::move(light), surfaceScale, ks, shine, common.getInput(0),
                &common.cropRect());
}

void SkSpecularLightingImageFilter::flatten(SkWriteBuffer& buffer) const {
    this->INHERITED::flatten(buffer);
    buffer.writeScalar(fKS);
    buffer.writeScalar(fShininess);
}

sk_sp<SkSpecialImage> SkSpecularLightingImageFilter::onFilterImage(const Context& ctx,
                                                                   SkIPoint* offset) const {
    SkIPoint inputOffset = SkIPoint::Make(0, 0);
    sk_sp<SkSpecialImage> input(this->filterInput(0, ctx, &inputOffset));
    if (!input) {
        return nullptr;
    }

    const SkIRect inputBounds = SkIRect::MakeXYWH(inputOffset.x(), inputOffset.y(),
                                                  input->width(), input->height());
    SkIRect bounds;
    if (!this->applyCropRect(ctx, inputBounds, &bounds)) {
        return nullptr;
    }

    offset->fX = bounds.left();
    offset->fY = bounds.top();
    bounds.offset(-inputOffset);

#if SK_SUPPORT_GPU
    if (ctx.gpuBacked()) {
        SkMatrix matrix(ctx.ctm());
        matrix.postTranslate(SkIntToScalar(-offset->fX), SkIntToScalar(-offset->fY));

        return this->filterImageGPU(ctx, input.get(), bounds, matrix);
    }
#endif

    if (bounds.width() < 2 || bounds.height() < 2) {
        return nullptr;
    }

    SkBitmap inputBM;

    if (!input->getROPixels(&inputBM)) {
        return nullptr;
    }

    if (inputBM.colorType() != kN32_SkColorType) {
        return nullptr;
    }

    if (!inputBM.getPixels()) {
        return nullptr;
    }

    const SkImageInfo info = SkImageInfo::MakeN32Premul(bounds.width(), bounds.height());

    SkBitmap dst;
    if (!dst.tryAllocPixels(info)) {
        return nullptr;
    }

    SpecularLightingType lightingType(fKS, fShininess);

    SkMatrix matrix(ctx.ctm());
    matrix.postTranslate(SkIntToScalar(-inputOffset.x()), SkIntToScalar(-inputOffset.y()));

    sk_sp<SkImageFilterLight> transformedLight(light()->transform(matrix));

    lightBitmap(lightingType,
                                                              transformedLight.get(),
                                                              inputBM,
                                                              &dst,
                                                              surfaceScale(),
                                                              bounds);

    return SkSpecialImage::MakeFromRaster(SkIRect::MakeWH(bounds.width(), bounds.height()), dst);
}

#if SK_SUPPORT_GPU
std::unique_ptr<GrFragmentProcessor> SkSpecularLightingImageFilter::makeFragmentProcessor(
        GrSurfaceProxyView view,
        const SkMatrix& matrix,
        const SkIRect* srcBounds,
        BoundaryMode boundaryMode,
        const GrCaps& caps) const {
    SkScalar scale = this->surfaceScale() * 255;
    return GrSpecularLightingEffect::Make(std::move(view), this->refLight(), scale, matrix,
                                          this->ks(), this->shininess(), boundaryMode, srcBounds,
                                          caps);
}
#endif

///////////////////////////////////////////////////////////////////////////////

#if SK_SUPPORT_GPU

static SkString emitNormalFunc(BoundaryMode mode,
                               const char* pointToNormalName,
                               const char* sobelFuncName) {
    SkString result;
    switch (mode) {
    case kTopLeft_BoundaryMode:
        result.printf("\treturn %s(%s(0.0, 0.0, m[4], m[5], m[7], m[8], %g),\n"
                      "\t          %s(0.0, 0.0, m[4], m[7], m[5], m[8], %g),\n"
                      "\t          surfaceScale);\n",
                      pointToNormalName, sobelFuncName, gTwoThirds,
                                         sobelFuncName, gTwoThirds);
        break;
    case kTop_BoundaryMode:
        result.printf("\treturn %s(%s(0.0, 0.0, m[3], m[5], m[6], m[8], %g),\n"
                      "\t          %s(0.0, 0.0, m[4], m[7], m[5], m[8], %g),\n"
                      "\t          surfaceScale);\n",
                      pointToNormalName, sobelFuncName, gOneThird,
                                         sobelFuncName, gOneHalf);
        break;
    case kTopRight_BoundaryMode:
        result.printf("\treturn %s(%s( 0.0,  0.0, m[3], m[4], m[6], m[7], %g),\n"
                      "\t          %s(m[3], m[6], m[4], m[7],  0.0,  0.0, %g),\n"
                      "\t          surfaceScale);\n",
                      pointToNormalName, sobelFuncName, gTwoThirds,
                                         sobelFuncName, gTwoThirds);
        break;
    case kLeft_BoundaryMode:
        result.printf("\treturn %s(%s(m[1], m[2], m[4], m[5], m[7], m[8], %g),\n"
                      "\t          %s( 0.0,  0.0, m[1], m[7], m[2], m[8], %g),\n"
                      "\t          surfaceScale);\n",
                      pointToNormalName, sobelFuncName, gOneHalf,
                                         sobelFuncName, gOneThird);
        break;
    case kInterior_BoundaryMode:
        result.printf("\treturn %s(%s(m[0], m[2], m[3], m[5], m[6], m[8], %g),\n"
                      "\t          %s(m[0], m[6], m[1], m[7], m[2], m[8], %g),\n"
                      "\t          surfaceScale);\n",
                      pointToNormalName, sobelFuncName, gOneQuarter,
                                         sobelFuncName, gOneQuarter);
        break;
    case kRight_BoundaryMode:
        result.printf("\treturn %s(%s(m[0], m[1], m[3], m[4], m[6], m[7], %g),\n"
                      "\t          %s(m[0], m[6], m[1], m[7],  0.0,  0.0, %g),\n"
                      "\t          surfaceScale);\n",
                      pointToNormalName, sobelFuncName, gOneHalf,
                                         sobelFuncName, gOneThird);
        break;
    case kBottomLeft_BoundaryMode:
        result.printf("\treturn %s(%s(m[1], m[2], m[4], m[5],  0.0,  0.0, %g),\n"
                      "\t          %s( 0.0,  0.0, m[1], m[4], m[2], m[5], %g),\n"
                      "\t          surfaceScale);\n",
                      pointToNormalName, sobelFuncName, gTwoThirds,
                                         sobelFuncName, gTwoThirds);
        break;
    case kBottom_BoundaryMode:
        result.printf("\treturn %s(%s(m[0], m[2], m[3], m[5],  0.0,  0.0, %g),\n"
                      "\t          %s(m[0], m[3], m[1], m[4], m[2], m[5], %g),\n"
                      "\t          surfaceScale);\n",
                      pointToNormalName, sobelFuncName, gOneThird,
                                         sobelFuncName, gOneHalf);
        break;
    case kBottomRight_BoundaryMode:
        result.printf("\treturn %s(%s(m[0], m[1], m[3], m[4],  0.0,  0.0, %g),\n"
                      "\t          %s(m[0], m[3], m[1], m[4],  0.0,  0.0, %g),\n"
                      "\t          surfaceScale);\n",
                      pointToNormalName, sobelFuncName, gTwoThirds,
                                         sobelFuncName, gTwoThirds);
        break;
    default:
        SkASSERT(false);
        break;
    }
    return result;
}

class GrGLLightingEffect : public GrGLSLFragmentProcessor {
public:
    GrGLLightingEffect() : fLight(nullptr) { }
    ~GrGLLightingEffect() override { delete fLight; }

    void emitCode(EmitArgs&) override;

    static inline void GenKey(const GrProcessor&, const GrShaderCaps&, GrProcessorKeyBuilder* b);

protected:
    /**
     * Subclasses of GrGLLightingEffect must call INHERITED::onSetData();
     */
    void onSetData(const GrGLSLProgramDataManager&, const GrFragmentProcessor&) override;

    virtual void emitLightFunc(const GrFragmentProcessor*,
                               GrGLSLUniformHandler*,
                               GrGLSLFPFragmentBuilder*,
                               SkString* funcName) = 0;

private:
    typedef GrGLSLFragmentProcessor INHERITED;

    UniformHandle              fSurfaceScaleUni;
    GrGLLight*                 fLight;
};

///////////////////////////////////////////////////////////////////////////////

class GrGLDiffuseLightingEffect  : public GrGLLightingEffect {
public:
    void emitLightFunc(const GrFragmentProcessor*, GrGLSLUniformHandler*, GrGLSLFPFragmentBuilder*,
                       SkString* funcName) override;

protected:
    void onSetData(const GrGLSLProgramDataManager&, const GrFragmentProcessor&) override;

private:
    typedef GrGLLightingEffect INHERITED;

    UniformHandle   fKDUni;
};

///////////////////////////////////////////////////////////////////////////////

class GrGLSpecularLightingEffect  : public GrGLLightingEffect {
public:
    void emitLightFunc(const GrFragmentProcessor*, GrGLSLUniformHandler*, GrGLSLFPFragmentBuilder*,
                       SkString* funcName) override;

protected:
    void onSetData(const GrGLSLProgramDataManager&, const GrFragmentProcessor&) override;

private:
    typedef GrGLLightingEffect INHERITED;

    UniformHandle   fKSUni;
    UniformHandle   fShininessUni;
};

///////////////////////////////////////////////////////////////////////////////

GrLightingEffect::GrLightingEffect(ClassID classID,
                                   GrSurfaceProxyView view,
                                   sk_sp<const SkImageFilterLight> light,
                                   SkScalar surfaceScale,
                                   const SkMatrix& matrix,
                                   BoundaryMode boundaryMode,
                                   const SkIRect* srcBounds,
                                   const GrCaps& caps)
        // Perhaps this could advertise the opaque or coverage-as-alpha optimizations?
        : INHERITED(classID, kNone_OptimizationFlags)
        , fLight(std::move(light))
        , fSurfaceScale(surfaceScale)
        , fFilterMatrix(matrix)
        , fBoundaryMode(boundaryMode) {
    static constexpr GrSamplerState kSampler(GrSamplerState::WrapMode::kClampToBorder,
                                             GrSamplerState::Filter::kNearest);
    std::unique_ptr<GrFragmentProcessor> child;
    if (srcBounds) {
        child = GrTextureEffect::MakeSubset(std::move(view), kPremul_SkAlphaType, SkMatrix::I(),
                                            kSampler, SkRect::Make(*srcBounds), caps);
    } else {
        child = GrTextureEffect::Make(std::move(view), kPremul_SkAlphaType, SkMatrix::I(), kSampler,
                                      caps);
    }
    child->setSampledWithExplicitCoords();
    this->registerChildProcessor(std::move(child));
    this->addCoordTransform(&fCoordTransform);
}

GrLightingEffect::GrLightingEffect(const GrLightingEffect& that)
        : INHERITED(that.classID(), that.optimizationFlags())
        , fLight(that.fLight)
        , fSurfaceScale(that.fSurfaceScale)
        , fFilterMatrix(that.fFilterMatrix)
        , fBoundaryMode(that.fBoundaryMode) {
    auto child = that.childProcessor(0).clone();
    child->setSampledWithExplicitCoords();
    this->registerChildProcessor(std::move(child));
    this->addCoordTransform(&fCoordTransform);
}

bool GrLightingEffect::onIsEqual(const GrFragmentProcessor& sBase) const {
    const GrLightingEffect& s = sBase.cast<GrLightingEffect>();
    return fLight->isEqual(*s.fLight) &&
           fSurfaceScale == s.fSurfaceScale &&
           fBoundaryMode == s.fBoundaryMode;
}

///////////////////////////////////////////////////////////////////////////////

GrDiffuseLightingEffect::GrDiffuseLightingEffect(GrSurfaceProxyView view,
                                                 sk_sp<const SkImageFilterLight> light,
                                                 SkScalar surfaceScale,
                                                 const SkMatrix& matrix,
                                                 SkScalar kd,
                                                 BoundaryMode boundaryMode,
                                                 const SkIRect* srcBounds,
                                                 const GrCaps& caps)
        : INHERITED(kGrDiffuseLightingEffect_ClassID,
                    std::move(view),
                    std::move(light),
                    surfaceScale,
                    matrix,
                    boundaryMode,
                    srcBounds,
                    caps)
        , fKD(kd) {}

GrDiffuseLightingEffect::GrDiffuseLightingEffect(const GrDiffuseLightingEffect& that)
        : INHERITED(that), fKD(that.fKD) {}

bool GrDiffuseLightingEffect::onIsEqual(const GrFragmentProcessor& sBase) const {
    const GrDiffuseLightingEffect& s = sBase.cast<GrDiffuseLightingEffect>();
    return INHERITED::onIsEqual(sBase) && this->kd() == s.kd();
}

void GrDiffuseLightingEffect::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                                    GrProcessorKeyBuilder* b) const {
    GrGLDiffuseLightingEffect::GenKey(*this, caps, b);
}

GrGLSLFragmentProcessor* GrDiffuseLightingEffect::onCreateGLSLInstance() const {
    return new GrGLDiffuseLightingEffect;
}

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrDiffuseLightingEffect);

#if GR_TEST_UTILS

static SkPoint3 random_point3(SkRandom* random) {
    return SkPoint3::Make(SkScalarToFloat(random->nextSScalar1()),
                          SkScalarToFloat(random->nextSScalar1()),
                          SkScalarToFloat(random->nextSScalar1()));
}

static SkImageFilterLight* create_random_light(SkRandom* random) {
    int type = random->nextULessThan(3);
    switch (type) {
        case 0: {
            return new SkDistantLight(random_point3(random), random->nextU());
        }
        case 1: {
            return new SkPointLight(random_point3(random), random->nextU());
        }
        case 2: {
            return new SkSpotLight(random_point3(random), random_point3(random),
                                   random->nextUScalar1(), random->nextUScalar1(), random->nextU());
        }
        default:
            SK_ABORT("Unexpected value.");
    }
}

std::unique_ptr<GrFragmentProcessor> GrDiffuseLightingEffect::TestCreate(GrProcessorTestData* d) {
    auto [view, ct, at] = d->randomView();
    SkScalar surfaceScale = d->fRandom->nextSScalar1();
    SkScalar kd = d->fRandom->nextUScalar1();
    sk_sp<SkImageFilterLight> light(create_random_light(d->fRandom));
    SkMatrix matrix;
    for (int i = 0; i < 9; i++) {
        matrix[i] = d->fRandom->nextUScalar1();
    }

    uint32_t boundsX = d->fRandom->nextRangeU(0, view.width());
    uint32_t boundsY = d->fRandom->nextRangeU(0, view.height());
    uint32_t boundsW = d->fRandom->nextRangeU(0, view.width());
    uint32_t boundsH = d->fRandom->nextRangeU(0, view.height());
    SkIRect srcBounds = SkIRect::MakeXYWH(boundsX, boundsY, boundsW, boundsH);
    BoundaryMode mode = static_cast<BoundaryMode>(d->fRandom->nextU() % kBoundaryModeCount);

    return GrDiffuseLightingEffect::Make(std::move(view), std::move(light), surfaceScale, matrix,
                                         kd, mode, &srcBounds, *d->caps());
}
#endif


///////////////////////////////////////////////////////////////////////////////

void GrGLLightingEffect::emitCode(EmitArgs& args) {
    const GrLightingEffect& le = args.fFp.cast<GrLightingEffect>();
    if (!fLight) {
        fLight = le.light()->createGLLight();
    }

    GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;
    fSurfaceScaleUni = uniformHandler->addUniform(&le,
                                                  kFragment_GrShaderFlag,
                                                  kHalf_GrSLType, "SurfaceScale");
    fLight->emitLightColorUniform(&le, uniformHandler);
    GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
    SkString lightFunc;
    this->emitLightFunc(&le, uniformHandler, fragBuilder, &lightFunc);
    const GrShaderVar gSobelArgs[] =  {
        GrShaderVar("a", kHalf_GrSLType),
        GrShaderVar("b", kHalf_GrSLType),
        GrShaderVar("c", kHalf_GrSLType),
        GrShaderVar("d", kHalf_GrSLType),
        GrShaderVar("e", kHalf_GrSLType),
        GrShaderVar("f", kHalf_GrSLType),
        GrShaderVar("scale", kHalf_GrSLType),
    };
    SkString sobelFuncName;
    SkString coords2D = fragBuilder->ensureCoords2D(args.fTransformedCoords[0].fVaryingPoint,
                                                    args.fFp.sampleMatrix());

    fragBuilder->emitFunction(kHalf_GrSLType,
                              "sobel",
                              SK_ARRAY_COUNT(gSobelArgs),
                              gSobelArgs,
                              "\treturn (-a + b - 2.0 * c + 2.0 * d -e + f) * scale;\n",
                              &sobelFuncName);
    const GrShaderVar gPointToNormalArgs[] =  {
        GrShaderVar("x", kHalf_GrSLType),
        GrShaderVar("y", kHalf_GrSLType),
        GrShaderVar("scale", kHalf_GrSLType),
    };
    SkString pointToNormalName;
    fragBuilder->emitFunction(kHalf3_GrSLType,
                              "pointToNormal",
                              SK_ARRAY_COUNT(gPointToNormalArgs),
                              gPointToNormalArgs,
                              "\treturn normalize(half3(-x * scale, -y * scale, 1));\n",
                              &pointToNormalName);

    const GrShaderVar gInteriorNormalArgs[] =  {
        GrShaderVar("m", kHalf_GrSLType, 9),
        GrShaderVar("surfaceScale", kHalf_GrSLType),
    };
    SkString normalBody = emitNormalFunc(le.boundaryMode(),
                                         pointToNormalName.c_str(),
                                         sobelFuncName.c_str());
    SkString normalName;
    fragBuilder->emitFunction(kHalf3_GrSLType,
                              "normal",
                              SK_ARRAY_COUNT(gInteriorNormalArgs),
                              gInteriorNormalArgs,
                              normalBody.c_str(),
                              &normalName);

    fragBuilder->codeAppendf("\t\tfloat2 coord = %s;\n", coords2D.c_str());
    fragBuilder->codeAppend("\t\thalf m[9];\n");

    const char* surfScale = uniformHandler->getUniformCStr(fSurfaceScaleUni);

    int index = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            SkString texCoords;
            texCoords.appendf("coord + half2(%d, %d)", dx, dy);
            auto sample = this->invokeChild(0, args, texCoords.c_str());
            fragBuilder->codeAppendf("m[%d] = %s.a;", index, sample.c_str());
            index++;
        }
    }
    fragBuilder->codeAppend("\t\thalf3 surfaceToLight = ");
    SkString arg;
    arg.appendf("%s * m[4]", surfScale);
    fLight->emitSurfaceToLight(&le, uniformHandler, fragBuilder, arg.c_str());
    fragBuilder->codeAppend(";\n");
    fragBuilder->codeAppendf("\t\t%s = %s(%s(m, %s), surfaceToLight, ",
                             args.fOutputColor, lightFunc.c_str(), normalName.c_str(), surfScale);
    fLight->emitLightColor(&le, uniformHandler, fragBuilder, "surfaceToLight");
    fragBuilder->codeAppend(");\n");
    fragBuilder->codeAppendf("%s *= %s;\n", args.fOutputColor, args.fInputColor);
}

void GrGLLightingEffect::GenKey(const GrProcessor& proc,
                                const GrShaderCaps& caps, GrProcessorKeyBuilder* b) {
    const GrLightingEffect& lighting = proc.cast<GrLightingEffect>();
    b->add32(lighting.boundaryMode() << 2 | lighting.light()->type());
}

void GrGLLightingEffect::onSetData(const GrGLSLProgramDataManager& pdman,
                                   const GrFragmentProcessor& proc) {
    const GrLightingEffect& lighting = proc.cast<GrLightingEffect>();
    if (!fLight) {
        fLight = lighting.light()->createGLLight();
    }

    pdman.set1f(fSurfaceScaleUni, lighting.surfaceScale());
    sk_sp<SkImageFilterLight> transformedLight(
            lighting.light()->transform(lighting.filterMatrix()));
    fLight->setData(pdman, transformedLight.get());
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

void GrGLDiffuseLightingEffect::emitLightFunc(const GrFragmentProcessor* owner,
                                              GrGLSLUniformHandler* uniformHandler,
                                              GrGLSLFPFragmentBuilder* fragBuilder,
                                              SkString* funcName) {
    const char* kd;
    fKDUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag, kHalf_GrSLType, "KD", &kd);

    const GrShaderVar gLightArgs[] = {
        GrShaderVar("normal", kHalf3_GrSLType),
        GrShaderVar("surfaceToLight", kHalf3_GrSLType),
        GrShaderVar("lightColor", kHalf3_GrSLType)
    };
    SkString lightBody;
    lightBody.appendf("\thalf colorScale = %s * dot(normal, surfaceToLight);\n", kd);
    lightBody.appendf("\treturn half4(lightColor * saturate(colorScale), 1.0);\n");
    fragBuilder->emitFunction(kHalf4_GrSLType,
                              "light",
                              SK_ARRAY_COUNT(gLightArgs),
                              gLightArgs,
                              lightBody.c_str(),
                              funcName);
}

void GrGLDiffuseLightingEffect::onSetData(const GrGLSLProgramDataManager& pdman,
                                          const GrFragmentProcessor& proc) {
    INHERITED::onSetData(pdman, proc);
    const GrDiffuseLightingEffect& diffuse = proc.cast<GrDiffuseLightingEffect>();
    pdman.set1f(fKDUni, diffuse.kd());
}

///////////////////////////////////////////////////////////////////////////////

GrSpecularLightingEffect::GrSpecularLightingEffect(GrSurfaceProxyView view,
                                                   sk_sp<const SkImageFilterLight> light,
                                                   SkScalar surfaceScale,
                                                   const SkMatrix& matrix,
                                                   SkScalar ks,
                                                   SkScalar shininess,
                                                   BoundaryMode boundaryMode,
                                                   const SkIRect* srcBounds,
                                                   const GrCaps& caps)
        : INHERITED(kGrSpecularLightingEffect_ClassID,
                    std::move(view),
                    std::move(light),
                    surfaceScale,
                    matrix,
                    boundaryMode,
                    srcBounds,
                    caps)
        , fKS(ks)
        , fShininess(shininess) {}

GrSpecularLightingEffect::GrSpecularLightingEffect(const GrSpecularLightingEffect& that)
        : INHERITED(that), fKS(that.fKS), fShininess(that.fShininess) {}

bool GrSpecularLightingEffect::onIsEqual(const GrFragmentProcessor& sBase) const {
    const GrSpecularLightingEffect& s = sBase.cast<GrSpecularLightingEffect>();
    return INHERITED::onIsEqual(sBase) &&
           this->ks() == s.ks() &&
           this->shininess() == s.shininess();
}

void GrSpecularLightingEffect::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                                     GrProcessorKeyBuilder* b) const {
    GrGLSpecularLightingEffect::GenKey(*this, caps, b);
}

GrGLSLFragmentProcessor* GrSpecularLightingEffect::onCreateGLSLInstance() const {
    return new GrGLSpecularLightingEffect;
}

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrSpecularLightingEffect);

#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> GrSpecularLightingEffect::TestCreate(GrProcessorTestData* d) {
    auto [view, ct, at] = d->randomView();
    SkScalar surfaceScale = d->fRandom->nextSScalar1();
    SkScalar ks = d->fRandom->nextUScalar1();
    SkScalar shininess = d->fRandom->nextUScalar1();
    sk_sp<SkImageFilterLight> light(create_random_light(d->fRandom));
    SkMatrix matrix;
    for (int i = 0; i < 9; i++) {
        matrix[i] = d->fRandom->nextUScalar1();
    }
    BoundaryMode mode = static_cast<BoundaryMode>(d->fRandom->nextU() % kBoundaryModeCount);

    uint32_t boundsX = d->fRandom->nextRangeU(0, view.width());
    uint32_t boundsY = d->fRandom->nextRangeU(0, view.height());
    uint32_t boundsW = d->fRandom->nextRangeU(0, view.width());
    uint32_t boundsH = d->fRandom->nextRangeU(0, view.height());
    SkIRect srcBounds = SkIRect::MakeXYWH(boundsX, boundsY, boundsW, boundsH);

    return GrSpecularLightingEffect::Make(std::move(view), std::move(light), surfaceScale, matrix,
                                          ks, shininess, mode, &srcBounds, *d->caps());
}
#endif

///////////////////////////////////////////////////////////////////////////////

void GrGLSpecularLightingEffect::emitLightFunc(const GrFragmentProcessor* owner,
                                               GrGLSLUniformHandler* uniformHandler,
                                               GrGLSLFPFragmentBuilder* fragBuilder,
                                               SkString* funcName) {
    const char* ks;
    const char* shininess;

    fKSUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag, kHalf_GrSLType, "KS", &ks);
    fShininessUni = uniformHandler->addUniform(owner,
                                               kFragment_GrShaderFlag,
                                               kHalf_GrSLType,
                                               "Shininess",
                                               &shininess);

    const GrShaderVar gLightArgs[] = {
        GrShaderVar("normal", kHalf3_GrSLType),
        GrShaderVar("surfaceToLight", kHalf3_GrSLType),
        GrShaderVar("lightColor", kHalf3_GrSLType)
    };
    SkString lightBody;
    lightBody.appendf("\thalf3 halfDir = half3(normalize(surfaceToLight + half3(0, 0, 1)));\n");
    lightBody.appendf("\thalf colorScale = half(%s * pow(dot(normal, halfDir), %s));\n",
                      ks, shininess);
    lightBody.appendf("\thalf3 color = lightColor * saturate(colorScale);\n");
    lightBody.appendf("\treturn half4(color, max(max(color.r, color.g), color.b));\n");
    fragBuilder->emitFunction(kHalf4_GrSLType,
                              "light",
                              SK_ARRAY_COUNT(gLightArgs),
                              gLightArgs,
                              lightBody.c_str(),
                              funcName);
}

void GrGLSpecularLightingEffect::onSetData(const GrGLSLProgramDataManager& pdman,
                                           const GrFragmentProcessor& effect) {
    INHERITED::onSetData(pdman, effect);
    const GrSpecularLightingEffect& spec = effect.cast<GrSpecularLightingEffect>();
    pdman.set1f(fKSUni, spec.ks());
    pdman.set1f(fShininessUni, spec.shininess());
}

///////////////////////////////////////////////////////////////////////////////
void GrGLLight::emitLightColorUniform(const GrFragmentProcessor* owner,
                                      GrGLSLUniformHandler* uniformHandler) {
    fColorUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag, kHalf3_GrSLType,
                                           "LightColor");
}

void GrGLLight::emitLightColor(const GrFragmentProcessor* owner,
                               GrGLSLUniformHandler* uniformHandler,
                               GrGLSLFPFragmentBuilder* fragBuilder,
                               const char *surfaceToLight) {
    fragBuilder->codeAppend(uniformHandler->getUniformCStr(this->lightColorUni()));
}

void GrGLLight::setData(const GrGLSLProgramDataManager& pdman,
                        const SkImageFilterLight* light) const {
    setUniformPoint3(pdman, fColorUni,
                     light->color().makeScale(SkScalarInvert(SkIntToScalar(255))));
}

///////////////////////////////////////////////////////////////////////////////

void GrGLDistantLight::setData(const GrGLSLProgramDataManager& pdman,
                               const SkImageFilterLight* light) const {
    INHERITED::setData(pdman, light);
    SkASSERT(light->type() == SkImageFilterLight::kDistant_LightType);
    const SkDistantLight* distantLight = static_cast<const SkDistantLight*>(light);
    setUniformNormal3(pdman, fDirectionUni, distantLight->direction());
}

void GrGLDistantLight::emitSurfaceToLight(const GrFragmentProcessor* owner,
                                          GrGLSLUniformHandler* uniformHandler,
                                          GrGLSLFPFragmentBuilder* fragBuilder,
                                          const char* z) {
    const char* dir;
    fDirectionUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag, kHalf3_GrSLType,
                                               "LightDirection", &dir);
    fragBuilder->codeAppend(dir);
}

///////////////////////////////////////////////////////////////////////////////

void GrGLPointLight::setData(const GrGLSLProgramDataManager& pdman,
                             const SkImageFilterLight* light) const {
    INHERITED::setData(pdman, light);
    SkASSERT(light->type() == SkImageFilterLight::kPoint_LightType);
    const SkPointLight* pointLight = static_cast<const SkPointLight*>(light);
    setUniformPoint3(pdman, fLocationUni, pointLight->location());
}

void GrGLPointLight::emitSurfaceToLight(const GrFragmentProcessor* owner,
                                        GrGLSLUniformHandler* uniformHandler,
                                        GrGLSLFPFragmentBuilder* fragBuilder,
                                        const char* z) {
    const char* loc;
    fLocationUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag, kHalf3_GrSLType,
                                              "LightLocation", &loc);
    fragBuilder->codeAppendf("normalize(%s - half3(sk_FragCoord.xy, %s))",
                             loc, z);
}

///////////////////////////////////////////////////////////////////////////////

void GrGLSpotLight::setData(const GrGLSLProgramDataManager& pdman,
                            const SkImageFilterLight* light) const {
    INHERITED::setData(pdman, light);
    SkASSERT(light->type() == SkImageFilterLight::kSpot_LightType);
    const SkSpotLight* spotLight = static_cast<const SkSpotLight *>(light);
    setUniformPoint3(pdman, fLocationUni, spotLight->location());
    pdman.set1f(fExponentUni, spotLight->specularExponent());
    pdman.set1f(fCosInnerConeAngleUni, spotLight->cosInnerConeAngle());
    pdman.set1f(fCosOuterConeAngleUni, spotLight->cosOuterConeAngle());
    pdman.set1f(fConeScaleUni, spotLight->coneScale());
    setUniformNormal3(pdman, fSUni, spotLight->s());
}

void GrGLSpotLight::emitSurfaceToLight(const GrFragmentProcessor* owner,
                                       GrGLSLUniformHandler* uniformHandler,
                                       GrGLSLFPFragmentBuilder* fragBuilder,
                                       const char* z) {
    const char* location;
    fLocationUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag, kHalf3_GrSLType,
                                              "LightLocation", &location);

    fragBuilder->codeAppendf("normalize(%s - half3(sk_FragCoord.xy, %s))",
                             location, z);
}

void GrGLSpotLight::emitLightColor(const GrFragmentProcessor* owner,
                                   GrGLSLUniformHandler* uniformHandler,
                                   GrGLSLFPFragmentBuilder* fragBuilder,
                                   const char *surfaceToLight) {

    const char* color = uniformHandler->getUniformCStr(this->lightColorUni()); // created by parent class.

    const char* exponent;
    const char* cosInner;
    const char* cosOuter;
    const char* coneScale;
    const char* s;
    fExponentUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag, kHalf_GrSLType,
                                              "Exponent", &exponent);
    fCosInnerConeAngleUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag,
                                                       kHalf_GrSLType, "CosInnerConeAngle",
                                                       &cosInner);
    fCosOuterConeAngleUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag,
                                                       kHalf_GrSLType, "CosOuterConeAngle",
                                                       &cosOuter);
    fConeScaleUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag, kHalf_GrSLType,
                                               "ConeScale", &coneScale);
    fSUni = uniformHandler->addUniform(owner, kFragment_GrShaderFlag, kHalf3_GrSLType, "S", &s);

    const GrShaderVar gLightColorArgs[] = {
        GrShaderVar("surfaceToLight", kHalf3_GrSLType)
    };
    SkString lightColorBody;
    lightColorBody.appendf("\thalf cosAngle = -dot(surfaceToLight, %s);\n", s);
    lightColorBody.appendf("\tif (cosAngle < %s) {\n", cosOuter);
    lightColorBody.appendf("\t\treturn half3(0);\n");
    lightColorBody.appendf("\t}\n");
    lightColorBody.appendf("\thalf scale = pow(cosAngle, %s);\n", exponent);
    lightColorBody.appendf("\tif (cosAngle < %s) {\n", cosInner);
    lightColorBody.appendf("\t\treturn %s * scale * (cosAngle - %s) * %s;\n",
                           color, cosOuter, coneScale);
    lightColorBody.appendf("\t}\n");
    lightColorBody.appendf("\treturn %s;\n", color);
    fragBuilder->emitFunction(kHalf3_GrSLType,
                              "lightColor",
                              SK_ARRAY_COUNT(gLightColorArgs),
                              gLightColorArgs,
                              lightColorBody.c_str(),
                              &fLightColorFunc);

    fragBuilder->codeAppendf("%s(%s)", fLightColorFunc.c_str(), surfaceToLight);
}

#endif

void SkLightingImageFilter::RegisterFlattenables() {
    SK_REGISTER_FLATTENABLE(SkDiffuseLightingImageFilter);
    SK_REGISTER_FLATTENABLE(SkSpecularLightingImageFilter);
}
