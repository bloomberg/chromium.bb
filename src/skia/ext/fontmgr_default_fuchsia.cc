// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/fontmgr_default.h"

#include <fuchsia/fonts/cpp/fidl.h>

#include "base/fuchsia/service_directory_client.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontMgr_fuchsia.h"

namespace skia {

SK_API sk_sp<SkFontMgr> CreateDefaultSkFontMgr() {
  return SkFontMgr_New_Fuchsia(
      base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
          ->ConnectToServiceSync<fuchsia::fonts::Provider>());
}

}  // namespace skia