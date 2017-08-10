// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/PaintPrinters.h"

#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include <iomanip>  // NOLINT
#include <ostream>  // NOLINT

namespace blink {

void PrintTo(const PaintChunk& chunk, std::ostream* os) {
  *os << "PaintChunk(" << chunk.ToString() << ")";
}

void PrintTo(const PaintChunkProperties& properties, std::ostream* os) {
  *os << "PaintChunkProperties(" << properties.ToString() << ")";
}

void PrintTo(const ClipPaintPropertyNode& node, std::ostream* os) {
  *os << "ClipPaintPropertyNode(" << node.ToString() << ")";
}

void PrintTo(const TransformPaintPropertyNode& node, std::ostream* os) {
  *os << "TransformPaintPropertyNode(" << node.ToString() << ")";
}

void PrintTo(const EffectPaintPropertyNode& node, std::ostream* os) {
  *os << "EffectPaintPropertyNode(" << node.ToString() << ")";
}

void PrintTo(const ScrollPaintPropertyNode& node, std::ostream* os) {
  *os << "ScrollPaintPropertyNode(" << node.ToString() << ")";
}

}  // namespace blink
