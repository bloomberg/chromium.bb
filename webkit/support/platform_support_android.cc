// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/platform_support.h"

#include "base/android/jni_android.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "base/test/test_support_android.h"
#include "googleurl/src/gurl.h"
#include "grit/webkit_resources.h"
#include "media/base/android/media_jni_registrar.h"
#include "net/android/net_jni_registrar.h"
#include "net/android/network_library.h"
#include "ui/android/ui_jni_registrar.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/support/test_webkit_platform_support.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

namespace {

// The place where the Android layout test script will put the required tools
// and resources. Must keep consistent with DEVICE_DRT_DIR in
// WebKit/Tools/Scripts/webkitpy/layout_tests/port/chromium_android.py.
const char kDumpRenderTreeDir[] = "/data/local/tmp/drt";

}

namespace webkit_support {

void BeforeInitialize(bool unit_test_mode) {
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

  // Chromium binaries will register their Jni bindings through the library
  // loader that is part of content/. WebKit uses a different path, so the
  // bindings have to be initialized separately as well.
  if (!unit_test_mode) {
    media::RegisterJni(env);
    net::android::RegisterJni(env);
    ui::android::RegisterJni(env);
  }
}

void AfterInitialize(bool unit_test_mode) {
  base::FilePath data_path(kDumpRenderTreeDir);
  data_path = data_path.Append("DumpRenderTree.pak");
  ResourceBundle::InitSharedInstanceWithPakPath(data_path);

  if (unit_test_mode)
    return;

  // We enable file-over-http to bridge the file protocol to http protocol
  // in here, which can
  // (1) run the layout and performance tests on android target device, but
  // never need to push the test files and corresponding resources to device,
  // which saves huge running time.
  // (2) still run non-http layout (tests not under LayoutTests/http) tests
  // via file protocol without breaking test environment / convention of webkit
  // layout tests, which are followed by current all webkit ports.
  SimpleResourceLoaderBridge::AllowFileOverHTTP(
      "third_party/WebKit/LayoutTests/",
      GURL("http://127.0.0.1:8000/all-tests/"));
  SimpleResourceLoaderBridge::AllowFileOverHTTP(
      "third_party/WebKit/PerformanceTests/",
      GURL("http://127.0.0.1:8000/all-perf-tests/"));
}

void BeforeShutdown() {
  ResourceBundle::CleanupSharedInstance();
}

void AfterShutdown() {
}

}  // namespace webkit_support

base::string16 TestWebKitPlatformSupport::GetLocalizedString(int message_id) {
  return ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
}

base::StringPiece TestWebKitPlatformSupport::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  base::FilePath resources_path(kDumpRenderTreeDir);
  resources_path = resources_path.Append("DumpRenderTree_resources");
  switch (resource_id) {
    case IDR_BROKENIMAGE: {
      CR_DEFINE_STATIC_LOCAL(std::string, broken_image_data, ());
      if (broken_image_data.empty()) {
        base::FilePath path = resources_path.Append("missingImage.gif");
        bool success = file_util::ReadFileToString(path, &broken_image_data);
        if (!success)
          LOG(FATAL) << "Failed reading: " << path.value();
      }
      return broken_image_data;
    }
    case IDR_TEXTAREA_RESIZER: {
      CR_DEFINE_STATIC_LOCAL(std::string, resize_corner_data, ());
      if (resize_corner_data.empty()) {
        base::FilePath path = resources_path.Append("textAreaResizeCorner.png");
        bool success = file_util::ReadFileToString(path, &resize_corner_data);
        if (!success)
          LOG(FATAL) << "Failed reading: " << path.value();
      }
      return resize_corner_data;
    }
  }

  return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}
