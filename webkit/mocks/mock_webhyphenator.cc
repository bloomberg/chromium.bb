// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/mocks/mock_webhyphenator.h"

#include "base/logging.h"
#include "base/memory/scoped_handle.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "third_party/hyphen/hyphen.h"

namespace webkit_glue {

MockWebHyphenator::MockWebHyphenator()
    : hyphen_dictionary_(NULL) {
}

MockWebHyphenator::~MockWebHyphenator() {
}

void MockWebHyphenator::LoadDictionary(base::PlatformFile dict_file) {
}

bool MockWebHyphenator::canHyphenate(const WebKit::WebString& locale) {
  return false;
}

size_t MockWebHyphenator::computeLastHyphenLocation(
    const WebKit::WebString& characters,
    size_t before_index,
    const WebKit::WebString& locale) {
  return 0;
}

}  // namespace webkit_glue
