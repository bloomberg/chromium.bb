// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_APPLY_CONTENT_PROTECTION_TASK_H_
#define UI_DISPLAY_MANAGER_APPLY_CONTENT_PROTECTION_TASK_H_

#include <cstddef>
#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/manager/display_configurator.h"

namespace display {

class DisplayLayoutManager;
class NativeDisplayDelegate;

// In order to apply content protection the task executes in the following
// manner:
// 1) Run()
//   a) Query NativeDisplayDelegate for HDCP state on capable displays
//   b) OnGetHDCPState() called for each display as response to (a)
// 2) ApplyProtections()
//   a) Compute preferred HDCP state for capable displays
//   b) Call into NativeDisplayDelegate to set HDCP state on capable displays
//   c) OnSetHDCPState() called for each display as response to (b)
// 3) Call |callback_| to signal end of task.
//
// Note, in steps 1a and 2a, if no HDCP capable displays are found or if errors
// are reported, the task finishes early and skips to step 3.
class DISPLAY_MANAGER_EXPORT ApplyContentProtectionTask
    : public DisplayConfigurator::ContentProtectionTask {
 public:
  using ResponseCallback = base::OnceCallback<void(Status)>;

  ApplyContentProtectionTask(DisplayLayoutManager* layout_manager,
                             NativeDisplayDelegate* native_display_delegate,
                             DisplayConfigurator::ContentProtections requests,
                             ResponseCallback callback);
  ~ApplyContentProtectionTask() override;

  void Run() override;

 private:
  void OnGetHDCPState(int64_t display_id, bool success, HDCPState state);
  void OnSetHDCPState(bool success);

  void ApplyProtections();

  uint32_t GetDesiredProtectionMask(int64_t display_id) const;

  DisplayLayoutManager* const layout_manager_;            // Not owned.
  NativeDisplayDelegate* const native_display_delegate_;  // Not owned.

  const DisplayConfigurator::ContentProtections requests_;
  ResponseCallback callback_;

  base::flat_map<int64_t /* display_id */, HDCPState> hdcp_states_;

  bool success_ = true;
  size_t pending_requests_ = 0;

  base::WeakPtrFactory<ApplyContentProtectionTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ApplyContentProtectionTask);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_APPLY_CONTENT_PROTECTION_TASK_H_
