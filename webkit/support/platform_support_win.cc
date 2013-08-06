// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/platform_support.h"

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/win/resource_util.h"
#include "grit/blink_resources.h"
#include "grit/webkit_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/support/test_webkit_platform_support.h"


namespace webkit_support {

void BeforeInitialize() {
}

void AfterInitialize() {
}

void BeforeShutdown() {
}

void AfterShutdown() {
}

}  // namespace webkit_support

