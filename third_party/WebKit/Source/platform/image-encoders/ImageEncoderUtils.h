// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageEncoderUtils_h
#define ImageEncoderUtils_h

#include "platform/PlatformExport.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT ImageEncoderUtils {
 public:
  enum EncodeReason {
    kEncodeReasonToDataURL = 0,
    kEncodeReasonToBlobCallback = 1,
    kEncodeReasonConvertToBlobPromise = 2,
    kNumberOfEncodeReasons
  };
  static String ToEncodingMimeType(const String& mime_type, const EncodeReason);

  // Default image mime type for toDataURL and toBlob functions
  static const char kDefaultMimeType[];
};

}  // namespace blink

#endif  // ImageEncoderUtils_h
