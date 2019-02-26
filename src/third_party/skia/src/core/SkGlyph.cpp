/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkGlyph.h"

#include "SkArenaAlloc.h"
#include "SkScalerContext.h"

void SkGlyph::initWithGlyphID(SkPackedGlyphID glyph_id) {
    fID             = glyph_id;
    fImage          = nullptr;
    fPathData       = nullptr;
    fMaskFormat     = MASK_FORMAT_UNKNOWN;
    fForceBW        = 0;
}

void SkGlyph::toMask(SkMask* mask) const {
    SkASSERT(mask);

    mask->fImage = (uint8_t*)fImage;
    mask->fBounds.set(fLeft, fTop, fLeft + fWidth, fTop + fHeight);
    mask->fRowBytes = this->rowBytes();
    mask->fFormat = static_cast<SkMask::Format>(fMaskFormat);
}

void SkGlyph::zeroMetrics() {
    fAdvanceX = 0;
    fAdvanceY = 0;
    fWidth    = 0;
    fHeight   = 0;
    fTop      = 0;
    fLeft     = 0;
}

static size_t bits_to_bytes(size_t bits) {
    return (bits + 7) >> 3;
}

static size_t format_alignment(SkMask::Format format) {
    switch (format) {
        case SkMask::kBW_Format:
        case SkMask::kA8_Format:
        case SkMask::k3D_Format:
        case SkMask::kSDF_Format:
            return alignof(uint8_t);
        case SkMask::kARGB32_Format:
            return alignof(uint32_t);
        case SkMask::kLCD16_Format:
            return alignof(uint16_t);
        default:
            SK_ABORT("Unknown mask format.");
            break;
    }
    return 0;
}

static size_t format_rowbytes(int width, SkMask::Format format) {
    return format == SkMask::kBW_Format ? bits_to_bytes(width)
                                        : width * format_alignment(format);
}

size_t SkGlyph::formatAlignment() const {
    auto format = static_cast<SkMask::Format>(fMaskFormat);
    return format_alignment(format);
}

size_t SkGlyph::allocImage(SkArenaAlloc* alloc) {
    auto size = this->computeImageSize();
    auto format = static_cast<SkMask::Format>(fMaskFormat);
    fImage = alloc->makeBytesAlignedTo(size, format_alignment(format));

    return size;
}

size_t SkGlyph::rowBytes() const {
    return format_rowbytes(fWidth, (SkMask::Format)fMaskFormat);
}

size_t SkGlyph::rowBytesUsingFormat(SkMask::Format format) const {
    return format_rowbytes(fWidth, format);
}

size_t SkGlyph::computeImageSize() const {
    size_t size = this->rowBytes() * fHeight;

    if (fMaskFormat == SkMask::k3D_Format) {
        size *= 3;
    }

    return size;
}

size_t SkGlyph::copyImageData(const SkGlyph& from, SkArenaAlloc* alloc) {
    fMaskFormat = from.fMaskFormat;
    fWidth = from.fWidth;
    fHeight = from.fHeight;
    fLeft = from.fLeft;
    fTop = from.fTop;
    fForceBW = from.fForceBW;

    if (from.fImage != nullptr) {
        auto imageSize = this->allocImage(alloc);
        SkASSERT(imageSize == from.computeImageSize());

        memcpy(fImage, from.fImage, imageSize);
        return imageSize;
    }

    return 0u;
}

SkPath* SkGlyph::addPath(SkScalerContext* scalerContext, SkArenaAlloc* alloc) {
    if (!this->isEmpty()) {
        if (fPathData == nullptr) {
            SkGlyph::PathData* pathData = alloc->make<SkGlyph::PathData>();
            fPathData = pathData;
            pathData->fIntercept = nullptr;
            SkPath* path = new SkPath;
            if (scalerContext->getPath(this->getPackedID(), path)) {
                path->updateBoundsCache();
                path->getGenerationID();
                pathData->fPath = path;
            } else {
                pathData->fPath = nullptr;
                delete path;
            }
        }
    }
    return fPathData ? fPathData->fPath : nullptr;
}

