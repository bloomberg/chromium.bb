// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGTextFragmentPaintInfo_h
#define NGTextFragmentPaintInfo_h

#include "platform/PlatformExport.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

class ShapeResult;

// Bridge struct for painting text. Encapsulates info needed by the paint code.
struct PLATFORM_EXPORT NGTextFragmentPaintInfo {
  // The string to paint. May include surrounding context.
  const StringView text;

  // The range of the |text| to paint.
  unsigned from;
  unsigned to;

  // The |shape_result| may not contain all characters of the |text|, but is
  // guaranteed to contain |from| to |to|.
  const ShapeResult* shape_result;
};

}  // namespace blink

#endif  // NGTextFragmentPaintInfo_h
