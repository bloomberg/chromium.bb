// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedMockedURL_h
#define ScopedMockedURL_h

#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"

namespace blink {

namespace test {

// Convenience classes that register a mocked URL on construction, and
// unregister it on destruction. This prevent mocked URL from leaking to other
// tests.
class ScopedMockedURL {
 public:
  explicit ScopedMockedURL(const WebURL&);
  virtual ~ScopedMockedURL();

 private:
  WebURL url_;
};

class ScopedMockedURLLoad : ScopedMockedURL {
 public:
  ScopedMockedURLLoad(
      const WebURL& full_url,
      const WebString& file_path,
      const WebString& mime_type = WebString::FromUTF8("text/html"));
  ~ScopedMockedURLLoad() override = default;
};

}  // namespace test

}  // namespace blink

#endif
