// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/AddStringToDigestor.h"

#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebCrypto.h"

namespace blink {

void AddStringToDigestor(WebCryptoDigestor* digestor, const String& string) {
  const CString c_string = string.Utf8();
  digestor->Consume(reinterpret_cast<const unsigned char*>(c_string.data()),
                    c_string.length());
}

}  // namespace blink
