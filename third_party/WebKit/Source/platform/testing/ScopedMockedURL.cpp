// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/ScopedMockedURL.h"

#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"

namespace blink {
namespace testing {

ScopedMockedURL::ScopedMockedURL(const WebURL& url) : m_url(url) {}

ScopedMockedURL::~ScopedMockedURL() {
  Platform::current()->getURLLoaderMockFactory()->unregisterURL(m_url);
}

ScopedMockedURLLoad::ScopedMockedURLLoad(const WebURL& fullURL,
                                         const WebString& filePath,
                                         const WebString& mimeType)
    : ScopedMockedURL(fullURL) {
  URLTestHelpers::registerMockedURLLoad(fullURL, filePath, mimeType);
}

}  // namespace testing
}  // namespace blink
