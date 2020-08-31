/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkDevice.h"

#include "include/core/SkColorFilter.h"
#include "include/core/SkDrawable.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkPathMeasure.h"
#include "include/core/SkRSXform.h"
#include "include/core/SkShader.h"
#include "include/core/SkVertices.h"
#include "include/private/SkTo.h"
#include "src/core/SkDraw.h"
#include "src/core/SkGlyphRun.h"
#include "src/core/SkImageFilterCache.h"
#include "src/core/SkImagePriv.h"
#include "src/core/SkLatticeIter.h"
#include "src/core/SkMarkerStack.h"
#include "src/core/SkMatrixPriv.h"
#include "src/core/SkPathPriv.h"
#include "src/core/SkRasterClip.h"
#include "src/core/SkSpecialImage.h"
#include "src/core/SkTLazy.h"
#include "src/core/SkTextBlobPriv.h"
#include "src/core/SkUtils.h"
#include "src/image/SkImage_Base.h"
#include "src/shaders/SkLocalMatrixShader.h"
#include "src/utils/SkPatchUtils.h"

SkBaseDevice::SkBaseDevice(const SkImageInfo& info, const SkSurfaceProps& surfaceProps)
        : SkMatrixProvider(/* fLocalToDevice = */ SkMatrix::I())
        , fInfo(info)
        , fSurfaceProps(surfaceProps) {
    fDeviceToGlobal.reset();
    fGlobalToDevice.reset();
}

void SkBaseDevice::setDeviceCoordinateSystem(const SkMatrix& deviceToGlobal,
                                             const SkM44& localToDevice,
                                             int bufferOriginX,
                                             int bufferOriginY) {
    fDeviceToGlobal = deviceToGlobal;
    fDeviceToGlobal.normalizePerspective();
    SkAssertResult(deviceToGlobal.invert(&fGlobalToDevice));

    fLocalToDevice = localToDevice;
    fLocalToDevice.normalizePerspective();
    if (bufferOriginX | bufferOriginY) {
        fDeviceToGlobal.preTranslate(bufferOriginX, bufferOriginY);
        fGlobalToDevice.postTranslate(-bufferOriginX, -bufferOriginY);
        fLocalToDevice.postTranslate(-bufferOriginX, -bufferOriginY);
    }
    fLocalToDevice33 = fLocalToDevice.asM33();
}

void SkBaseDevice::setGlobalCTM(const SkM44& ctm) {
    fLocalToDevice = ctm;
    fLocalToDevice.normalizePerspective();
    if (!fGlobalToDevice.isIdentity()) {
        // Map from the global CTM state to this device's coordinate system.
        fLocalToDevice.postConcat(SkM44(fGlobalToDevice));
    }
    fLocalToDevice33 = fLocalToDevice.asM33();
}

bool SkBaseDevice::isPixelAlignedToGlobal() const {
    return fDeviceToGlobal.isTranslate() &&
           SkScalarIsInt(fDeviceToGlobal.getTranslateX()) &&
           SkScalarIsInt(fDeviceToGlobal.getTranslateY());
}

SkIPoint SkBaseDevice::getOrigin() const {
    // getOrigin() is deprecated, the old origin has been moved into the fDeviceToGlobal matrix.
    // This extracts the origin from the matrix, but asserts that a more complicated coordinate
    // space hasn't been set of the device. This function can be removed once existing use cases
    // have been updated to use the device-to-global matrix instead or have themselves been removed
    // (e.g. Android's device-space clip regions are going away, and are not compatible with the
    // generalized device coordinate system).
    SkASSERT(this->isPixelAlignedToGlobal());
    return SkIPoint::Make(SkScalarFloorToInt(fDeviceToGlobal.getTranslateX()),
                          SkScalarFloorToInt(fDeviceToGlobal.getTranslateY()));
}

