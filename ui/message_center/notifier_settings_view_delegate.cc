// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notifier_settings_view_delegate.h"

namespace message_center {

Notifier::Notifier(const std::string& id,
                   const string16& name,
                   bool enabled)
    : id(id),
      name(name),
      enabled(enabled),
      type(APPLICATION) {
}

Notifier::Notifier(const GURL& url, const string16& name, bool enabled)
    : url(url),
      name(name),
      enabled(enabled),
      type(WEB_PAGE) {
}

Notifier::~Notifier() {
}

}  // namespace message_center
