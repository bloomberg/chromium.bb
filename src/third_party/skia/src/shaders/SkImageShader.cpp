/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/shaders/SkImageShader.h"

#include "src/core/SkArenaAlloc.h"
#include "src/core/SkBitmapController.h"
#include "src/core/SkColorSpacePriv.h"
#include "src/core/SkColorSpaceXformSteps.h"
#include "src/core/SkMatrixProvider.h"
#include "src/core/SkOpts.h"
#include "src/core/SkRasterPipeline.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkScopeExit.h"
#include "src/core/SkVM.h"
#include "src/core/SkWriteBuffer.h"
#include "src/image/SkImage_Base.h"
#include "src/shaders/SkBitmapProcShader.h"
#include "src/shaders/SkEmptyShader.h"

SkM44 SkImageShader::CubicResamplerMatrix(float B, float C) {
#if 0
    constexpr SkM44 kMitchell = SkM44( 1.f/18.f, -9.f/18.f,  15.f/18.f,  -7.f/18.f,
                                      16.f/18.f,  0.f/18.f, -36.f/18.f,  21.f/18.f,
                                       1.f/18.f,  9.f/18.f,  27.f/18.f, -21.f/18.f,
                                       0.f/18.f,  0.f/18.f,  -6.f/18.f,   7.f/18.f);

    constexpr SkM44 kCatmull = SkM44(0.0f, -0.5f,  1.0f, -0.5f,
                                     1.0f,  0.0f, -2.5f,  1.5f,
                                     0.0f,  0.5f,  2.0f, -1.5f,
                                     0.0f,  0.0f, -0.5f,  0.5f);

    if (B == 1.0f/3 && C == 1.0f/3) {
        return kMitchell;
    }
    if (B == 0 && C == 0.5f) {
        return kCatmull;
    }
#endif
    return SkM44(    (1.f/6)*B, -(3.f/6)*B - C,       (3.f/6)*B + 2*C,    - (1.f/6)*B - C,
                 1 - (2.f/6)*B,              0, -3 + (12.f/6)*B +   C,  2 - (9.f/6)*B - C,
                     (1.f/6)*B,  (3.f/6)*B + C,  3 - (15.f/6)*B - 2*C, -2 + (9.f/6)*B + C,
                             0,              0,                    -C,      (1.f/6)*B + C);
}

/**
 *  We are faster in clamp, so always use that tiling when we can.
 */
static SkTileMode optimize(SkTileMode tm, int dimension) {
    SkASSERT(dimension > 0);
#ifdef SK_BUILD_FOR_ANDROID_FRAMEWORK
    // need to update frameworks/base/libs/hwui/tests/unit/SkiaBehaviorTests.cpp:55 to allow
    // for transforming to clamp.
    return tm;
#else
    return dimension == 1 ? SkTileMode::kClamp : tm;
#endif
}

SkImageShader::SkImageShader(sk_sp<SkImage> img,
                             SkTileMode tmx, SkTileMode tmy,
                             const SkMatrix* localMatrix,
                             FilterEnum filtering,
                             bool clampAsIfUnpremul)
    : INHERITED(localMatrix)
    , fImage(std::move(img))
    , fTileModeX(optimize(tmx, fImage->width()))
    , fTileModeY(optimize(tmy, fImage->height()))
    , fFilterEnum(filtering)
    , fClampAsIfUnpremul(clampAsIfUnpremul)
    , fFilterOptions({})    // ignored
{
    SkASSERT(filtering != kUseFilterOptions);
}

SkImageShader::SkImageShader(sk_sp<SkImage> img,
                             SkTileMode tmx, SkTileMode tmy,
                             const SkSamplingOptions& options,
                             const SkMatrix* localMatrix)
    : INHERITED(localMatrix)
    , fImage(std::move(img))
    , fTileModeX(optimize(tmx, fImage->width()))
    , fTileModeY(optimize(tmy, fImage->height()))
    , fFilterEnum(options.fUseCubic ? FilterEnum::kUseCubicResampler
                                    : FilterEnum::kUseFilterOptions)
    , fClampAsIfUnpremul(false)
    , fFilterOptions(options.fFilter)
    , fCubic(options.fCubic)
{}

// fClampAsIfUnpremul is always false when constructed through public APIs,
// so there's no need to read or write it here.

sk_sp<SkFlattenable> SkImageShader::CreateProc(SkReadBuffer& buffer) {
    auto tmx = buffer.read32LE<SkTileMode>(SkTileMode::kLastTileMode);
    auto tmy = buffer.read32LE<SkTileMode>(SkTileMode::kLastTileMode);

    FilterEnum fe = kInheritFromPaint;
    if (!buffer.isVersionLT(SkPicturePriv::kFilterEnumInImageShader_Version)) {
        fe = buffer.read32LE<FilterEnum>(kLast);
    }

    SkSamplingOptions op;

    if (buffer.isVersionLT(SkPicturePriv::kCubicResamplerImageShader_Version)) {
        if (!buffer.isVersionLT(SkPicturePriv::kFilterOptionsInImageShader_Version)) {
            op.fUseCubic = false;
            op.fFilter.fSampling = buffer.read32LE<SkSamplingMode>(SkSamplingMode::kLinear);
            op.fFilter.fMipmap   = buffer.read32LE<SkMipmapMode>(SkMipmapMode::kLinear);
        }
    } else {
        switch (fe) {
            case kUseFilterOptions:
                op.fUseCubic = false;
                op.fFilter.fSampling = buffer.read32LE<SkSamplingMode>(SkSamplingMode::kLinear);
                op.fFilter.fMipmap   = buffer.read32LE<SkMipmapMode>(SkMipmapMode::kLinear);
                break;
            case kUseCubicResampler:
                op.fUseCubic = true;
                op.fCubic.B = buffer.readScalar();
                op.fCubic.C = buffer.readScalar();
                break;
            default:
                break;
        }
    }

    SkMatrix localMatrix;
    buffer.readMatrix(&localMatrix);
    sk_sp<SkImage> img = buffer.readImage();
    if (!img) {
        return nullptr;
    }

    switch (fe) {
        case kUseFilterOptions:
        case kUseCubicResampler:
            return SkImageShader::Make(std::move(img), tmx, tmy, op, &localMatrix);
        default:
            break;
    }
    return SkImageShader::Make(std::move(img), tmx, tmy, &localMatrix, fe);
}

