/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGPathParser_h
#define SVGPathParser_h

#include "core/CoreExport.h"
#include "core/svg/SVGPathData.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGPathConsumer;

namespace SVGPathParser {

template <typename SourceType, typename ConsumerType>
inline bool ParsePath(SourceType& source, ConsumerType& consumer) {
  while (source.HasMoreData()) {
    PathSegmentData segment = source.ParseSegment();
    if (segment.command == kPathSegUnknown)
      return false;

    consumer.EmitSegment(segment);
  }
  return true;
}

}  // namespace SVGPathParser

class SVGPathNormalizer {
  STACK_ALLOCATED();

 public:
  SVGPathNormalizer(SVGPathConsumer* consumer)
      : consumer_(consumer), last_command_(kPathSegUnknown) {
    DCHECK(consumer_);
  }

  void EmitSegment(const PathSegmentData&);

 private:
  bool DecomposeArcToCubic(const FloatPoint& current_point,
                           const PathSegmentData&);

  SVGPathConsumer* consumer_;
  FloatPoint control_point_;
  FloatPoint current_point_;
  FloatPoint sub_path_point_;
  SVGPathSegType last_command_;
};

}  // namespace blink

#endif  // SVGPathParser_h