SkMatrix SkBaseDevice::getRelativeTransform(const SkBaseDevice& inputDevice) const {
    // To get the transform from the input's space to this space, transform from the input space to
    // the global space, and then from the global space back to this space.
    return SkMatrix::Concat(fGlobalToDevice, inputDevice.fDeviceToGlobal);
}

bool SkBaseDevice::getLocalToMarker(uint32_t id, SkM44* localToMarker) const {
    // The marker stack stores CTM snapshots, which are "marker to global" matrices.
    // We ask for the (cached) inverse, which is a "global to marker" matrix.
    SkM44 globalToMarker;
    if (fMarkerStack && fMarkerStack->findMarkerInverse(id, &globalToMarker)) {
        if (localToMarker) {
            *localToMarker = globalToMarker * SkM44(fDeviceToGlobal) * fLocalToDevice;
        }
        return true;
    }
    return false;
}

static inline bool is_int(float x) {
    return x == (float) sk_float_round2int(x);
}

void SkBaseDevice::drawRegion(const SkRegion& region, const SkPaint& paint) {
    const SkMatrix& localToDevice = this->localToDevice();
    bool isNonTranslate = localToDevice.getType() & ~(SkMatrix::kTranslate_Mask);
    bool complexPaint = paint.getStyle() != SkPaint::kFill_Style || paint.getMaskFilter() ||
                        paint.getPathEffect();
    bool antiAlias = paint.isAntiAlias() && (!is_int(localToDevice.getTranslateX()) ||
                                             !is_int(localToDevice.getTranslateY()));
    if (isNonTranslate || complexPaint || antiAlias) {
        SkPath path;
        region.getBoundaryPath(&path);
        path.setIsVolatile(true);
        return this->drawPath(path, paint, true);
    }

    SkRegion::Iterator it(region);
    while (!it.done()) {
        this->drawRect(SkRect::Make(it.rect()), paint);
        it.next();
    }
}

void SkBaseDevice::drawArc(const SkRect& oval, SkScalar startAngle,
                           SkScalar sweepAngle, bool useCenter, const SkPaint& paint) {
    SkPath path;
    bool isFillNoPathEffect = SkPaint::kFill_Style == paint.getStyle() && !paint.getPathEffect();
    SkPathPriv::CreateDrawArcPath(&path, oval, startAngle, sweepAngle, useCenter,
                                  isFillNoPathEffect);
    this->drawPath(path, paint);
}

void SkBaseDevice::drawDRRect(const SkRRect& outer,
                              const SkRRect& inner, const SkPaint& paint) {
    SkPath path;
    path.addRRect(outer);
    path.addRRect(inner);
    path.setFillType(SkPathFillType::kEvenOdd);
    path.setIsVolatile(true);

    this->drawPath(path, paint, true);
}

void SkBaseDevice::drawPatch(const SkPoint cubics[12], const SkColor colors[4],
                             const SkPoint texCoords[4], SkBlendMode bmode, const SkPaint& paint) {
    SkISize lod = SkPatchUtils::GetLevelOfDetail(cubics, &this->localToDevice());
    auto vertices = SkPatchUtils::MakeVertices(cubics, colors, texCoords, lod.width(), lod.height(),
                                               this->imageInfo().colorSpace());
    if (vertices) {
        this->drawVertices(vertices.get(), bmode, paint);
    }
}

void SkBaseDevice::drawImageNine(const SkImage* image, const SkIRect& center,
                                 const SkRect& dst, const SkPaint& paint) {
    SkLatticeIter iter(image->width(), image->height(), center, dst);

    SkRect srcR, dstR;
    while (iter.next(&srcR, &dstR)) {
        this->drawImageRect(image, &srcR, dstR, paint, SkCanvas::kStrict_SrcRectConstraint);
    }
}

