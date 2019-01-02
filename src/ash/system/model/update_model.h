// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_UPDATE_MODEL_H_
#define ASH_SYSTEM_MODEL_UPDATE_MODEL_H_

#include "ash/public/interfaces/update.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"

namespace ash {

class UpdateObserver {
 public:
  virtual ~UpdateObserver() {}

  virtual void OnUpdateAvailable() = 0;
};

// Model to store system update availability.
class UpdateModel {
 public:
  UpdateModel();
  ~UpdateModel();

  void AddObserver(UpdateObserver* observer);
  void RemoveObserver(UpdateObserver* observer);

  // Store the state that a software update is available. The state persists
  // until reboot. Based on |severity|, |factory_reset_required| and |rollback|,
  // the observer views can indicate the severity of the update to users by
  // changing the icon, color, and tooltip.
  void SetUpdateAvailable(mojom::UpdateSeverity severity,
                          bool factory_reset_required,
                          bool rollback,
                          mojom::UpdateType update_type);

  // Store the state of the notification according to the RelaunchNotification
  // policy. State persists until reboot or another call to this function.
  // The |notification_body| changes the text of the notification, as it
  // contains a countdown until the required reboot.
  void SetUpdateNotificationState(mojom::NotificationStyle style,
                                  const base::string16& notification_title,
                                  const base::string16& notification_body);

  // If |available| is true, a software update is available but user's agreement
  // is required as current connection is cellular. If |available| is false, the
  // user's one time permission on update over cellular connection has been
  // granted.
  void SetUpdateOverCellularAvailable(bool available);

  mojom::UpdateSeverity GetSeverity() const;

  bool update_required() const { return update_required_; }
  bool factory_reset_required() const { return factory_reset_required_; }
  bool rollback() const { return rollback_; }
  mojom::UpdateType update_type() const { return update_type_; }
  mojom::NotificationStyle notification_style() const {
    return notification_style_;
  }
  const base::string16& notification_title() const {
    return notification_title_;
  }
  const base::string16& notification_body() const { return notification_body_; }
  bool update_over_cellular_available() const {
    return update_over_cellular_available_;
  }

 private:
  void NotifyUpdateAvailable();

  bool update_required_ = false;
  mojom::UpdateSeverity severity_ = mojom::UpdateSeverity::NONE;
  bool factory_reset_required_ = false;
  bool rollback_ = false;
  mojom::UpdateType update_type_ = mojom::UpdateType::SYSTEM;
  mojom::NotificationStyle notification_style_ =
      mojom::NotificationStyle::DEFAULT;
  base::string16 notification_title_;
  base::string16 notification_body_;
  bool update_over_cellular_available_ = false;

  base::ObserverList<UpdateObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(UpdateModel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_UPDATE_MODEL_H_
