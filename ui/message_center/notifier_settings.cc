// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ui/message_center/notifier_settings.h"

namespace message_center {

NotifierId::NotifierId(NotifierType type,
                       const std::string& id)
    : type(type),
      id(id),
      system_component_type(-1) {
  DCHECK(type == APPLICATION || type == SYNCED_NOTIFICATION_SERVICE);
  DCHECK(!id.empty());
}

NotifierId::NotifierId(const GURL& url)
    : type(WEB_PAGE),
      url(url),
      system_component_type(-1) {}

NotifierId::NotifierId(int type)
    : type(SYSTEM_COMPONENT),
      system_component_type(type) {
  DCHECK_LE(0, system_component_type);
}

NotifierId::NotifierId()
    : type(SYSTEM_COMPONENT),
      system_component_type(-1) {
}

bool NotifierId::operator==(const NotifierId& other) const {
  if (type != other.type)
    return false;

  switch (type) {
    case WEB_PAGE:
      return url == other.url;
    case SYSTEM_COMPONENT:
      return system_component_type == other.system_component_type;
    case APPLICATION:
    case SYNCED_NOTIFICATION_SERVICE:
      return id == other.id;
  }

  NOTREACHED();
  return false;
}

Notifier::Notifier(const NotifierId& notifier_id,
                   const string16& name,
                   bool enabled)
    : notifier_id(notifier_id),
      name(name),
      enabled(enabled) {
}

Notifier::~Notifier() {
}

NotifierGroup::NotifierGroup(const gfx::Image& icon,
                             const string16& name,
                             const string16& login_info,
                             size_t index)
    : icon(icon), name(name), login_info(login_info), index(index) {}

NotifierGroup::~NotifierGroup() {}

}  // namespace message_center
