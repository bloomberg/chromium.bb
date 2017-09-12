// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef TextSuggestionInfo_h
#define TextSuggestionInfo_h

#include "platform/wtf/text/WTFString.h"

namespace blink {

struct TextSuggestionInfo {
  int32_t marker_tag;
  uint32_t suggestion_index;

  int32_t span_start;
  int32_t span_end;

  String prefix;
  String suggestion;
  String suffix;
};

}  // namespace blink

#endif  // TextSuggestionList_h
