/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef SVGPathStringSource_h
#define SVGPathStringSource_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/SVGPathData.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT SVGPathStringSource {
  STACK_ALLOCATED();

 public:
  explicit SVGPathStringSource(const String&);

  bool HasMoreData() const {
    if (is8_bit_source_)
      return current_.character8_ < end_.character8_;
    return current_.character16_ < end_.character16_;
  }
  PathSegmentData ParseSegment();

  SVGParsingError ParseError() const { return error_; }

 private:
  void EatWhitespace();
  float ParseNumberWithError();
  bool ParseArcFlagWithError();
  void SetErrorMark(SVGParseStatus);

  bool is8_bit_source_;

  union {
    const LChar* character8_;
    const UChar* character16_;
  } current_;
  union {
    const LChar* character8_;
    const UChar* character16_;
  } end_;

  SVGPathSegType previous_command_;
  SVGParsingError error_;
  String string_;

  DISALLOW_COPY_AND_ASSIGN(SVGPathStringSource);
};

}  // namespace blink

#endif  // SVGPathStringSource_h
