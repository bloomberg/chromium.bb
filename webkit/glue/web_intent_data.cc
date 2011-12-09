// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/web_intent_data.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntent.h"

namespace webkit_glue {

WebIntentData::WebIntentData() {
}

WebIntentData::~WebIntentData() {
}

WebIntentData::WebIntentData(const WebKit::WebIntent& intent)
    : action(intent.action()),
      type(intent.type()),
      data(intent.data()) {
}

}  // namespace webkit_glue
