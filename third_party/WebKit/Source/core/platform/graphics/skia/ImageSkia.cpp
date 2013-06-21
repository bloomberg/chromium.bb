/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "SkBitmap.h"
#include "SkRect.h"
#include "SkShader.h"
#include "core/platform/FloatConversion.h"
#include "core/platform/graphics/BitmapImage.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/GraphicsContextStateSaver.h"
#include "core/platform/graphics/ImageObserver.h"
#include "core/platform/graphics/chromium/DeferredImageDecoder.h"
#include "core/platform/graphics/skia/NativeImageSkia.h"
#include "core/platform/graphics/skia/SkiaUtils.h"
#include "core/platform/graphics/transforms/AffineTransform.h"
#include <wtf/text/WTFString.h>

#include <math.h>
#include <limits>

#include "core/platform/chromium/TraceEvent.h"

namespace WebCore {

// Used by computeResamplingMode to tell how bitmaps should be resampled.
enum ResamplingMode {
    // Nearest neighbor resampling. Used when we detect that the page is
    // trying to make a pattern by stretching a small bitmap very large.
    RESAMPLE_NONE,

    // Default skia resampling. Used for large growing of images where high
    // quality resampling doesn't get us very much except a slowdown.
    RESAMPLE_LINEAR,

    // High quality resampling.
    RESAMPLE_AWESOME,
};

static ResamplingMode computeResamplingMode(const SkMatrix& matrix, const NativeImageSkia& bitmap, float srcWidth, float srcHeight, float destWidth, float destHeight)
{
    // The percent change below which we will not resample. This usually means
    // an off-by-one error on the web page, and just doing nearest neighbor
    // sampling is usually good enough.
    const float kFractionalChangeThreshold = 0.025f;

    // Images smaller than this in either direction are considered "small" and
    // are not resampled ever (see below).
    const int kSmallImageSizeThreshold = 8;

    // The amount an image can be stretched in a single direction before we
    // say that it is being stretched so much that it must be a line or
    // background that doesn't need resampling.
    const float kLargeStretch = 3.0f;

    // Figure out if we should resample this image. We try to prune out some
    // common cases where resampling won't give us anything, since it is much
    // slower than drawing stretched.
    float diffWidth = fabs(destWidth - srcWidth);
    float diffHeight = fabs(destHeight - srcHeight);
    bool widthNearlyEqual = diffWidth < std::numeric_limits<float>::epsilon();
    bool heightNearlyEqual = diffHeight < std::numeric_limits<float>::epsilon();
    // We don't need to resample if the source and destination are the same.
    if (widthNearlyEqual && heightNearlyEqual)
        return RESAMPLE_NONE;

    if (srcWidth <= kSmallImageSizeThreshold
        || srcHeight <= kSmallImageSizeThreshold
        || destWidth <= kSmallImageSizeThreshold
        || destHeight <= kSmallImageSizeThreshold) {
        // Never resample small images. These are often used for borders and
        // rules (think 1x1 images used to make lines).
        return RESAMPLE_NONE;
    }

    if (srcHeight * kLargeStretch <= destHeight || srcWidth * kLargeStretch <= destWidth) {
        // Large image detected.

        // Don't resample if it is being stretched a lot in only one direction.
        // This is trying to catch cases where somebody has created a border
        // (which might be large) and then is stretching it to fill some part
        // of the page.
        if (widthNearlyEqual || heightNearlyEqual)
            return RESAMPLE_NONE;

        // The image is growing a lot and in more than one direction. Resampling
        // is slow and doesn't give us very much when growing a lot.
        return RESAMPLE_LINEAR;
    }

    if ((diffWidth / srcWidth < kFractionalChangeThreshold)
        && (diffHeight / srcHeight < kFractionalChangeThreshold)) {
        // It is disappointingly common on the web for image sizes to be off by
        // one or two pixels. We don't bother resampling if the size difference
        // is a small fraction of the original size.
        return RESAMPLE_NONE;
    }

    // When the image is not yet done loading, use linear. We don't cache the
    // partially resampled images, and as they come in incrementally, it causes
    // us to have to resample the whole thing every time.
    if (!bitmap.isDataComplete())
        return RESAMPLE_LINEAR;

    // Everything else gets resampled.
    // High quality interpolation only enabled for scaling and translation.
    if (!(matrix.getType() & (SkMatrix::kAffine_Mask | SkMatrix::kPerspective_Mask)))
        return RESAMPLE_AWESOME;
    
    return RESAMPLE_LINEAR;
}

static ResamplingMode limitResamplingMode(GraphicsContext* context, ResamplingMode resampling)
{
    switch (context->imageInterpolationQuality()) {
    case InterpolationNone:
        return RESAMPLE_NONE;
    case InterpolationMedium:
        // For now we treat InterpolationMedium and InterpolationLow the same.
    case InterpolationLow:
        if (resampling == RESAMPLE_AWESOME)
            return RESAMPLE_LINEAR;
        break;
    case InterpolationHigh:
    case InterpolationDefault:
        break;
    }

    return resampling;
}

// Return true if the rectangle is aligned to integer boundaries.
// See comments for computeBitmapDrawRects() for how this is used.
static bool areBoundariesIntegerAligned(const SkRect& rect)
{
    // Value is 1.19209e-007. This is the tolerance threshold.
    const float epsilon = std::numeric_limits<float>::epsilon();
    SkIRect roundedRect = roundedIntRect(rect);

    return fabs(rect.x() - roundedRect.x()) < epsilon
        && fabs(rect.y() - roundedRect.y()) < epsilon
        && fabs(rect.right() - roundedRect.right()) < epsilon
        && fabs(rect.bottom() - roundedRect.bottom()) < epsilon;
}

// This function is used to scale an image and extract a scaled fragment.
//
// ALGORITHM
//
// Because the scaled image size has to be integers, we approximate the real
// scale with the following formula (only X direction is shown):
//
// scaledImageWidth = round(scaleX * imageRect.width())
// approximateScaleX = scaledImageWidth / imageRect.width()
//
// With this method we maintain a constant scale factor among fragments in
// the scaled image. This allows fragments to stitch together to form the
// full scaled image. The downside is there will be a small difference
// between |scaleX| and |approximateScaleX|.
//
// A scaled image fragment is identified by:
//
// - Scaled image size
// - Scaled image fragment rectangle (IntRect)
//
// Scaled image size has been determined and the next step is to compute the
// rectangle for the scaled image fragment which needs to be an IntRect.
//
// scaledSrcRect = srcRect * (approximateScaleX, approximateScaleY)
// enclosingScaledSrcRect = enclosingIntRect(scaledSrcRect)
//
// Finally we extract the scaled image fragment using
// (scaledImageSize, enclosingScaledSrcRect).
//
static SkBitmap extractScaledImageFragment(const NativeImageSkia& bitmap, const SkRect& srcRect, float scaleX, float scaleY, SkRect* scaledSrcRect)
{
    SkISize imageSize = SkISize::Make(bitmap.bitmap().width(), bitmap.bitmap().height());
    SkISize scaledImageSize = SkISize::Make(clampToInteger(roundf(imageSize.width() * scaleX)),
        clampToInteger(roundf(imageSize.height() * scaleY)));

    SkRect imageRect = SkRect::MakeWH(imageSize.width(), imageSize.height());
    SkRect scaledImageRect = SkRect::MakeWH(scaledImageSize.width(), scaledImageSize.height());

    SkMatrix scaleTransform;
    scaleTransform.setRectToRect(imageRect, scaledImageRect, SkMatrix::kFill_ScaleToFit);
    scaleTransform.mapRect(scaledSrcRect, srcRect);

    scaledSrcRect->intersect(scaledImageRect);
    SkIRect enclosingScaledSrcRect = enclosingIntRect(*scaledSrcRect);

    // |enclosingScaledSrcRect| can be larger than |scaledImageSize| because
    // of float inaccuracy so clip to get inside.
    enclosingScaledSrcRect.intersect(SkIRect::MakeSize(scaledImageSize));

    // scaledSrcRect is relative to the pixel snapped fragment we're extracting.
    scaledSrcRect->offset(-enclosingScaledSrcRect.x(), -enclosingScaledSrcRect.y());

    return bitmap.resizedBitmap(scaledImageSize, enclosingScaledSrcRect);
}

// This does a lot of computation to resample only the portion of the bitmap
// that will only be drawn. This is critical for performance since when we are
// scrolling, for example, we are only drawing a small strip of the image.
// Resampling the whole image every time is very slow, so this speeds up things
// dramatically.
//
// Note: this code is only used when the canvas transformation is limited to
// scaling or translation.
static void drawResampledBitmap(GraphicsContext* context, SkPaint& paint, const NativeImageSkia& bitmap, const SkRect& srcRect, const SkRect& destRect)
{
    TRACE_EVENT0("skia", "drawResampledBitmap");
    // We want to scale |destRect| with transformation in the canvas to obtain
    // the final scale. The final scale is a combination of scale transform
    // in canvas and explicit scaling (srcRect and destRect).
    SkRect screenRect;
    context->getTotalMatrix().mapRect(&screenRect, destRect);
    float realScaleX = screenRect.width() / srcRect.width();
    float realScaleY = screenRect.height() / srcRect.height();

    // This part of code limits scaling only to visible portion in the
    SkRect destRectVisibleSubset;
    ClipRectToCanvas(context, destRect, &destRectVisibleSubset);

    // ClipRectToCanvas often overshoots, resulting in a larger region than our
    // original destRect. Intersecting gets us back inside.
    if (!destRectVisibleSubset.intersect(destRect))
        return; // Nothing visible in destRect.

    // Find the corresponding rect in the source image.
    SkMatrix destToSrcTransform;
    SkRect srcRectVisibleSubset;
    destToSrcTransform.setRectToRect(destRect, srcRect, SkMatrix::kFill_ScaleToFit);
    destToSrcTransform.mapRect(&srcRectVisibleSubset, destRectVisibleSubset);

    SkRect scaledSrcRect;
    SkBitmap scaledImageFragment = extractScaledImageFragment(bitmap, srcRectVisibleSubset, realScaleX, realScaleY, &scaledSrcRect);

    context->drawBitmapRect(scaledImageFragment, &scaledSrcRect, destRectVisibleSubset, &paint);
}

static bool hasNon90rotation(GraphicsContext* context)
{
    return !context->getTotalMatrix().rectStaysRect();
}

void Image::paintSkBitmap(GraphicsContext* context, const NativeImageSkia& bitmap, const SkRect& srcRect, const SkRect& destRect, const SkXfermode::Mode& compOp)
{
    TRACE_EVENT0("skia", "paintSkBitmap");
    SkPaint paint;
    paint.setXfermodeMode(compOp);
    paint.setAlpha(context->getNormalizedAlpha());
    paint.setLooper(context->drawLooper());
    // only antialias if we're rotated or skewed
    paint.setAntiAlias(hasNon90rotation(context));

    ResamplingMode resampling;
    if (context->isAccelerated())
        resampling = RESAMPLE_LINEAR;
    else if (context->printing())
        resampling = RESAMPLE_NONE;
    else {
        // Take into account scale applied to the canvas when computing sampling mode (e.g. CSS scale or page scale).
        SkRect destRectTarget = destRect;
        if (!(context->getTotalMatrix().getType() & (SkMatrix::kAffine_Mask | SkMatrix::kPerspective_Mask)))
            context->getTotalMatrix().mapRect(&destRectTarget, destRect);

        resampling = computeResamplingMode(context->getTotalMatrix(), bitmap,
            SkScalarToFloat(srcRect.width()), SkScalarToFloat(srcRect.height()),
            SkScalarToFloat(destRectTarget.width()), SkScalarToFloat(destRectTarget.height()));
    }

    if (resampling == RESAMPLE_NONE) {
        // FIXME: This is to not break tests (it results in the filter bitmap flag
        // being set to true). We need to decide if we respect RESAMPLE_NONE
        // being returned from computeResamplingMode.
        resampling = RESAMPLE_LINEAR;
    }
    resampling = limitResamplingMode(context, resampling);
    paint.setFilterBitmap(resampling == RESAMPLE_LINEAR);

    // FIXME: Bicubic filtering in Skia is only applied to defer-decoded images
    // as an experiment. Once this filtering code path becomes stable we should
    // turn this on for all cases, including non-defer-decoded images.
    bool useBicubicFilter = resampling == RESAMPLE_AWESOME
        && DeferredImageDecoder::isLazyDecoded(bitmap.bitmap());
    if (useBicubicFilter)
        paint.setFlags(paint.getFlags() | SkPaint::kBicubicFilterBitmap_Flag | SkPaint::kFilterBitmap_Flag);

    if (resampling == RESAMPLE_AWESOME && !useBicubicFilter) {
        // Resample the image and then draw the result to canvas with bilinear
        // filtering.
        drawResampledBitmap(context, paint, bitmap, srcRect, destRect);
    } else {
        // We want to filter it if we decided to do interpolation above, or if
        // there is something interesting going on with the matrix (like a rotation).
        // Note: for serialization, we will want to subset the bitmap first so we
        // don't send extra pixels.
        context->drawBitmapRect(bitmap.bitmap(), &srcRect, destRect, &paint);
    }
    context->didDrawRect(destRect, paint, &bitmap.bitmap());
}

bool FrameData::clear(bool clearMetadata)
{
    if (clearMetadata)
        m_haveMetadata = false;

    m_orientation = DefaultImageOrientation;

    if (m_frame) {
        m_frame.clear();

        return true;
    }
    return false;
}

void Image::drawPattern(GraphicsContext* context,
                        const FloatRect& floatSrcRect,
                        const FloatSize& scale,
                        const FloatPoint& phase,
                        CompositeOperator compositeOp,
                        const FloatRect& destRect,
                        BlendMode blendMode)
{
    TRACE_EVENT0("skia", "Image::drawPattern");
    RefPtr<NativeImageSkia> bitmap = nativeImageForCurrentFrame();
    if (!bitmap)
        return;

    FloatRect normSrcRect = adjustForNegativeSize(floatSrcRect);
    normSrcRect.intersect(FloatRect(0, 0, bitmap->bitmap().width(), bitmap->bitmap().height()));
    if (destRect.isEmpty() || normSrcRect.isEmpty())
        return; // nothing to draw

    SkMatrix totalMatrix = context->getTotalMatrix();
    SkScalar ctmScaleX = totalMatrix.getScaleX();
    SkScalar ctmScaleY = totalMatrix.getScaleY();
    totalMatrix.preScale(scale.width(), scale.height());

    // Figure out what size the bitmap will be in the destination. The
    // destination rect is the bounds of the pattern, we need to use the
    // matrix to see how big it will be.
    SkRect destRectTarget;
    totalMatrix.mapRect(&destRectTarget, normSrcRect);

    float destBitmapWidth = SkScalarToFloat(destRectTarget.width());
    float destBitmapHeight = SkScalarToFloat(destRectTarget.height());

    // Compute the resampling mode.
    ResamplingMode resampling;
    if (context->isAccelerated() || context->printing())
        resampling = RESAMPLE_LINEAR;
    else
        resampling = computeResamplingMode(totalMatrix, *bitmap, normSrcRect.width(), normSrcRect.height(), destBitmapWidth, destBitmapHeight);
    resampling = limitResamplingMode(context, resampling);

    SkMatrix shaderTransform;
    SkShader* shader;

    // Bicubic filter is only applied to defer-decoded images, see
    // paintSkBitmap() for details.
    bool useBicubicFilter = resampling == RESAMPLE_AWESOME
        && DeferredImageDecoder::isLazyDecoded(bitmap->bitmap());

    if (resampling == RESAMPLE_AWESOME && !useBicubicFilter) {
        // Do nice resampling.
        float scaleX = destBitmapWidth / normSrcRect.width();
        float scaleY = destBitmapHeight / normSrcRect.height();
        SkRect scaledSrcRect;

        // The image fragment generated here is not exactly what is
        // requested. The scale factor used is approximated and image
        // fragment is slightly larger to align to integer
        // boundaries.
        SkBitmap resampled = extractScaledImageFragment(*bitmap, normSrcRect, scaleX, scaleY, &scaledSrcRect);
        shader = SkShader::CreateBitmapShader(resampled, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);

        // Since we just resized the bitmap, we need to remove the scale
        // applied to the pixels in the bitmap shader. This means we need
        // CTM * shaderTransform to have identity scale. Since we
        // can't modify CTM (or the rectangle will be drawn in the wrong
        // place), we must set shaderTransform's scale to the inverse of
        // CTM scale.
        shaderTransform.setScale(ctmScaleX ? 1 / ctmScaleX : 1, ctmScaleY ? 1 / ctmScaleY : 1);
    } else {
        // No need to resample before drawing.
        SkBitmap srcSubset;
        bitmap->bitmap().extractSubset(&srcSubset, enclosingIntRect(normSrcRect));
        shader = SkShader::CreateBitmapShader(srcSubset, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);
        // Because no resizing occurred, the shader transform should be
        // set to the pattern's transform, which just includes scale.
        shaderTransform.setScale(scale.width(), scale.height());
    }

    // We also need to translate it such that the origin of the pattern is the
    // origin of the destination rect, which is what WebKit expects. Skia uses
    // the coordinate system origin as the base for the pattern. If WebKit wants
    // a shifted image, it will shift it from there using the shaderTransform.
    float adjustedX = phase.x() + normSrcRect.x() * scale.width();
    float adjustedY = phase.y() + normSrcRect.y() * scale.height();
    shaderTransform.postTranslate(SkFloatToScalar(adjustedX), SkFloatToScalar(adjustedY));
    shader->setLocalMatrix(shaderTransform);

    SkPaint paint;
    paint.setShader(shader)->unref();
    paint.setXfermodeMode(WebCoreCompositeToSkiaComposite(compositeOp, blendMode));

    paint.setFilterBitmap(resampling == RESAMPLE_LINEAR);
    if (useBicubicFilter)
        paint.setFlags(paint.getFlags() | SkPaint::kBicubicFilterBitmap_Flag);

    context->drawRect(destRect, paint);
}

} // namespace WebCore
