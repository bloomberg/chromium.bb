// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/testing/PaintPrinters.h"

#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include <ostream> // NOLINT

namespace blink {

void PrintTo(const PaintChunk& chunk, std::ostream* os)
{
    *os << "PaintChunk(begin=" << chunk.beginIndex
        << ", end=" << chunk.endIndex
        << ", props=";
    PrintTo(chunk.properties, os);
    *os << ")";
}

void PrintTo(const PaintChunkProperties& properties, std::ostream* os)
{
    *os << "PaintChunkProperties(";
    if (properties.transform) {
        *os << "transform=";
        PrintTo(*properties.transform, os);
    }
    *os << ")";
}

// TODO(pdr): Create and move this to TransformPrinters.cpp.
static void PrintTo(const TransformationMatrix& matrix, std::ostream* os)
{
    TransformationMatrix::DecomposedType decomposition;
    if (!matrix.decompose(decomposition)) {
        *os << "TransformationMatrix(degenerate)";
        return;
    }

    if (matrix.isIdentityOrTranslation()) {
        *os << "TransformationMatrix(translation=(" << decomposition.translateX << "," << decomposition.translateY << "," << decomposition.translateZ << "))";
        return;
    }

    *os << "TransformationMatrix("
        << "translation=(" << decomposition.translateX << "," << decomposition.translateY << "," << decomposition.translateZ << ")"
        << ", scale=(" << decomposition.scaleX << "," << decomposition.scaleY << "," << decomposition.scaleZ << ")"
        << ", skew=(" << decomposition.skewXY << "," << decomposition.skewXZ << "," << decomposition.skewYZ << ")"
        << ", quaternion=(" << decomposition.quaternionX << "," << decomposition.quaternionY << "," << decomposition.quaternionZ << "," << decomposition.quaternionW << ")"
        << ", perspective=(" << decomposition.perspectiveX << "," << decomposition.perspectiveY << "," << decomposition.perspectiveZ << "," << decomposition.perspectiveW << ")"
        << ")";
}

void PrintTo(const TransformPaintPropertyNode& transformPaintProperty, std::ostream* os)
{
    *os << "TransformPaintPropertyNode(matrix=";
    PrintTo(transformPaintProperty.matrix(), os);
    *os << ", origin=";
    PrintTo(transformPaintProperty.origin(), os);
    *os << ")";
}

} // namespace blink
