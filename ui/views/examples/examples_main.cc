// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"

#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  base::AtExitManager at_exit;
  base::MessageLoopForUI message_loop;

  base::FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);

  base::FilePath pak_file;
  pak_file = pak_dir.Append(FILE_PATH_LITERAL("ui_test.pak"));

  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);

#if !defined(OS_CHROMEOS)
  scoped_ptr<gfx::Screen> desktop_screen(views::CreateDesktopScreen());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, desktop_screen.get());
#endif

  base::RunLoop().Run();

  ui::ResourceBundle::CleanupSharedInstance();

  return 0;
}
