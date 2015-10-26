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

void PrintTo(const TransformPaintPropertyNode& transformPaintProperty, std::ostream* os)
{
    *os << "TransformPaintPropertyNode(matrix=";
    PrintTo(transformPaintProperty.matrix(), os);
    *os << ", origin=";
    PrintTo(transformPaintProperty.origin(), os);
    *os << ")";
}

} // namespace blink
