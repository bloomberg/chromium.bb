// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/URLConversion.h"

#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebString.h"
#include "url/gurl.h"

namespace blink {

GURL WebStringToGURL(const WebString& web_string) {
  if (web_string.IsEmpty())
    return GURL();

  String str = web_string;
  if (str.Is8Bit()) {
    // Ensure the (possibly Latin-1) 8-bit string is UTF-8 for GURL.
    StringUTF8Adaptor utf8(str);
    return GURL(utf8.AsStringPiece());
  }

  // GURL can consume UTF-16 directly.
  return GURL(base::StringPiece16(str.Characters16(), str.length()));
}

}  // namespace blink
