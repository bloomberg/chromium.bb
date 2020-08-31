// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UPDATE_UPDATE_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_UPDATE_UPDATE_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/model/update_model.h"
#include "base/files/file_path.h"
#include "base/macros.h"

namespace ash {

class ShutdownConfirmationDialog;

// Controller class to manage update notification.
class ASH_EXPORT UpdateNotificationController : public UpdateObserver {
 public:
  UpdateNotificationController();
  ~UpdateNotificationController() override;

  // UpdateObserver:
  void OnUpdateAvailable() override;

  // Callback functions for Shutdown Confirmation Dialog which is generated
  // when the device bootup process is occasionally slow - eg. memory training
  // during the bootup due to a system firmware update.
  void RestartForUpdate();
  void RestartCancelled();

 private:
  friend class UpdateNotificationControllerTest;

  bool ShouldShowUpdate() const;
  base::string16 GetNotificationTitle() const;
  base::string16 GetNotificationMessage() const;
  void HandleNotificationClick(base::Optional<int> index);
  void GenerateUpdateNotification(
      base::Optional<bool> slow_boot_file_path_exists);

  static const char kNotificationId[];

  UpdateModel* const model_;

  base::FilePath slow_boot_file_path_;
  bool slow_boot_file_path_exists_ = false;
  ShutdownConfirmationDialog* confirmation_dialog_ = nullptr;

  base::WeakPtrFactory<UpdateNotificationController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UpdateNotificationController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UPDATE_UPDATE_NOTIFICATION_CONTROLLER_H_
