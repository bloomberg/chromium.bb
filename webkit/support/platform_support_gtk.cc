// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/platform_support.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "gfx/gfx_module.h"
#include "grit/webkit_resources.h"
#include "ui/base/resource/data_pack.h"

namespace {

// Data resources on linux.  This is a pointer to the mmapped resources file.
ui::DataPack* g_resource_data_pack = NULL;

base::StringPiece TestResourceProvider(int resource_id) {
  base::StringPiece res;
  if (g_resource_data_pack)
    g_resource_data_pack->GetStringPiece(resource_id, &res);
  return res;
}

}

namespace webkit_support {

// TODO(tkent): Implement some of the followings for platform-dependent tasks
// such as loading resource.

void BeforeInitialize(bool unit_test_mode) {
}

void AfterInitialize(bool unit_test_mode) {
  if (unit_test_mode)
    return;  // We don't have a resource pack when running the unit-tests.
  g_resource_data_pack = new ui::DataPack;
  FilePath data_path;
  PathService::Get(base::DIR_EXE, &data_path);
  data_path = data_path.Append("DumpRenderTree.pak");
  if (!g_resource_data_pack->Load(data_path))
    LOG(FATAL) << "failed to load DumpRenderTree.pak";

  // Config the modules that need access to a limited set of resources.
  gfx::GfxModule::SetResourceProvider(TestResourceProvider);
}

void BeforeShutdown() {
}

void AfterShutdown() {
}

}  // namespace webkit_support

namespace webkit_glue {

string16 GetLocalizedString(int message_id) {
  base::StringPiece res;
  if (!g_resource_data_pack->GetStringPiece(message_id, &res))
    LOG(FATAL) << "failed to load webkit string with id " << message_id;

  return string16(reinterpret_cast<const char16*>(res.data()),
                  res.length() / 2);
}

base::StringPiece GetDataResource(int resource_id) {
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
  base::StringPiece res;
  g_resource_data_pack->GetStringPiece(resource_id, &res);
  return res;
}

}  // namespace webkit_glue
