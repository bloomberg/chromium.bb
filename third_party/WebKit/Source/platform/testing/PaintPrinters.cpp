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
  *os << "PaintChunk(begin=" << chunk.begin_index << ", end=" << chunk.end_index
      << ", id=(" << &chunk.id.client << ", ";
#ifndef NDEBUG
  *os << DisplayItem::TypeAsDebugString(chunk.id.type);
#else
  *os << static_cast<int>(chunk.id.type);
#endif
  *os << "), cacheable=" << chunk.is_cacheable;
  *os << ", props=";
  PrintTo(chunk.properties, os);
  *os << ", bounds=";
  PrintTo(chunk.bounds, os);
  *os << ", knownToBeOpaque=" << chunk.known_to_be_opaque << ")";

  *os << ", rerasterizationRects=[";
  bool first = true;
  for (auto& r : chunk.raster_invalidation_rects) {
    if (!first)
      *os << ", ";
    first = false;
    PrintTo(r, os);
  };
  *os << "]";
}

void PrintTo(const PaintChunkProperties& properties, std::ostream* os) {
  *os << "PaintChunkProperties(";
  bool printed_property = false;
  if (properties.property_tree_state.Transform()) {
    *os << "transform=";
    PrintTo(*properties.property_tree_state.Transform(), os);
    printed_property = true;
  }

  if (properties.property_tree_state.Clip()) {
    if (printed_property)
      *os << ", ";
    *os << "clip=";
    PrintTo(*properties.property_tree_state.Clip(), os);
    printed_property = true;
  }

  if (properties.property_tree_state.Effect()) {
    if (printed_property)
      *os << ", ";
    *os << "effect=";
    PrintTo(*properties.property_tree_state.Effect(), os);
    printed_property = true;
  }

  if (printed_property)
    *os << ", ";
  *os << "backfaceHidden=" << properties.backface_hidden;

  *os << ")";
}

void PrintTo(const ClipPaintPropertyNode& node, std::ostream* os) {
  *os << "ClipPaintPropertyNode(" << node.ToString().Ascii().data() << ")";
}

void PrintTo(const TransformPaintPropertyNode& node, std::ostream* os) {
  *os << "TransformPaintPropertyNode(" << node.ToString().Ascii().data() << ")";
}

void PrintTo(const EffectPaintPropertyNode& node, std::ostream* os) {
  *os << "EffectPaintPropertyNode(" << node.ToString().Ascii().data() << ")";
}

void PrintTo(const ScrollPaintPropertyNode& node, std::ostream* os) {
  *os << "ScrollPaintPropertyNode(" << node.ToString().Ascii().data() << ")";
}

}  // namespace blink
