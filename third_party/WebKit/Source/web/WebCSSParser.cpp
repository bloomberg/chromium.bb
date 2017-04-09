// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebCSSParser.h"

#include "core/css/parser/CSSParser.h"
#include "platform/graphics/Color.h"
#include "public/platform/WebString.h"

namespace blink {

bool WebCSSParser::ParseColor(WebColor* web_color,
                              const WebString& color_string) {
  Color color = Color(*web_color);
  bool success = CSSParser::ParseColor(color, color_string, true);
  *web_color = color.Rgb();
  return success;
}

}  // namespace blink
