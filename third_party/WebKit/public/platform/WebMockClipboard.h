// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMockClipboard_h
#define WebMockClipboard_h

#include "base/containers/span.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebImage.h"

namespace blink {

// Provides convenience methods for retrieving data from the mock clipboard
// used in layout and unit tests.
class BLINK_PLATFORM_EXPORT WebMockClipboard : public WebClipboard {
 public:
  virtual WebImage ReadRawImage(Buffer) { return WebImage(); }

 protected:
  // Convenience method for WebMockClipoard implementations to create a blob
  // from bytes.
  WebBlobInfo CreateBlobFromData(base::span<const uint8_t> data,
                                 const WebString& mime_type);
};

}  // namespace blink

#endif
