// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_util.h"

#include "ash/shell.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot_aura.h"

namespace ash::test {

namespace {
void SnapshotCallback(base::RunLoop* run_loop,
                      gfx::Image* ret_image,
                      gfx::Image image) {
  *ret_image = image;
  run_loop->Quit();
}
}  // namespace

bool TakePrimaryDisplayScreenshotAndSave(const base::FilePath& file_path) {
  // Return false if the file extension is not "png".
  if (file_path.Extension() != ".png")
    return false;

  // Return false if `file_path`'s directory does not exist.
  const base::FilePath directory_name = file_path.DirName();
  if (!base::PathExists(directory_name))
    return false;

  base::RunLoop run_loop;
  gfx::Image image;
  ui::GrabWindowSnapshotAsyncAura(
      Shell::Get()->GetPrimaryRootWindow(),
      Shell::Get()->GetPrimaryRootWindow()->bounds(),
      base::BindOnce(&SnapshotCallback, &run_loop, &image));
  run_loop.Run();
  auto data = image.As1xPNGBytes();
  int data_size = static_cast<int>(data->size());
  DCHECK_GT(data_size, 0);
  int written_size = base::WriteFile(
      file_path, reinterpret_cast<const char*>(data->front()), data_size);
  return written_size == data_size;
}

}  // namespace ash::test
