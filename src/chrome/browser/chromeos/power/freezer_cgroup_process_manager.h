// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_FREEZER_CGROUP_PROCESS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_FREEZER_CGROUP_PROCESS_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "chrome/browser/chromeos/power/renderer_freezer.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {

// Manages all the processes in the freezer cgroup on Chrome OS.
class FreezerCgroupProcessManager : public RendererFreezer::Delegate {
 public:
  FreezerCgroupProcessManager();
  ~FreezerCgroupProcessManager() override;

  // RendererFreezer::Delegate overrides.
  void SetShouldFreezeRenderer(base::ProcessHandle handle,
                               bool frozen) override;
  void FreezeRenderers() override;
  void ThawRenderers(ResultCallback callback) override;
  void CheckCanFreezeRenderers(ResultCallback callback) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> file_thread_;

  class FileWorker;
  std::unique_ptr<FileWorker> file_worker_;

  DISALLOW_COPY_AND_ASSIGN(FreezerCgroupProcessManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_FREEZER_CGROUP_PROCESS_MANAGER_H_
