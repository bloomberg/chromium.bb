// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/ScopedMockedURL.h"

#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"

namespace blink {
namespace test {

ScopedMockedURL::ScopedMockedURL(const WebURL& url) : url_(url) {}

ScopedMockedURL::~ScopedMockedURL() {
  Platform::Current()->GetURLLoaderMockFactory()->UnregisterURL(url_);
}

ScopedMockedURLLoad::ScopedMockedURLLoad(const WebURL& full_url,
                                         const WebString& file_path,
                                         const WebString& mime_type)
    : ScopedMockedURL(full_url) {
  URLTestHelpers::RegisterMockedURLLoad(full_url, file_path, mime_type);
}

}  // namespace test
}  // namespace blink