void SkBaseDevice::drawImageLattice(const SkImage* image,
                                    const SkCanvas::Lattice& lattice, const SkRect& dst,
                                    const SkPaint& paint) {
    SkLatticeIter iter(lattice, dst);

    SkRect srcR, dstR;
    SkColor c;
    bool isFixedColor = false;
    const SkImageInfo info = SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kUnpremul_SkAlphaType);

    while (iter.next(&srcR, &dstR, &isFixedColor, &c)) {
          if (isFixedColor || (srcR.width() <= 1.0f && srcR.height() <= 1.0f &&
                               image->readPixels(info, &c, 4, srcR.fLeft, srcR.fTop))) {
              // Fast draw with drawRect, if this is a patch containing a single color
              // or if this is a patch containing a single pixel.
              if (0 != c || !paint.isSrcOver()) {
                   SkPaint paintCopy(paint);
                   int alpha = SkAlphaMul(SkColorGetA(c), SkAlpha255To256(paint.getAlpha()));
                   paintCopy.setColor(SkColorSetA(c, alpha));
                   this->drawRect(dstR, paintCopy);
              }
        } else {
            this->drawImageRect(image, &srcR, dstR, paint, SkCanvas::kStrict_SrcRectConstraint);
        }
    }
}

static SkPoint* quad_to_tris(SkPoint tris[6], const SkPoint quad[4]) {
    tris[0] = quad[0];
    tris[1] = quad[1];
    tris[2] = quad[2];

    tris[3] = quad[0];
    tris[4] = quad[2];
    tris[5] = quad[3];

    return tris + 6;
}

void SkBaseDevice::drawAtlas(const SkImage* atlas, const SkRSXform xform[],
                             const SkRect tex[], const SkColor colors[], int quadCount,
                             SkBlendMode mode, const SkPaint& paint) {
    const int triCount = quadCount << 1;
    const int vertexCount = triCount * 3;
    uint32_t flags = SkVertices::kHasTexCoords_BuilderFlag;
    if (colors) {
        flags |= SkVertices::kHasColors_BuilderFlag;
    }
    SkVertices::Builder builder(SkVertices::kTriangles_VertexMode, vertexCount, 0, flags);

    SkPoint* vPos = builder.positions();
    SkPoint* vTex = builder.texCoords();
    SkColor* vCol = builder.colors();
    for (int i = 0; i < quadCount; ++i) {
        SkPoint tmp[4];
        xform[i].toQuad(tex[i].width(), tex[i].height(), tmp);
        vPos = quad_to_tris(vPos, tmp);

        tex[i].toQuad(tmp);
        vTex = quad_to_tris(vTex, tmp);

        if (colors) {
            sk_memset32(vCol, colors[i], 6);
            vCol += 6;
        }
    }
    SkPaint p(paint);
    p.setShader(atlas->makeShader());
    this->drawVertices(builder.detach().get(), mode, p);
}


void SkBaseDevice::drawEdgeAAQuad(const SkRect& r, const SkPoint clip[4], SkCanvas::QuadAAFlags aa,
                                  const SkColor4f& color, SkBlendMode mode) {
    SkPaint paint;
    paint.setColor4f(color);
    paint.setBlendMode(mode);
    paint.setAntiAlias(aa == SkCanvas::kAll_QuadAAFlags);

    if (clip) {
        // Draw the clip directly as a quad since it's a filled color with no local coords
        SkPath clipPath;
        clipPath.addPoly(clip, 4, true);
        this->drawPath(clipPath, paint);
    } else {
        this->drawRect(r, paint);
    }
}

