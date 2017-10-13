// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_CHANGE_QUEUE_H_
#define UI_MESSAGE_CENTER_CHANGE_QUEUE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/message_center/message_center_export.h"

namespace message_center {

class MessageCenterImpl;
class Notification;

// ChangeQueue keeps track of all the changes. This class can be used for
// queuing changes during the message center can't be updated.
class MESSAGE_CENTER_EXPORT ChangeQueue {
 public:
  enum ChangeType {
    CHANGE_TYPE_ADD = 0,
    CHANGE_TYPE_UPDATE,
    CHANGE_TYPE_DELETE
  };

  // Change represents an operation made on a notification.  Since it contains
  // the final state of the notification, we only keep the last change for a
  // particular notification that is in the notification list around.  There are
  // two ids; |id_| is the newest notification id that has been assigned by an
  // update, and |notification_list_id_| is the id of the notification it should
  // be updating as it exists in the notification list.
  class Change;

  ChangeQueue();
  ~ChangeQueue();

  // Called when the message center has appropriate visibility.  Modifies
  // |message_center| but does not retain it.  This also causes the queue to
  // empty itself.
  void ApplyChanges(MessageCenterImpl* message_center);

  // Causes a TYPE_ADD change to be added to the queue.
  void AddNotification(std::unique_ptr<Notification> notification);

  // Causes a TYPE_UPDATE change to be added to the queue.
  void UpdateNotification(const std::string& old_id,
                          std::unique_ptr<Notification> notification);

  // Causes a TYPE_DELETE change to be added to the queue.
  void RemoveNotification(const std::string& id, bool by_user);

  // Returns the pointer to the Notification in the latest Change which has the
  // given ID. If the Change is UPDATE, the ID after the change is searched.
  // If the Change id REMOVE, this returns nullptr.
  // ChangeQueue keeps retaining ownership of the Notification. The returned
  // notification can be modified or be copied by caller.
  Notification* GetLatestNotification(const std::string& id) const;

 private:
  void ApplyChangeInternal(MessageCenterImpl* message_center,
                           std::unique_ptr<Change> change);

  std::vector<std::unique_ptr<Change>> changes_;

  DISALLOW_COPY_AND_ASSIGN(ChangeQueue);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_CHANGE_QUEUE_H_
