// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/testing/PaintPrinters.h"

#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintProperties.h"
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

void PrintTo(const PaintProperties& properties, std::ostream* os)
{
    *os << "PaintProperties()";
}

} // namespace blink
