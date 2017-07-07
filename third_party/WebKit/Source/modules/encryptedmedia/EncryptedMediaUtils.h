// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EncryptedMediaUtils_h
#define EncryptedMediaUtils_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebEncryptedMediaKeyInformation.h"
#include "public/platform/WebEncryptedMediaTypes.h"

namespace blink {

class EncryptedMediaUtils {
  STATIC_ONLY(EncryptedMediaUtils);

 public:
  static WebEncryptedMediaInitDataType ConvertToInitDataType(
      const String& init_data_type);
  static String ConvertFromInitDataType(WebEncryptedMediaInitDataType);

  static WebEncryptedMediaSessionType ConvertToSessionType(
      const String& session_type);
  static String ConvertFromSessionType(WebEncryptedMediaSessionType);

  static String ConvertKeyStatusToString(
      const WebEncryptedMediaKeyInformation::KeyStatus);
};

}  // namespace blink

#endif  // EncryptedMediaUtils_h
