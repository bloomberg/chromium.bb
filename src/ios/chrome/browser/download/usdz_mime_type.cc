// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/usdz_mime_type.h"

char kUsdzMimeType[] = "model/vnd.usdz+zip";
char kLegacyUsdzMimeType[] = "model/usd";
char kLegacyPixarUsdzMimeType[] = "model/vnd.pixar.usd";

bool IsUsdzFileFormat(const std::string& mime_type) {
  return mime_type == kUsdzMimeType || mime_type == kLegacyUsdzMimeType ||
         mime_type == kLegacyPixarUsdzMimeType;
}