void SkImageShader::flatten(SkWriteBuffer& buffer) const {
    buffer.writeUInt((unsigned)fTileModeX);
    buffer.writeUInt((unsigned)fTileModeY);
    buffer.writeUInt((unsigned)fFilterEnum);
    switch (fFilterEnum) {
        case kUseCubicResampler:
            buffer.writeScalar(fCubic.B);
            buffer.writeScalar(fCubic.C);
            break;
        case kUseFilterOptions:
            buffer.writeUInt((unsigned)fFilterOptions.fSampling);
            buffer.writeUInt((unsigned)fFilterOptions.fMipmap);
            break;
        default:
            break;
    }
    buffer.writeMatrix(this->getLocalMatrix());
    buffer.writeImage(fImage.get());
    SkASSERT(fClampAsIfUnpremul == false);
}

bool SkImageShader::isOpaque() const {
    return fImage->isOpaque() &&
           fTileModeX != SkTileMode::kDecal && fTileModeY != SkTileMode::kDecal;
}

#ifdef SK_ENABLE_LEGACY_SHADERCONTEXT
static bool legacy_shader_can_handle(const SkMatrix& inv) {
    SkASSERT(!inv.hasPerspective());

    // Scale+translate methods are always present, but affine might not be.
    if (!SkOpts::S32_alpha_D32_filter_DXDY && !inv.isScaleTranslate()) {
        return false;
    }

    // legacy code uses SkFixed 32.32, so ensure the inverse doesn't map device coordinates
    // out of range.
    const SkScalar max_dev_coord = 32767.0f;
    const SkRect src = inv.mapRect(SkRect::MakeWH(max_dev_coord, max_dev_coord));

    // take 1/4 of max signed 32bits so we have room to subtract local values
    const SkScalar max_fixed32dot32 = float(SK_MaxS32) * 0.25f;
    if (!SkRect::MakeLTRB(-max_fixed32dot32, -max_fixed32dot32,
                          +max_fixed32dot32, +max_fixed32dot32).contains(src)) {
        return false;
    }

    // legacy shader impl should be able to handle these matrices
    return true;
}

SkShaderBase::Context* SkImageShader::onMakeContext(const ContextRec& rec,
                                                    SkArenaAlloc* alloc) const {
    // we only support the old SkFilterQuality setting
    if (fFilterEnum > kInheritFromPaint) {
        return nullptr;
    }

    auto quality = this->resolveFiltering(rec.fPaint->getFilterQuality());

    if (quality == kHigh_SkFilterQuality) {
        return nullptr;
    }
    if (fImage->alphaType() == kUnpremul_SkAlphaType) {
        return nullptr;
    }
    if (fImage->colorType() != kN32_SkColorType) {
        return nullptr;
    }
    if (fTileModeX != fTileModeY) {
        return nullptr;
    }
    if (fTileModeX == SkTileMode::kDecal || fTileModeY == SkTileMode::kDecal) {
        return nullptr;
    }

    // SkBitmapProcShader stores bitmap coordinates in a 16bit buffer,
    // so it can't handle bitmaps larger than 65535.
    //
    // We back off another bit to 32767 to make small amounts of
    // intermediate math safe, e.g. in
    //
    //     SkFixed fx = ...;
    //     fx = tile(fx + SK_Fixed1);
    //
    // we want to make sure (fx + SK_Fixed1) never overflows.
    if (fImage-> width() > 32767 ||
        fImage->height() > 32767) {
        return nullptr;
    }

    SkMatrix inv;
    if (!this->computeTotalInverse(*rec.fMatrix, rec.fLocalMatrix, &inv) ||
        !legacy_shader_can_handle(inv)) {
        return nullptr;
    }

    if (!rec.isLegacyCompatible(fImage->colorSpace())) {
        return nullptr;
    }

    // Send in a modified paint with different filter-quality if we don't agree with the paint
    SkPaint modifiedPaint;
    ContextRec modifiedRec = rec;
    if (quality != rec.fPaint->getFilterQuality()) {
        modifiedPaint = *rec.fPaint;
        modifiedPaint.setFilterQuality(quality);
        modifiedRec.fPaint = &modifiedPaint;
    }
    return SkBitmapProcLegacyShader::MakeContext(*this, fTileModeX, fTileModeY,
                                                 as_IB(fImage.get()), modifiedRec, alloc);
}
#endif

SkImage* SkImageShader::onIsAImage(SkMatrix* texM, SkTileMode xy[]) const {
    if (texM) {
        *texM = this->getLocalMatrix();
    }
    if (xy) {
        xy[0] = fTileModeX;
        xy[1] = fTileModeY;
    }
    return const_cast<SkImage*>(fImage.get());
}

sk_sp<SkShader> SkImageShader::Make(sk_sp<SkImage> image,
                                    SkTileMode tmx, SkTileMode tmy,
                                    const SkMatrix* localMatrix,
                                    FilterEnum filtering,
                                    bool clampAsIfUnpremul) {
    if (!image) {
        return sk_make_sp<SkEmptyShader>();
    }
    return sk_sp<SkShader>{
        new SkImageShader(image, tmx, tmy, localMatrix, filtering, clampAsIfUnpremul)
    };
}

