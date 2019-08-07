// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/infolist_entry.h"

namespace ui {

InfolistEntry::InfolistEntry(const base::string16& title,
                             const base::string16& body)
    : title(title), body(body), highlighted(false) {}

bool InfolistEntry::operator==(const InfolistEntry& other) const {
  return title == other.title && body == other.body &&
      highlighted == other.highlighted;
}

bool InfolistEntry::operator!=(const InfolistEntry& other) const {
  return !(*this == other);
}

}  // namespace ui
