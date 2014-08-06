// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/keyboard/webui/vk_webui_controller.h"

namespace keyboard {

static bool initialized = false;

void ResetKeyboardForTesting() {
  initialized = false;
}

void InitializeKeyboard() {
  if (initialized)
    return;
  initialized = true;

  base::FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);
  base::FilePath pak_file = pak_dir.Append(
      FILE_PATH_LITERAL("keyboard_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_100P);
}

void InitializeWebUIBindings() {
  CHECK(initialized);
  base::FilePath content_resources;
  DCHECK(PathService::Get(base::DIR_MODULE, &content_resources));
  content_resources =
      content_resources.Append(FILE_PATH_LITERAL("content_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      content_resources, ui::SCALE_FACTOR_100P);

  content::WebUIControllerFactory::RegisterFactory(
      VKWebUIControllerFactory::GetInstance());
}

}  // namespace keyboard
