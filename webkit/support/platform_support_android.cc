// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/platform_support.h"

#include "base/android/jni_android.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/test/test_support_android.h"
#include "grit/webkit_resources.h"
#include "media/base/android/media_jni_registrar.h"
#include "net/android/net_jni_registrar.h"
#include "net/android/network_library.h"
#include "ui/android/ui_jni_registrar.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gl/android/gl_jni_registrar.h"
#include "url/gurl.h"
#include "webkit/support/test_webkit_platform_support.h"

namespace {

// The place where the Android layout test script will put the required tools
// and resources. Must keep consistent with DEVICE_DRT_DIR in
// WebKit/Tools/Scripts/webkitpy/layout_tests/port/chromium_android.py.
const char kDumpRenderTreeDir[] = "/data/local/tmp/drt";

}

namespace webkit_support {

void BeforeInitialize() {
  base::InitAndroidTestPaths();

  // Place cache under kDumpRenderTreeDir to allow the NRWT script to clear it.
  base::FilePath path(kDumpRenderTreeDir);
  path = path.Append("cache");
  PathService::Override(base::DIR_CACHE, path);

  // Set XML_CATALOG_FILES environment variable to blank to prevent libxml from
  // loading and complaining the non-exsistent /etc/xml/catalog file.
  setenv("XML_CATALOG_FILES", "", 0);

  JNIEnv* env = base::android::AttachCurrentThread();
  net::android::RegisterNetworkLibrary(env);
}

void AfterInitialize() {
}

void BeforeShutdown() {
}

void AfterShutdown() {
}

}  // namespace webkit_support

