// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/PaintPrinters.h"

#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include <iomanip>  // NOLINT
#include <ostream>  // NOLINT

namespace {
class StreamStateSaver : private std::ios {
  WTF_MAKE_NONCOPYABLE(StreamStateSaver);

 public:
  StreamStateSaver(std::ios& other) : std::ios(nullptr), m_other(other) {
    copyfmt(other);
  }
  ~StreamStateSaver() { m_other.copyfmt(*this); }

 private:
  std::ios& m_other;
};
}  // unnamed namespace

namespace blink {

void PrintTo(const PaintChunk& chunk, std::ostream* os) {
  *os << "PaintChunk(begin=" << chunk.beginIndex << ", end=" << chunk.endIndex
      << ", id=";
  if (!chunk.id) {
    *os << "null";
  } else {
    *os << "(" << &chunk.id->client << ", ";
#ifndef NDEBUG
    *os << DisplayItem::typeAsDebugString(chunk.id->type);
#else
    *os << static_cast<int>(chunk.id->type);
#endif
    *os << ")";
  }
  *os << ", props=";
  PrintTo(chunk.properties, os);
  *os << ", bounds=";
  PrintTo(chunk.bounds, os);
  *os << ", knownToBeOpaque=" << chunk.knownToBeOpaque << ")";

  *os << ", rerasterizationRects=[";
  bool first = true;
  for (auto& r : chunk.rasterInvalidationRects) {
    if (!first)
      *os << ", ";
    first = false;
    PrintTo(r, os);
  };
  *os << "]";
}

void PrintTo(const PaintChunkProperties& properties, std::ostream* os) {
  *os << "PaintChunkProperties(";
  bool printedProperty = false;
  if (properties.propertyTreeState.transform()) {
    *os << "transform=";
    PrintTo(*properties.propertyTreeState.transform(), os);
    printedProperty = true;
  }

  if (properties.propertyTreeState.clip()) {
    if (printedProperty)
      *os << ", ";
    *os << "clip=";
    PrintTo(*properties.propertyTreeState.clip(), os);
    printedProperty = true;
  }

  if (properties.propertyTreeState.effect()) {
    if (printedProperty)
      *os << ", ";
    *os << "effect=";
    PrintTo(*properties.propertyTreeState.effect(), os);
    printedProperty = true;
  }

  if (printedProperty)
    *os << ", ";
  *os << "backfaceHidden=" << properties.backfaceHidden;

  *os << ")";
}

void PrintTo(const ClipPaintPropertyNode& node, std::ostream* os) {
  *os << "ClipPaintPropertyNode(" << node.toString().ascii().data() << ")";
}

void PrintTo(const TransformPaintPropertyNode& node, std::ostream* os) {
  *os << "TransformPaintPropertyNode(" << node.toString().ascii().data() << ")";
}

void PrintTo(const EffectPaintPropertyNode& node, std::ostream* os) {
  *os << "EffectPaintPropertyNode(" << node.toString().ascii().data() << ")";
}

void PrintTo(const ScrollPaintPropertyNode& node, std::ostream* os) {
  *os << "ScrollPaintPropertyNode(" << node.toString().ascii().data() << ")";
}

}  // namespace blink