sk_sp<SkShader> SkImageShader::Make(sk_sp<SkImage> image,
                                    SkTileMode tmx, SkTileMode tmy,
                                    const SkSamplingOptions& options,
                                    const SkMatrix* localMatrix) {
    auto is_unit = [](float x) {
        return x >= 0 && x <= 1;
    };
    if (options.fUseCubic) {
        if (!is_unit(options.fCubic.B) || !is_unit(options.fCubic.C)) {
            return nullptr;
        }
    }
    if (!image) {
        return sk_make_sp<SkEmptyShader>();
    }
    return sk_sp<SkShader>{
        new SkImageShader(image, tmx, tmy, options, localMatrix)
    };
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if SK_SUPPORT_GPU

#include "include/gpu/GrRecordingContext.h"
#include "src/gpu/GrBitmapTextureMaker.h"
#include "src/gpu/GrCaps.h"
#include "src/gpu/GrColorInfo.h"
#include "src/gpu/GrImageTextureMaker.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrTextureAdjuster.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/effects/GrBicubicEffect.h"
#include "src/gpu/effects/GrBlendFragmentProcessor.h"
#include "src/gpu/effects/GrTextureEffect.h"

std::unique_ptr<GrFragmentProcessor> SkImageShader::asFragmentProcessor(
        const GrFPArgs& args) const {
    const auto lm = this->totalLocalMatrix(args.fPreLocalMatrix);
    SkMatrix lmInverse;
    if (!lm->invert(&lmInverse)) {
        return nullptr;
    }

    // This would all be much nicer with std::variant.
    static constexpr size_t kSize = std::max({sizeof(GrYUVAImageTextureMaker),
                                              sizeof(GrTextureAdjuster      ),
                                              sizeof(GrImageTextureMaker    ),
                                              sizeof(GrBitmapTextureMaker   )});
    static constexpr size_t kAlign = std::max({alignof(GrYUVAImageTextureMaker),
                                               alignof(GrTextureAdjuster      ),
                                               alignof(GrImageTextureMaker    ),
                                               alignof(GrBitmapTextureMaker   )});
    alignas(kAlign) char storage[kSize];
    GrTextureProducer* producer = nullptr;
    SkScopeExit destroyProducer([&producer]{ if (producer) { producer->~GrTextureProducer(); } });

    uint32_t pinnedUniqueID;
    SkBitmap bm;
    if (as_IB(fImage)->isYUVA()) {
        producer = new (&storage) GrYUVAImageTextureMaker(args.fContext, fImage.get());
    } else if (GrSurfaceProxyView view =
                       as_IB(fImage)->refPinnedView(args.fContext, &pinnedUniqueID)) {
        GrColorInfo colorInfo;
        if (args.fContext->priv().caps()->isFormatSRGB(view.proxy()->backendFormat())) {
            SkASSERT(fImage->colorType() == kRGBA_8888_SkColorType);
            colorInfo = GrColorInfo(GrColorType::kRGBA_8888_SRGB, fImage->alphaType(),
                                    fImage->refColorSpace());
        } else {
            colorInfo = fImage->imageInfo().colorInfo();
        }
        producer = new (&storage)
                GrTextureAdjuster(args.fContext, std::move(view), colorInfo, pinnedUniqueID);
    } else if (fImage->isLazyGenerated()) {
        producer = new (&storage)
                GrImageTextureMaker(args.fContext, fImage.get(), GrImageTexGenPolicy::kDraw);
    } else if (as_IB(fImage)->getROPixels(nullptr, &bm)) {
        producer =
                new (&storage) GrBitmapTextureMaker(args.fContext, bm, GrImageTexGenPolicy::kDraw);
    } else {
        return nullptr;
    }
    GrSamplerState::WrapMode wmX = SkTileModeToWrapMode(fTileModeX),
                             wmY = SkTileModeToWrapMode(fTileModeY);
    // Must set wrap and filter on the sampler before requesting a texture. In two places
    // below we check the matrix scale factors to determine how to interpret the filter
    // quality setting. This completely ignores the complexity of the drawVertices case
    // where explicit local coords are provided by the caller.
    bool sharpen = args.fContext->priv().options().fSharpenMipmappedTextures;
    GrSamplerState::Filter fm;
    GrSamplerState::MipmapMode mm;
    bool bicubic;
    SkCubicResampler kernel = GrBicubicEffect::gMitchell;

    switch (fFilterEnum) {
        case FilterEnum::kUseFilterOptions:
            bicubic = false;
            switch (fFilterOptions.fSampling) {
                case SkSamplingMode::kNearest: fm = GrSamplerState::Filter::kNearest; break;
                case SkSamplingMode::kLinear : fm = GrSamplerState::Filter::kLinear ; break;
            }
            switch (fFilterOptions.fMipmap) {
                case SkMipmapMode::kNone   : mm = GrSamplerState::MipmapMode::kNone   ; break;
                case SkMipmapMode::kNearest: mm = GrSamplerState::MipmapMode::kNearest; break;
                case SkMipmapMode::kLinear : mm = GrSamplerState::MipmapMode::kLinear ; break;
            }
            break;
        case FilterEnum::kUseCubicResampler:
            bicubic = true;
            kernel = fCubic;
            fm = GrSamplerState::Filter::kNearest;
            mm = GrSamplerState::MipmapMode::kNone;
            break;
        case FilterEnum::kInheritFromPaint:
        default: // none, low, medium, high
            std::tie(fm, mm, bicubic) =
                    GrInterpretFilterQuality(fImage->dimensions(),
                                             this->resolveFiltering(args.fFilterQuality),
                                             args.fMatrixProvider.localToDevice(),
                                             *lm,
                                             sharpen,
                                             args.fAllowFilterQualityReduction);
            break;
    }
    std::unique_ptr<GrFragmentProcessor> fp;
    if (bicubic) {
        fp = producer->createBicubicFragmentProcessor(lmInverse, nullptr, nullptr, wmX, wmY, kernel);
    } else {
        fp = producer->createFragmentProcessor(lmInverse, nullptr, nullptr, {wmX, wmY, fm, mm});
    }
    if (!fp) {
        return nullptr;
    }
    fp = GrColorSpaceXformEffect::Make(std::move(fp), fImage->colorSpace(), producer->alphaType(),
                                       args.fDstColorInfo->colorSpace(), kPremul_SkAlphaType);
    fp = GrBlendFragmentProcessor::Make(std::move(fp), nullptr, SkBlendMode::kModulate);
    bool isAlphaOnly = SkColorTypeIsAlphaOnly(fImage->colorType());
    if (isAlphaOnly) {
        return fp;
    } else if (args.fInputColorIsOpaque) {
        return GrFragmentProcessor::OverrideInput(std::move(fp), SK_PMColor4fWHITE, false);
    }
    return GrFragmentProcessor::MulChildByInputAlpha(std::move(fp));
}

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
#include "src/core/SkImagePriv.h"

sk_sp<SkShader> SkMakeBitmapShader(const SkBitmap& src, SkTileMode tmx, SkTileMode tmy,
                                   const SkMatrix* localMatrix, SkCopyPixelsMode cpm) {
    return SkImageShader::Make(SkMakeImageFromRasterBitmap(src, cpm),
                               tmx, tmy, localMatrix, SkImageShader::kInheritFromPaint);
}

sk_sp<SkShader> SkMakeBitmapShaderForPaint(const SkPaint& paint, const SkBitmap& src,
                                           SkTileMode tmx, SkTileMode tmy,
                                           const SkMatrix* localMatrix, SkCopyPixelsMode mode) {
    auto s = SkMakeBitmapShader(src, tmx, tmy, localMatrix, mode);
    if (!s) {
        return nullptr;
    }
    if (src.colorType() == kAlpha_8_SkColorType && paint.getShader()) {
        // Compose the image shader with the paint's shader. Alpha images+shaders should output the
        // texture's alpha multiplied by the shader's color. DstIn (d*sa) will achieve this with
        // the source image and dst shader (MakeBlend takes dst first, src second).
        s = SkShaders::Blend(SkBlendMode::kDstIn, paint.refShader(), std::move(s));
    }
    return s;
}

void SkShaderBase::RegisterFlattenables() { SK_REGISTER_FLATTENABLE(SkImageShader); }

class SkImageStageUpdater : public SkStageUpdater {
public:
    SkImageStageUpdater(const SkImageShader* shader, bool usePersp)
        : fShader(shader)
        , fUsePersp(usePersp || as_SB(shader)->getLocalMatrix().hasPerspective())
    {}

    const SkImageShader* fShader;
    const bool           fUsePersp; // else use affine

    // large enough for perspective, though often we just use 2x3
    float fMatrixStorage[9];

#if 0   // TODO: when we support mipmaps
    SkRasterPipeline_GatherCtx* fGather;
    SkRasterPipeline_TileCtx* fLimitX;
    SkRasterPipeline_TileCtx* fLimitY;
    SkRasterPipeline_DecalTileCtx* fDecal;
#endif

    void append_matrix_stage(SkRasterPipeline* p) {
        if (fUsePersp) {
            p->append(SkRasterPipeline::matrix_perspective, fMatrixStorage);
        } else {
            p->append(SkRasterPipeline::matrix_2x3, fMatrixStorage);
        }
    }

    bool update(const SkMatrix& ctm, const SkMatrix* localM) override {
        SkMatrix matrix;
        if (fShader->computeTotalInverse(ctm, localM, &matrix)) {
            if (fUsePersp) {
                matrix.get9(fMatrixStorage);
            } else {
                // if we get here, matrix should be affine. If it isn't, then defensively we
                // won't draw (by returning false), but we should work to never let this
                // happen (i.e. better preflight by the caller to know ahead of time that we
                // may encounter perspective, either in the CTM, or in the localM).
                //
                // See https://bugs.chromium.org/p/skia/issues/detail?id=10004
                //
                if (!matrix.asAffine(fMatrixStorage)) {
                    SkASSERT(false);
                    return false;
                }
            }
            return true;
        }
        return false;
    }
};

static void tweak_quality_and_inv_matrix(SkFilterQuality* quality, SkMatrix* matrix) {
    // When the matrix is just an integer translate, bilerp == nearest neighbor.
    if (*quality == kLow_SkFilterQuality &&
            matrix->getType() <= SkMatrix::kTranslate_Mask &&
            matrix->getTranslateX() == (int)matrix->getTranslateX() &&
            matrix->getTranslateY() == (int)matrix->getTranslateY()) {
        *quality = kNone_SkFilterQuality;
    }

    // See skia:4649 and the GM image_scale_aligned.
    if (*quality == kNone_SkFilterQuality) {
        if (matrix->getScaleX() >= 0) {
            matrix->setTranslateX(nextafterf(matrix->getTranslateX(),
                                             floorf(matrix->getTranslateX())));
        }
        if (matrix->getScaleY() >= 0) {
            matrix->setTranslateY(nextafterf(matrix->getTranslateY(),
                                             floorf(matrix->getTranslateY())));
        }
    }
}

bool SkImageShader::doStages(const SkStageRec& rec, SkImageStageUpdater* updater) const {
    SkFilterQuality quality;
    switch (fFilterEnum) {
        case FilterEnum::kUseFilterOptions:
        case FilterEnum::kUseCubicResampler:
            return false;   // TODO: support these in stages
        case FilterEnum::kInheritFromPaint:
            quality = rec.fPaint.getFilterQuality();
            break;
        default:
            quality = (SkFilterQuality)fFilterEnum;
            break;
    }

    if (updater && quality == kMedium_SkFilterQuality) {
        // TODO: medium: recall RequestBitmap and update width/height accordingly
        return false;
    }

    SkRasterPipeline* p = rec.fPipeline;
    SkArenaAlloc* alloc = rec.fAlloc;

    SkMatrix matrix;
    if (!this->computeTotalInverse(rec.fMatrixProvider.localToDevice(), rec.fLocalM, &matrix)) {
        return false;
    }

    const auto* state = SkBitmapController::RequestBitmap(as_IB(fImage.get()),
                                                          matrix, quality, alloc);
    if (!state) {
        return false;
    }

    const SkPixmap& pm = state->pixmap();
    matrix  = state->invMatrix();
    quality = state->quality();
    auto info = pm.info();

    p->append(SkRasterPipeline::seed_shader);

    if (updater) {
        updater->append_matrix_stage(p);
    } else {
        tweak_quality_and_inv_matrix(&quality, &matrix);
        p->append_matrix(alloc, matrix);
    }

    auto gather = alloc->make<SkRasterPipeline_GatherCtx>();
    gather->pixels = pm.addr();
    gather->stride = pm.rowBytesAsPixels();
    gather->width  = pm.width();
    gather->height = pm.height();

    auto limit_x = alloc->make<SkRasterPipeline_TileCtx>(),
         limit_y = alloc->make<SkRasterPipeline_TileCtx>();
    limit_x->scale = pm.width();
    limit_x->invScale = 1.0f / pm.width();
    limit_y->scale = pm.height();
    limit_y->invScale = 1.0f / pm.height();

    SkRasterPipeline_DecalTileCtx* decal_ctx = nullptr;
    bool decal_x_and_y = fTileModeX == SkTileMode::kDecal && fTileModeY == SkTileMode::kDecal;
    if (fTileModeX == SkTileMode::kDecal || fTileModeY == SkTileMode::kDecal) {
        decal_ctx = alloc->make<SkRasterPipeline_DecalTileCtx>();
        decal_ctx->limit_x = limit_x->scale;
        decal_ctx->limit_y = limit_y->scale;
    }

#if 0   // TODO: when we support kMedium
    if (updator && (quality == kMedium_SkFilterQuality)) {
        // if we change levels in mipmap, we need to update the scales (and invScales)
        updator->fGather = gather;
        updator->fLimitX = limit_x;
        updator->fLimitY = limit_y;
        updator->fDecal = decal_ctx;
    }
#endif

    auto append_tiling_and_gather = [&] {
        if (decal_x_and_y) {
            p->append(SkRasterPipeline::decal_x_and_y,  decal_ctx);
        } else {
            switch (fTileModeX) {
                case SkTileMode::kClamp:  /* The gather_xxx stage will clamp for us. */     break;
                case SkTileMode::kMirror: p->append(SkRasterPipeline::mirror_x, limit_x);   break;
                case SkTileMode::kRepeat: p->append(SkRasterPipeline::repeat_x, limit_x);   break;
                case SkTileMode::kDecal:  p->append(SkRasterPipeline::decal_x,  decal_ctx); break;
            }
            switch (fTileModeY) {
                case SkTileMode::kClamp:  /* The gather_xxx stage will clamp for us. */     break;
                case SkTileMode::kMirror: p->append(SkRasterPipeline::mirror_y, limit_y);   break;
                case SkTileMode::kRepeat: p->append(SkRasterPipeline::repeat_y, limit_y);   break;
                case SkTileMode::kDecal:  p->append(SkRasterPipeline::decal_y,  decal_ctx); break;
            }
        }

        void* ctx = gather;
        switch (info.colorType()) {
            case kAlpha_8_SkColorType:      p->append(SkRasterPipeline::gather_a8,      ctx); break;
            case kA16_unorm_SkColorType:    p->append(SkRasterPipeline::gather_a16,     ctx); break;
            case kA16_float_SkColorType:    p->append(SkRasterPipeline::gather_af16,    ctx); break;
            case kRGB_565_SkColorType:      p->append(SkRasterPipeline::gather_565,     ctx); break;
            case kARGB_4444_SkColorType:    p->append(SkRasterPipeline::gather_4444,    ctx); break;
            case kR8G8_unorm_SkColorType:   p->append(SkRasterPipeline::gather_rg88,    ctx); break;
            case kR16G16_unorm_SkColorType: p->append(SkRasterPipeline::gather_rg1616,  ctx); break;
            case kR16G16_float_SkColorType: p->append(SkRasterPipeline::gather_rgf16,  ctx);  break;
            case kRGBA_8888_SkColorType:    p->append(SkRasterPipeline::gather_8888,    ctx); break;
            case kRGBA_1010102_SkColorType: p->append(SkRasterPipeline::gather_1010102, ctx); break;
            case kR16G16B16A16_unorm_SkColorType:
                                            p->append(SkRasterPipeline::gather_16161616,ctx); break;
            case kRGBA_F16Norm_SkColorType:
            case kRGBA_F16_SkColorType:     p->append(SkRasterPipeline::gather_f16,     ctx); break;
            case kRGBA_F32_SkColorType:     p->append(SkRasterPipeline::gather_f32,     ctx); break;

            case kGray_8_SkColorType:       p->append(SkRasterPipeline::gather_a8,      ctx);
                                            p->append(SkRasterPipeline::alpha_to_gray      ); break;

            case kRGB_888x_SkColorType:     p->append(SkRasterPipeline::gather_8888,    ctx);
                                            p->append(SkRasterPipeline::force_opaque       ); break;

            case kBGRA_1010102_SkColorType: p->append(SkRasterPipeline::gather_1010102, ctx);
                                            p->append(SkRasterPipeline::swap_rb            ); break;

            case kRGB_101010x_SkColorType:  p->append(SkRasterPipeline::gather_1010102, ctx);
                                            p->append(SkRasterPipeline::force_opaque       ); break;

            case kBGR_101010x_SkColorType:  p->append(SkRasterPipeline::gather_1010102, ctx);
                                            p->append(SkRasterPipeline::force_opaque       );
                                            p->append(SkRasterPipeline::swap_rb            ); break;

            case kBGRA_8888_SkColorType:    p->append(SkRasterPipeline::gather_8888,    ctx);
                                            p->append(SkRasterPipeline::swap_rb            ); break;

            case kUnknown_SkColorType: SkASSERT(false);
        }
        if (decal_ctx) {
            p->append(SkRasterPipeline::check_decal_mask, decal_ctx);
        }
    };

    auto append_misc = [&] {
        SkColorSpace* cs = info.colorSpace();
        SkAlphaType   at = info.alphaType();

        // Color for A8 images comes from the paint.  TODO: all alpha images?  none?
        if (info.colorType() == kAlpha_8_SkColorType) {
            SkColor4f rgb = rec.fPaint.getColor4f();
            p->append_set_rgb(alloc, rgb);

            cs = sk_srgb_singleton();
            at = kUnpremul_SkAlphaType;
        }

        // Bicubic filtering naturally produces out of range values on both sides of [0,1].
        if (quality == kHigh_SkFilterQuality) {
            p->append(SkRasterPipeline::clamp_0);
            p->append(at == kUnpremul_SkAlphaType || fClampAsIfUnpremul
                          ? SkRasterPipeline::clamp_1
                          : SkRasterPipeline::clamp_a);
        }

        // Transform color space and alpha type to match shader convention (dst CS, premul alpha).
        alloc->make<SkColorSpaceXformSteps>(cs, at,
                                            rec.fDstCS, kPremul_SkAlphaType)
            ->apply(p);

        return true;
    };

    // Check for fast-path stages.
    auto ct = info.colorType();
    if (true
        && (ct == kRGBA_8888_SkColorType || ct == kBGRA_8888_SkColorType)
        && quality == kLow_SkFilterQuality
        && fTileModeX == SkTileMode::kClamp && fTileModeY == SkTileMode::kClamp) {

        p->append(SkRasterPipeline::bilerp_clamp_8888, gather);
        if (ct == kBGRA_8888_SkColorType) {
            p->append(SkRasterPipeline::swap_rb);
        }
        return append_misc();
    }
    if (true
        && (ct == kRGBA_8888_SkColorType || ct == kBGRA_8888_SkColorType) // TODO: all formats
        && quality == kLow_SkFilterQuality
        && fTileModeX != SkTileMode::kDecal // TODO decal too?
        && fTileModeY != SkTileMode::kDecal) {

        auto ctx = alloc->make<SkRasterPipeline_SamplerCtx2>();
        *(SkRasterPipeline_GatherCtx*)(ctx) = *gather;
        ctx->ct = ct;
        ctx->tileX = fTileModeX;
        ctx->tileY = fTileModeY;
        ctx->invWidth  = 1.0f / ctx->width;
        ctx->invHeight = 1.0f / ctx->height;
        p->append(SkRasterPipeline::bilinear, ctx);
        return append_misc();
    }
    if (true
        && (ct == kRGBA_8888_SkColorType || ct == kBGRA_8888_SkColorType)
        && quality == kHigh_SkFilterQuality
        && fTileModeX == SkTileMode::kClamp && fTileModeY == SkTileMode::kClamp) {

        p->append(SkRasterPipeline::bicubic_clamp_8888, gather);
        if (ct == kBGRA_8888_SkColorType) {
            p->append(SkRasterPipeline::swap_rb);
        }
        return append_misc();
    }
    if (true
        && (ct == kRGBA_8888_SkColorType || ct == kBGRA_8888_SkColorType) // TODO: all formats
        && quality == kHigh_SkFilterQuality
        && fTileModeX != SkTileMode::kDecal // TODO decal too?
        && fTileModeY != SkTileMode::kDecal) {

        auto ctx = alloc->make<SkRasterPipeline_SamplerCtx2>();
        *(SkRasterPipeline_GatherCtx*)(ctx) = *gather;
        ctx->ct = ct;
        ctx->tileX = fTileModeX;
        ctx->tileY = fTileModeY;
        ctx->invWidth  = 1.0f / ctx->width;
        ctx->invHeight = 1.0f / ctx->height;
        p->append(SkRasterPipeline::bicubic, ctx);
        return append_misc();
    }

    SkRasterPipeline_SamplerCtx* sampler = nullptr;
    if (quality != kNone_SkFilterQuality) {
        sampler = alloc->make<SkRasterPipeline_SamplerCtx>();
    }

    auto sample = [&](SkRasterPipeline::StockStage setup_x,
                      SkRasterPipeline::StockStage setup_y) {
        p->append(setup_x, sampler);
        p->append(setup_y, sampler);
        append_tiling_and_gather();
        p->append(SkRasterPipeline::accumulate, sampler);
    };

    if (quality == kNone_SkFilterQuality) {
        append_tiling_and_gather();
    } else if (quality == kLow_SkFilterQuality) {
        p->append(SkRasterPipeline::save_xy, sampler);

        sample(SkRasterPipeline::bilinear_nx, SkRasterPipeline::bilinear_ny);
        sample(SkRasterPipeline::bilinear_px, SkRasterPipeline::bilinear_ny);
        sample(SkRasterPipeline::bilinear_nx, SkRasterPipeline::bilinear_py);
        sample(SkRasterPipeline::bilinear_px, SkRasterPipeline::bilinear_py);

        p->append(SkRasterPipeline::move_dst_src);

    } else {
        SkASSERT(quality == kHigh_SkFilterQuality);
        p->append(SkRasterPipeline::save_xy, sampler);

        sample(SkRasterPipeline::bicubic_n3x, SkRasterPipeline::bicubic_n3y);
        sample(SkRasterPipeline::bicubic_n1x, SkRasterPipeline::bicubic_n3y);
        sample(SkRasterPipeline::bicubic_p1x, SkRasterPipeline::bicubic_n3y);
        sample(SkRasterPipeline::bicubic_p3x, SkRasterPipeline::bicubic_n3y);

        sample(SkRasterPipeline::bicubic_n3x, SkRasterPipeline::bicubic_n1y);
        sample(SkRasterPipeline::bicubic_n1x, SkRasterPipeline::bicubic_n1y);
        sample(SkRasterPipeline::bicubic_p1x, SkRasterPipeline::bicubic_n1y);
        sample(SkRasterPipeline::bicubic_p3x, SkRasterPipeline::bicubic_n1y);

        sample(SkRasterPipeline::bicubic_n3x, SkRasterPipeline::bicubic_p1y);
        sample(SkRasterPipeline::bicubic_n1x, SkRasterPipeline::bicubic_p1y);
        sample(SkRasterPipeline::bicubic_p1x, SkRasterPipeline::bicubic_p1y);
        sample(SkRasterPipeline::bicubic_p3x, SkRasterPipeline::bicubic_p1y);

        sample(SkRasterPipeline::bicubic_n3x, SkRasterPipeline::bicubic_p3y);
        sample(SkRasterPipeline::bicubic_n1x, SkRasterPipeline::bicubic_p3y);
        sample(SkRasterPipeline::bicubic_p1x, SkRasterPipeline::bicubic_p3y);
        sample(SkRasterPipeline::bicubic_p3x, SkRasterPipeline::bicubic_p3y);

        p->append(SkRasterPipeline::move_dst_src);
    }

    return append_misc();
}

bool SkImageShader::onAppendStages(const SkStageRec& rec) const {
    return this->doStages(rec, nullptr);
}

SkStageUpdater* SkImageShader::onAppendUpdatableStages(const SkStageRec& rec) const {
    bool usePersp = rec.fMatrixProvider.localToDevice().hasPerspective();
    auto updater = rec.fAlloc->make<SkImageStageUpdater>(this, usePersp);
    return this->doStages(rec, updater) ? updater : nullptr;
}

enum class SamplingEnum {
    kNearest,
    kLinear,
    kBicubic,
};

skvm::Color SkImageShader::onProgram(skvm::Builder* p,
                                     skvm::Coord device, skvm::Coord origLocal, skvm::Color paint,
                                     const SkMatrixProvider& matrices, const SkMatrix* localM,
                                     SkFilterQuality paintQuality, const SkColorInfo& dst,
                                     skvm::Uniforms* uniforms, SkArenaAlloc* alloc) const {
    SkMatrix baseInv;
    if (!this->computeTotalInverse(matrices.localToDevice(), localM, &baseInv)) {
        return {};
    }
    baseInv.normalizePerspective();

    const SkPixmap *upper = nullptr,
                   *lower = nullptr;
    SkMatrix        upperInv;
    float           lowerWeight = 0;
    SamplingEnum    sampling = (SamplingEnum)fFilterOptions.fSampling;

    auto post_scale = [&](SkISize level, const SkMatrix& base) {
        return SkMatrix::Scale(SkIntToScalar(level.width())  / fImage->width(),
                               SkIntToScalar(level.height()) / fImage->height())
                * base;
    };

    if (fFilterEnum == kUseFilterOptions) {
        auto* access = alloc->make<SkMipmapAccessor>(as_IB(fImage.get()), baseInv,
                                                     fFilterOptions.fMipmap);
        upper = &access->level();
        upperInv = post_scale(upper->dimensions(), baseInv);
        lowerWeight = access->lowerWeight();
        if (lowerWeight > 0) {
            lower = &access->lowerLevel();
        }
    } else if (fFilterEnum == kUseCubicResampler){
        auto* access = alloc->make<SkMipmapAccessor>(as_IB(fImage.get()), baseInv,
                                                     SkMipmapMode::kNone);
        upper = &access->level();
        upperInv = post_scale(upper->dimensions(), baseInv);
        sampling = SamplingEnum::kBicubic;
    } else {
        // Convert from the filter-quality enum to our working description:
        //  sampling : nearest, bilerp, bicubic
        //  miplevel(s) and associated matrices
        //
        SkFilterQuality quality = paintQuality;
        if (fFilterEnum != kInheritFromPaint) {
            quality = (SkFilterQuality)fFilterEnum;
        }

        // We use RequestBitmap() to make sure our SkBitmapController::State lives in the alloc.
        // This lets the SkVMBlitter hang on to this state and keep our image alive.
        auto state = SkBitmapController::RequestBitmap(as_IB(fImage.get()), baseInv, quality, alloc);
        if (!state) {
            return {};
        }
        upper    = &state->pixmap();
        upperInv = state->invMatrix();

        quality  = state->quality();
        tweak_quality_and_inv_matrix(&quality, &upperInv);
        switch (quality) {
            case kNone_SkFilterQuality:   sampling = SamplingEnum::kNearest; break;
            case kLow_SkFilterQuality:    sampling = SamplingEnum::kLinear;  break;
            case kMedium_SkFilterQuality: sampling = SamplingEnum::kLinear;  break;
            case kHigh_SkFilterQuality:   sampling = SamplingEnum::kBicubic; break;
        }
    }

    skvm::Coord upperLocal = SkShaderBase::ApplyMatrix(p, upperInv, origLocal, uniforms);

    // All existing SkColorTypes pass these checks.  We'd only fail here adding new ones.
    skvm::PixelFormat unused;
    if (true  && !SkColorType_to_PixelFormat(upper->colorType(), &unused)) {
        return {};
    }
    if (lower && !SkColorType_to_PixelFormat(lower->colorType(), &unused)) {
        return {};
    }

    // We can exploit image opacity to skip work unpacking alpha channels.
    const bool input_is_opaque = SkAlphaTypeIsOpaque(upper->alphaType())
                              || SkColorTypeIsAlwaysOpaque(upper->colorType());

    // Each call to sample() will try to rewrite the same uniforms over and over,
    // so remember where we start and reset back there each time.  That way each
    // sample() call uses the same uniform offsets.

    auto compute_clamp_limit = [&](float limit) {
        // Subtract an ulp so the upper clamp limit excludes limit itself.
        int bits;
        memcpy(&bits, &limit, 4);
        return p->uniformF(uniforms->push(bits-1));
    };

    // Except in the simplest case (no mips, no filtering), we reference uniforms
    // more than once. To avoid adding/registering them multiple times, we pre-load them
    // into a struct (just to logically group them together), based on the "current"
    // pixmap (level of a mipmap).
    //
    struct Uniforms {
        skvm::F32   w, iw, i2w,
                    h, ih, i2h;

        skvm::F32   clamp_w,
                    clamp_h;

        skvm::Uniform addr;
        skvm::I32     rowBytesAsPixels;

        skvm::PixelFormat pixelFormat;  // not a uniform, but needed for each texel sample,
                                        // so we store it here, since it is also dependent on
                                        // the current pixmap (level).
    };

    auto setup_uniforms = [&](const SkPixmap& pm) -> Uniforms {
        skvm::PixelFormat pixelFormat;
        SkAssertResult(SkColorType_to_PixelFormat(pm.colorType(), &pixelFormat));
        return {
            p->uniformF(uniforms->pushF(     pm.width())),
            p->uniformF(uniforms->pushF(1.0f/pm.width())), // iff tileX == kRepeat
            p->uniformF(uniforms->pushF(0.5f/pm.width())), // iff tileX == kMirror

            p->uniformF(uniforms->pushF(     pm.height())),
            p->uniformF(uniforms->pushF(1.0f/pm.height())), // iff tileY == kRepeat
            p->uniformF(uniforms->pushF(0.5f/pm.height())), // iff tileY == kMirror

            compute_clamp_limit(pm. width()),
            compute_clamp_limit(pm.height()),

            uniforms->pushPtr(pm.addr()),
            p->uniform32(uniforms->push(pm.rowBytesAsPixels())),

            pixelFormat,
        };
    };

    auto sample_texel = [&](const Uniforms& u, skvm::F32 sx, skvm::F32 sy) -> skvm::Color {
        // repeat() and mirror() are written assuming they'll be followed by a [0,scale) clamp.
        auto repeat = [&](skvm::F32 v, skvm::F32 S, skvm::F32 I) {
            return v - floor(v * I) * S;
        };
        auto mirror = [&](skvm::F32 v, skvm::F32 S, skvm::F32 I2) {
            // abs( (v-scale) - (2*scale)*floor((v-scale)*(0.5f/scale)) - scale )
            //      {---A---}   {------------------B------------------}
            skvm::F32 A = v - S,
                      B = (S + S) * floor(A * I2);
            return abs(A - B - S);
        };
        switch (fTileModeX) {
            case SkTileMode::kDecal:  /* handled after gather */ break;
            case SkTileMode::kClamp:  /*    we always clamp   */ break;
            case SkTileMode::kRepeat: sx = repeat(sx, u.w, u.iw);  break;
            case SkTileMode::kMirror: sx = mirror(sx, u.w, u.i2w); break;
        }
        switch (fTileModeY) {
            case SkTileMode::kDecal:  /* handled after gather */  break;
            case SkTileMode::kClamp:  /*    we always clamp   */  break;
            case SkTileMode::kRepeat: sy = repeat(sy, u.h, u.ih);  break;
            case SkTileMode::kMirror: sy = mirror(sy, u.h, u.i2h); break;
        }

        // Always clamp sample coordinates to [0,width), [0,height), both for memory
        // safety and to handle the clamps still needed by kClamp, kRepeat, and kMirror.
        skvm::F32 clamped_x = clamp(sx, 0, u.clamp_w),
                  clamped_y = clamp(sy, 0, u.clamp_h);

        // Load pixels from pm.addr()[(int)sx + (int)sy*stride].
        skvm::I32 index = trunc(clamped_x) +
                          trunc(clamped_y) * u.rowBytesAsPixels;
        skvm::Color c = gather(u.pixelFormat, u.addr, index);

        // If we know the image is opaque, jump right to alpha = 1.0f, skipping work to unpack it.
        if (input_is_opaque) {
            c.a = p->splat(1.0f);
        }

        // Mask away any pixels that we tried to sample outside the bounds in kDecal.
        if (fTileModeX == SkTileMode::kDecal || fTileModeY == SkTileMode::kDecal) {
            skvm::I32 mask = p->splat(~0);
            if (fTileModeX == SkTileMode::kDecal) { mask &= (sx == clamped_x); }
            if (fTileModeY == SkTileMode::kDecal) { mask &= (sy == clamped_y); }
            c.r = bit_cast(p->bit_and(mask, bit_cast(c.r)));
            c.g = bit_cast(p->bit_and(mask, bit_cast(c.g)));
            c.b = bit_cast(p->bit_and(mask, bit_cast(c.b)));
            c.a = bit_cast(p->bit_and(mask, bit_cast(c.a)));
            // Notice that even if input_is_opaque, c.a might now be 0.
        }

        return c;
    };

    auto sample_level = [&](const SkPixmap& pm, const SkMatrix& inv, skvm::Coord local) {
        const Uniforms u = setup_uniforms(pm);

        if (sampling == SamplingEnum::kNearest) {
            return sample_texel(u, local.x,local.y);
        } else if (sampling == SamplingEnum::kLinear) {
            // Our four sample points are the corners of a logical 1x1 pixel
            // box surrounding (x,y) at (0.5,0.5) off-center.
            skvm::F32 left   = local.x - 0.5f,
                      top    = local.y - 0.5f,
                      right  = local.x + 0.5f,
                      bottom = local.y + 0.5f;

            // The fractional parts of right and bottom are our lerp factors in x and y respectively.
            skvm::F32 fx = fract(right ),
                      fy = fract(bottom);

            return lerp(lerp(sample_texel(u, left,top   ), sample_texel(u, right,top   ), fx),
                        lerp(sample_texel(u, left,bottom), sample_texel(u, right,bottom), fx), fy);
        } else {
            SkASSERT(sampling == SamplingEnum::kBicubic);

            // All bicubic samples have the same fractional offset (fx,fy) from the center.
            // They're either the 16 corners of a 3x3 grid/ surrounding (x,y) at (0.5,0.5) off-center.
            skvm::F32 fx = fract(local.x + 0.5f),
                      fy = fract(local.y + 0.5f);
            skvm::F32 wx[4],
                      wy[4];

            SkM44 weights = CubicResamplerMatrix(fCubic.B, fCubic.C);

            auto dot = [](const skvm::F32 a[], const skvm::F32 b[]) {
                return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
            };
            const skvm::F32 tmpx[] =  { p->splat(1.0f), fx, fx*fx, fx*fx*fx };
            const skvm::F32 tmpy[] =  { p->splat(1.0f), fy, fy*fy, fy*fy*fy };

            for (int row = 0; row < 4; ++row) {
                SkV4 r = weights.row(row);
                skvm::F32 ru[] = {
                    p->uniformF(uniforms->pushF(r[0])),
                    p->uniformF(uniforms->pushF(r[1])),
                    p->uniformF(uniforms->pushF(r[2])),
                    p->uniformF(uniforms->pushF(r[3])),
                };
                wx[row] = dot(ru, tmpx);
                wy[row] = dot(ru, tmpy);
            }

            skvm::Color c;
            c.r = c.g = c.b = c.a = p->splat(0.0f);

            skvm::F32 sy = local.y - 1.5f;
            for (int j = 0; j < 4; j++, sy += 1.0f) {
                skvm::F32 sx = local.x - 1.5f;
                for (int i = 0; i < 4; i++, sx += 1.0f) {
                    skvm::Color s = sample_texel(u, sx,sy);
                    skvm::F32   w = wx[i] * wy[j];

                    c.r += s.r * w;
                    c.g += s.g * w;
                    c.b += s.b * w;
                    c.a += s.a * w;
                }
            }
            return c;
        }
    };

    skvm::Color c = sample_level(*upper, upperInv, upperLocal);
    if (lower) {
        auto lowerInv = post_scale(lower->dimensions(), baseInv);
        auto lowerLocal = SkShaderBase::ApplyMatrix(p, lowerInv, origLocal, uniforms);
        // lower * weight + upper * (1 - weight)
        c = lerp(c,
                 sample_level(*lower, lowerInv, lowerLocal),
                 p->uniformF(uniforms->pushF(lowerWeight)));
    }

    // If the input is opaque and we're not in decal mode, that means the output is too.
    // Forcing *a to 1.0 here will retroactively skip any work we did to interpolate sample alphas.
    if (input_is_opaque
            && fTileModeX != SkTileMode::kDecal
            && fTileModeY != SkTileMode::kDecal) {
        c.a = p->splat(1.0f);
    }

    // Alpha-only images get their color from the paint (already converted to dst color space).
    SkColorSpace* cs = upper->colorSpace();
    SkAlphaType   at = upper->alphaType();
    if (SkColorTypeIsAlphaOnly(upper->colorType())) {
        c.r = paint.r;
        c.g = paint.g;
        c.b = paint.b;

        cs = dst.colorSpace();
        at = kUnpremul_SkAlphaType;
    }

    if (sampling == SamplingEnum::kBicubic) {
        // Bicubic filtering naturally produces out of range values on both sides of [0,1].
        c.a = clamp01(c.a);

        skvm::F32 limit = (at == kUnpremul_SkAlphaType || fClampAsIfUnpremul)
                        ? p->splat(1.0f)
                        : c.a;
        c.r = clamp(c.r, 0.0f, limit);
        c.g = clamp(c.g, 0.0f, limit);
        c.b = clamp(c.b, 0.0f, limit);
    }

    return SkColorSpaceXformSteps{cs,at, dst.colorSpace(),dst.alphaType()}.program(p, uniforms, c);
}
