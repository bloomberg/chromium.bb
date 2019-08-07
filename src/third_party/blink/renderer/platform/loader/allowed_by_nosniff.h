// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_ALLOWED_BY_NOSNIFF_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_ALLOWED_BY_NOSNIFF_H_

#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class ConsoleLogger;
class FetchContext;
class ResourceResponse;

class PLATFORM_EXPORT AllowedByNosniff final {
 public:
  enum class MimeTypeCheck { kStrict, kLax };

  static bool MimeTypeAsScript(FetchContext&,
                               ConsoleLogger*,
                               const ResourceResponse&,
                               MimeTypeCheck mime_type_check_mode,
                               bool is_worker_global_scope);
};

}  // namespace blink

#endif
