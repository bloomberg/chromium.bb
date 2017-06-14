// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockMimeRegistry_h
#define MockMimeRegistry_h

#include "net/base/mime_util.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/mime_registry.mojom-blink.h"

namespace blink {

// Used for unit tests.
class MockMimeRegistry : public mojom::blink::MimeRegistry {
 public:
  MockMimeRegistry() = default;
  ~MockMimeRegistry() = default;

  void GetMimeTypeFromExtension(
      const String& ext,
      GetMimeTypeFromExtensionCallback callback) override {
    std::string mime_type;
    net::GetMimeTypeFromExtension(WebStringToFilePath(ext).value(), &mime_type);
    std::move(callback).Run(
        String::FromUTF8(mime_type.data(), mime_type.length()));
  }
};

}  // namespace blink

#endif  // MockMimeRegistry_h
