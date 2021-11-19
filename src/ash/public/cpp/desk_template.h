// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESK_TEMPLATE_H_
#define ASH_PUBLIC_CPP_DESK_TEMPLATE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/guid.h"
#include "base/time/time.h"
#include "components/app_restore/restore_data.h"

namespace ash {

// Indicates where a desk template originated from.
enum class ASH_PUBLIC_EXPORT DeskTemplateSource {
  // Default value, indicates no value was set.
  kUnknownSource = 0,

  // Desk template created by the user.
  kUser,

  // Desk template pushed through policy.
  kPolicy
};

// Class to represent a desk template. It can be used to create a desk with
// a certain set of application windows specified in |desk_restore_data_|.
class ASH_PUBLIC_EXPORT DeskTemplate {
 public:
  // This constructor is used to instantiate DeskTemplate with a specific
  // source.
  DeskTemplate(const std::string& uuid,
               DeskTemplateSource source,
               const std::string& name,
               const base::Time created_time);

  DeskTemplate(const DeskTemplate&) = delete;
  DeskTemplate& operator=(const DeskTemplate&) = delete;
  ~DeskTemplate();

  base::GUID uuid() const { return uuid_; }
  DeskTemplateSource source() const { return source_; }
  base::Time created_time() const { return created_time_; }

  void set_updated_time(base::Time updated_time) {
    updated_time_ = updated_time;
  }
  void clear_updated_time() { updated_time_ = base::Time(); }

  const std::u16string& template_name() const { return template_name_; }
  void set_template_name(const std::u16string& template_name) {
    template_name_ = template_name;
  }

  const app_restore::RestoreData* desk_restore_data() const {
    return desk_restore_data_.get();
  }

  void set_desk_restore_data(
      std::unique_ptr<app_restore::RestoreData> restore_data) {
    desk_restore_data_ = std::move(restore_data);
  }

  // Used in cases where copies of a DeskTemplate are needed to be made.
  // This specifically used in the DeskSyncBridge which requires a map
  // of DeskTemplate unique pointers to be valid and needs to pass
  // that information in DeskModel callbacks.
  std::unique_ptr<DeskTemplate> Clone();

  // Indicates the last time the user created or updated this template.
  // If this desk template was never updated since creation, its creation time
  // will be returned.
  base::Time GetLastUpdatedTime() const {
    return updated_time_.is_null() ? created_time_ : updated_time_;
  }

  // Indicates whether this template has been updated since creation.
  bool WasUpdatedSinceCreation() const { return !updated_time_.is_null(); }

 private:
  DeskTemplate();

  const base::GUID uuid_;  // We utilize the string based base::GUID to uniquely
                           // identify the template.

  // Indicates the source where this desk template originates from.
  const DeskTemplateSource source_;

  const base::Time created_time_;  // Template creation time.

  // Indicates the last time the user updated this template.
  // If this desk template was never updated since creation, this field will
  // have null value.
  base::Time updated_time_;

  std::u16string template_name_;

  // Contains the app launching and window information that can be used to
  // create a new desk instance with the same set of apps/windows specified in
  // it.
  std::unique_ptr<app_restore::RestoreData> desk_restore_data_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESK_TEMPLATE_H_