void SkBaseDevice::drawEdgeAAImageSet(const SkCanvas::ImageSetEntry images[], int count,
                                      const SkPoint dstClips[], const SkMatrix preViewMatrices[],
                                      const SkPaint& paint,
                                      SkCanvas::SrcRectConstraint constraint) {
    SkASSERT(paint.getStyle() == SkPaint::kFill_Style);
    SkASSERT(!paint.getPathEffect());

    SkPaint entryPaint = paint;
    const SkM44 baseLocalToDevice = this->localToDevice44();
    int clipIndex = 0;
    for (int i = 0; i < count; ++i) {
        // TODO: Handle per-edge AA. Right now this mirrors the SkiaRenderer component of Chrome
        // which turns off antialiasing unless all four edges should be antialiased. This avoids
        // seaming in tiled composited layers.
        entryPaint.setAntiAlias(images[i].fAAFlags == SkCanvas::kAll_QuadAAFlags);
        entryPaint.setAlphaf(paint.getAlphaf() * images[i].fAlpha);

        bool needsRestore = false;
        SkASSERT(images[i].fMatrixIndex < 0 || preViewMatrices);
        if (images[i].fMatrixIndex >= 0) {
            this->save();
            this->setLocalToDevice(baseLocalToDevice *
                                   SkM44(preViewMatrices[images[i].fMatrixIndex]));
            needsRestore = true;
        }

        SkASSERT(!images[i].fHasClip || dstClips);
        if (images[i].fHasClip) {
            // Since drawImageRect requires a srcRect, the dst clip is implemented as a true clip
            if (!needsRestore) {
                this->save();
                needsRestore = true;
            }
            SkPath clipPath;
            clipPath.addPoly(dstClips + clipIndex, 4, true);
            this->clipPath(clipPath, SkClipOp::kIntersect, entryPaint.isAntiAlias());
            clipIndex += 4;
        }
        this->drawImageRect(images[i].fImage.get(), &images[i].fSrcRect, images[i].fDstRect,
                            entryPaint, constraint);
        if (needsRestore) {
            this->restoreLocal(baseLocalToDevice);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void SkBaseDevice::drawDrawable(SkDrawable* drawable, const SkMatrix* matrix, SkCanvas* canvas) {
    drawable->draw(canvas, matrix);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void SkBaseDevice::drawSpecial(SkSpecialImage*, int x, int y, const SkPaint&,
                               SkImage*, const SkMatrix&) {}
sk_sp<SkSpecialImage> SkBaseDevice::makeSpecial(const SkBitmap&) { return nullptr; }
sk_sp<SkSpecialImage> SkBaseDevice::makeSpecial(const SkImage*) { return nullptr; }
sk_sp<SkSpecialImage> SkBaseDevice::snapSpecial(const SkIRect&, bool) { return nullptr; }
sk_sp<SkSpecialImage> SkBaseDevice::snapSpecial() {
    return this->snapSpecial(SkIRect::MakeWH(this->width(), this->height()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool SkBaseDevice::readPixels(const SkPixmap& pm, int x, int y) {
    return this->onReadPixels(pm, x, y);
}

bool SkBaseDevice::writePixels(const SkPixmap& pm, int x, int y) {
    return this->onWritePixels(pm, x, y);
}

bool SkBaseDevice::onWritePixels(const SkPixmap&, int, int) {
    return false;
}

bool SkBaseDevice::onReadPixels(const SkPixmap&, int x, int y) {
    return false;
}

bool SkBaseDevice::accessPixels(SkPixmap* pmap) {
    SkPixmap tempStorage;
    if (nullptr == pmap) {
        pmap = &tempStorage;
    }
    return this->onAccessPixels(pmap);
}

bool SkBaseDevice::peekPixels(SkPixmap* pmap) {
    SkPixmap tempStorage;
    if (nullptr == pmap) {
        pmap = &tempStorage;
    }
    return this->onPeekPixels(pmap);
}

//////////////////////////////////////////////////////////////////////////////////////////

#include "src/core/SkUtils.h"

void SkBaseDevice::drawGlyphRunRSXform(const SkFont& font, const SkGlyphID glyphs[],
                                       const SkRSXform xform[], int count, SkPoint origin,
                                       const SkPaint& paint) {
    const SkM44 originalLocalToDevice = this->localToDevice44();
    if (!originalLocalToDevice.isFinite() || !SkScalarIsFinite(font.getSize()) ||
        !SkScalarIsFinite(font.getScaleX()) ||
        !SkScalarIsFinite(font.getSkewX())) {
        return;
    }

    SkPoint sharedPos{0, 0};    // we're at the origin
    SkGlyphID glyphID;
    SkGlyphRun glyphRun{
        font,
        SkSpan<const SkPoint>{&sharedPos, 1},
        SkSpan<const SkGlyphID>{&glyphID, 1},
        SkSpan<const char>{},
        SkSpan<const uint32_t>{}
    };

    for (int i = 0; i < count; i++) {
        glyphID = glyphs[i];
        // now "glyphRun" is pointing at the current glyphID

        SkMatrix glyphToDevice;
        glyphToDevice.setRSXform(xform[i]).postTranslate(origin.fX, origin.fY);

        // We want to rotate each glyph by the rsxform, but we don't want to rotate "space"
        // (i.e. the shader that cares about the ctm) so we have to undo our little ctm trick
        // with a localmatrixshader so that the shader draws as if there was no change to the ctm.
        SkPaint transformingPaint{paint};
        auto shader = transformingPaint.getShader();
        if (shader) {
            SkMatrix inverse;
            if (glyphToDevice.invert(&inverse)) {
                transformingPaint.setShader(shader->makeWithLocalMatrix(inverse));
            } else {
                transformingPaint.setShader(nullptr);  // can't handle this xform
            }
        }

        this->setLocalToDevice(originalLocalToDevice * SkM44(glyphToDevice));

        this->drawGlyphRunList(SkGlyphRunList{glyphRun, transformingPaint});
    }
    this->setLocalToDevice(originalLocalToDevice);
}

//////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkSurface> SkBaseDevice::makeSurface(SkImageInfo const&, SkSurfaceProps const&) {
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////

void SkBaseDevice::LogDrawScaleFactor(const SkMatrix& view, const SkMatrix& srcToDst,
                                      SkFilterQuality filterQuality) {
#if SK_HISTOGRAMS_ENABLED
    SkMatrix matrix = SkMatrix::Concat(view, srcToDst);
    enum ScaleFactor {
        kUpscale_ScaleFactor,
        kNoScale_ScaleFactor,
        kDownscale_ScaleFactor,
        kLargeDownscale_ScaleFactor,

        kLast_ScaleFactor = kLargeDownscale_ScaleFactor
    };

    float rawScaleFactor = matrix.getMinScale();

    ScaleFactor scaleFactor;
    if (rawScaleFactor < 0.5f) {
        scaleFactor = kLargeDownscale_ScaleFactor;
    } else if (rawScaleFactor < 1.0f) {
        scaleFactor = kDownscale_ScaleFactor;
    } else if (rawScaleFactor > 1.0f) {
        scaleFactor = kUpscale_ScaleFactor;
    } else {
        scaleFactor = kNoScale_ScaleFactor;
    }

    switch (filterQuality) {
        case kNone_SkFilterQuality:
            SK_HISTOGRAM_ENUMERATION("DrawScaleFactor.NoneFilterQuality", scaleFactor,
                                     kLast_ScaleFactor + 1);
            break;
        case kLow_SkFilterQuality:
            SK_HISTOGRAM_ENUMERATION("DrawScaleFactor.LowFilterQuality", scaleFactor,
                                     kLast_ScaleFactor + 1);
            break;
        case kMedium_SkFilterQuality:
            SK_HISTOGRAM_ENUMERATION("DrawScaleFactor.MediumFilterQuality", scaleFactor,
                                     kLast_ScaleFactor + 1);
            break;
        case kHigh_SkFilterQuality:
            SK_HISTOGRAM_ENUMERATION("DrawScaleFactor.HighFilterQuality", scaleFactor,
                                     kLast_ScaleFactor + 1);
            break;
    }

    // Also log filter quality independent scale factor.
    SK_HISTOGRAM_ENUMERATION("DrawScaleFactor.AnyFilterQuality", scaleFactor,
                             kLast_ScaleFactor + 1);

    // Also log an overall histogram of filter quality.
    SK_HISTOGRAM_ENUMERATION("FilterQuality", filterQuality, kLast_SkFilterQuality + 1);
#endif
}
