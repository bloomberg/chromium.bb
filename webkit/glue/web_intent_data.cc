// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/web_intent_data.h"

namespace webkit_glue {

WebIntentData::WebIntentData() {
}

WebIntentData::~WebIntentData() {
}

WebIntentData::WebIntentData(const WebIntentData& other)
    : action(other.action),
      type(other.type),
      data(other.data) {
}

}  // namespace webkit_glue
