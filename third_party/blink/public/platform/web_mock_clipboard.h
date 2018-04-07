// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_MOCK_CLIPBOARD_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_MOCK_CLIPBOARD_H_

#include "base/containers/span.h"
#include "third_party/blink/public/platform/web_clipboard.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_image.h"

namespace blink {

// Provides convenience methods for retrieving data from the mock clipboard
// used in layout and unit tests.
class BLINK_PLATFORM_EXPORT WebMockClipboard : public WebClipboard {
 public:
  virtual WebImage ReadRawImage(mojom::ClipboardBuffer) { return WebImage(); }

 protected:
  // Convenience method for WebMockClipoard implementations to create a blob
  // from bytes.
  WebBlobInfo CreateBlobFromData(base::span<const uint8_t> data,
                                 const WebString& mime_type);
};

}  // namespace blink

#endif
