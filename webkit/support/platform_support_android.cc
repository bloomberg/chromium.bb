// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/platform_support.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "googleurl/src/gurl.h"
#include "grit/webkit_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/support/test_webkit_platform_support.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

namespace webkit_support {

void BeforeInitialize(bool unit_test_mode) {
  NOTIMPLEMENTED(); // TODO(zhenghao): Implement this function.
}

void AfterInitialize(bool unit_test_mode) {
  if (unit_test_mode)
    return;  // We don't have a resource pack when running the unit-tests.

  FilePath data_path;
  PathService::Get(base::DIR_EXE, &data_path);
  data_path = data_path.Append("DumpRenderTree.pak");
  ResourceBundle::InitSharedInstanceForTest(data_path);

  // We enable file-over-http to bridge the file protocol to http protocol
  // in here, which can
  // (1) run the layout tests on android target device, but never need to
  // push the test files and corresponding resources to device, which saves
  // huge running time.
  // (2) still run non-http layout (tests not under LayoutTests/http) tests
  // via file protocol without breaking test environment / convention of webkit
  // layout tests, which are followed by current all webkit ports.
  SimpleResourceLoaderBridge::AllowFileOverHTTP(
      "third_party/WebKit/LayoutTests/",
      GURL("http://127.0.0.1:8000/all-tests/"));
}

void BeforeShutdown() {
  ResourceBundle::CleanupSharedInstance();
}

void AfterShutdown() {
  NOTIMPLEMENTED(); // TODO(zhenghao): Implement this function.
}

}  // namespace webkit_support

string16 TestWebKitPlatformSupport::GetLocalizedString(int message_id) {
  return ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
}

base::StringPiece TestWebKitPlatformSupport::GetDataResource(int resource_id) {
  FilePath resources_path;
  PathService::Get(base::DIR_EXE, &resources_path);
  resources_path = resources_path.Append("DumpRenderTree_resources");
  switch (resource_id) {
    case IDR_BROKENIMAGE: {
      static std::string broken_image_data;
      if (broken_image_data.empty()) {
        FilePath path = resources_path.Append("missingImage.gif");
        bool success = file_util::ReadFileToString(path, &broken_image_data);
        if (!success)
          LOG(FATAL) << "Failed reading: " << path.value();
      }
      return broken_image_data;
    }
    case IDR_TEXTAREA_RESIZER: {
      static std::string resize_corner_data;
      if (resize_corner_data.empty()) {
        FilePath path = resources_path.Append("textAreaResizeCorner.png");
        bool success = file_util::ReadFileToString(path, &resize_corner_data);
        if (!success)
          LOG(FATAL) << "Failed reading: " << path.value();
      }
      return resize_corner_data;
    }
  }

  return ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
}
