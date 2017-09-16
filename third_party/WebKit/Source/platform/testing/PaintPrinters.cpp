// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/PaintPrinters.h"

#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include <iomanip>  // NOLINT
#include <ostream>  // NOLINT

namespace blink {

// Utf8().data() to print the string as-is instead of being escaped.

void PrintTo(const PaintChunk& chunk, std::ostream* os) {
  *os << "PaintChunk(" << chunk.ToString().Utf8().data() << ")";
}

void PrintTo(const PaintChunkProperties& properties, std::ostream* os) {
  *os << "PaintChunkProperties(" << properties.ToString().Utf8().data() << ")";
}

void PrintTo(const ClipPaintPropertyNode& node, std::ostream* os) {
  *os << "ClipPaintPropertyNode(" << node.ToString().Utf8().data() << ")";
}

void PrintTo(const TransformPaintPropertyNode& node, std::ostream* os) {
  *os << "TransformPaintPropertyNode(" << node.ToString().Utf8().data() << ")";
}

void PrintTo(const EffectPaintPropertyNode& node, std::ostream* os) {
  *os << "EffectPaintPropertyNode(" << node.ToString().Utf8().data() << ")";
}

void PrintTo(const ScrollPaintPropertyNode& node, std::ostream* os) {
  *os << "ScrollPaintPropertyNode(" << node.ToString().Utf8().data() << ")";
}

}  // namespace blink
