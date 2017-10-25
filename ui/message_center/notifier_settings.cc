// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "ui/message_center/notifier_settings.h"

namespace message_center {

NotifierId::NotifierId(NotifierType type,
                       const std::string& id)
    : type(type),
      id(id) {
  DCHECK(type != WEB_PAGE);
  DCHECK(!id.empty());
}

NotifierId::NotifierId(const GURL& url)
    : type(WEB_PAGE),
      url(url) {}

NotifierId::NotifierId()
    : type(SYSTEM_COMPONENT) {
}

NotifierId::NotifierId(const NotifierId& other) = default;

bool NotifierId::operator==(const NotifierId& other) const {
  if (type != other.type)
    return false;

  if (profile_id != other.profile_id)
    return false;

  if (type == WEB_PAGE)
    return url == other.url;

  return id == other.id;
}

bool NotifierId::operator<(const NotifierId& other) const {
  if (type != other.type)
    return type < other.type;

  if (profile_id != other.profile_id)
    return profile_id < other.profile_id;

  if (type == WEB_PAGE)
    return url < other.url;

  return id < other.id;
}

NotifierUiData::NotifierUiData(const NotifierId& notifier_id,
                               const base::string16& name,
                               bool has_advanced_settings,
                               bool enabled)
    : notifier_id(notifier_id),
      name(name),
      has_advanced_settings(has_advanced_settings),
      enabled(enabled) {}

NotifierUiData::~NotifierUiData() {}

}  // namespace message_center
