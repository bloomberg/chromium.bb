// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedMockedURL_h
#define ScopedMockedURL_h

#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"

namespace blink {

namespace testing {

// Convenience classes that register a mocked URL on construction, and
// unregister it on destruction. This prevent mocked URL from leaking to other
// tests.
class ScopedMockedURL {
 public:
  explicit ScopedMockedURL(const WebURL&);
  virtual ~ScopedMockedURL();

 private:
  WebURL m_url;
};

class ScopedMockedURLLoad : ScopedMockedURL {
 public:
  ScopedMockedURLLoad(
      const WebURL& fullURL,
      const WebString& filePath,
      const WebString& mimeType = WebString::fromUTF8("text/html"));
  ~ScopedMockedURLLoad() override = default;
};

}  // namespace testing

}  // namespace blink

#endif
