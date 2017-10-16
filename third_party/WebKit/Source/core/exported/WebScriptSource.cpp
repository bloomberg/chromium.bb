// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebScriptSource.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "platform/wtf/text/TextPosition.h"

namespace blink {

WebScriptSource::operator ScriptSourceCode() const {
  TextPosition position(OrdinalNumber::FromOneBasedInt(start_line),
                        OrdinalNumber::First());
  return ScriptSourceCode(code, url, position);
}

}  // namespace blink
