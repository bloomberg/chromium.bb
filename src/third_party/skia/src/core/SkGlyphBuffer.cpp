/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkGlyphBuffer.h"
#include "src/core/SkGlyphRunPainter.h"
#include "src/core/SkStrikeForGPU.h"

void SkSourceGlyphBuffer::reset() {
    fRejectedGlyphIDs.reset();
    fRejectedPositions.reset();
}

void SkDrawableGlyphBuffer::ensureSize(size_t size) {
    if (size > fMaxSize) {
        fMultiBuffer.reset(size);
        fPositions.reset(size);
        fMaxSize = size;
    }

    fInputSize = 0;
    fDrawableSize = 0;
}

void SkDrawableGlyphBuffer::startSource(const SkZip<const SkGlyphID, const SkPoint>& source) {
    fInputSize = source.size();
    fDrawableSize = 0;

    auto positions = source.get<1>();
    memcpy(fPositions, positions.data(), positions.size() * sizeof(SkPoint));

    // Convert from SkGlyphIDs to SkPackedGlyphIDs.
    SkGlyphVariant* packedIDCursor = fMultiBuffer;
    for (auto t : source) {
        *packedIDCursor++ = SkPackedGlyphID{std::get<0>(t)};
    }
    SkDEBUGCODE(fPhase = kInput);
}

void SkDrawableGlyphBuffer::startBitmapDevice(
        const SkZip<const SkGlyphID, const SkPoint>& source,
        SkPoint origin, const SkMatrix& viewMatrix,
        const SkGlyphPositionRoundingSpec& roundingSpec) {
    fInputSize = source.size();
    fDrawableSize = 0;

    // Map the positions including subpixel position.
    auto positions = source.get<1>();
    SkMatrix matrix = viewMatrix;
    matrix.preTranslate(origin.x(), origin.y());
    SkPoint halfSampleFreq = roundingSpec.halfAxisSampleFreq;
    matrix.postTranslate(halfSampleFreq.x(), halfSampleFreq.y());
    matrix.mapPoints(fPositions, positions.data(), positions.size());

    // Mask for controlling axis alignment.
    SkIPoint mask = roundingSpec.ignorePositionFieldMask;

    // Convert glyph ids and positions to packed glyph ids.
    SkZip<const SkGlyphID, const SkPoint> withMappedPos =
            SkMakeZip(source.get<0>(), fPositions.get());
    SkGlyphVariant* packedIDCursor = fMultiBuffer;
    for (auto [glyphID, pos] : withMappedPos) {
        *packedIDCursor++ = SkPackedGlyphID{glyphID, pos, mask};
    }
    SkDEBUGCODE(fPhase = kInput);
}

SkPoint SkDrawableGlyphBuffer::startGPUDevice(
        const SkZip<const SkGlyphID, const SkPoint>& source,
        SkPoint origin, const SkMatrix& viewMatrix,
        const SkGlyphPositionRoundingSpec& roundingSpec) {
    fInputSize = source.size();
    fDrawableSize = 0;

    // Build up the mapping from source space to device space. Add the rounding constant
    // halfSampleFreq so we just need to floor to get the device result.
    SkMatrix device = viewMatrix;
    SkPoint halfSampleFreq = roundingSpec.halfAxisSampleFreq;
    device.postTranslate(halfSampleFreq.x(), halfSampleFreq.y());
    device.preTranslate(origin.x(), origin.y());

    auto positions = source.get<1>();
    device.mapPoints(fPositions, positions.data(), positions.size());

    auto floor = [](SkPoint pt) -> SkPoint {
        return {SkScalarFloorToScalar(pt.x()), SkScalarFloorToScalar(pt.y())};
    };

    // Map the origin from source space to device space without the halfSampleFreq offset.
    SkPoint originMappedToDevice = viewMatrix.mapXY(origin.x(), origin.y());

    for (auto [packedGlyphID, glyphID, pos]
            : SkMakeZip(fMultiBuffer.get(), source.get<0>(), fPositions.get())) {
        packedGlyphID = SkPackedGlyphID{glyphID, pos, roundingSpec.ignorePositionFieldMask};
        // Store rounded device coords back in pos.
        pos = floor(pos);
    }

    SkDEBUGCODE(fPhase = kInput);
    // Return the origin mapped through the initial matrix.
    return originMappedToDevice;
}


void SkDrawableGlyphBuffer::reset() {
    SkDEBUGCODE(fPhase = kReset);
    if (fMaxSize > 200) {
        fMultiBuffer.reset();
        fPositions.reset();
        fMaxSize = 0;
    }
    fInputSize = 0;
    fDrawableSize = 0;
}

