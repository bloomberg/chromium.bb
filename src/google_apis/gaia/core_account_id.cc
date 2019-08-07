// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/core_account_id.h"

CoreAccountId::CoreAccountId() = default;

CoreAccountId::~CoreAccountId() = default;

CoreAccountId::CoreAccountId(const char* id) : id(id) {}

CoreAccountId::CoreAccountId(std::string&& id) : id(std::move(id)) {}

CoreAccountId::CoreAccountId(const std::string& id) : id(id) {}

CoreAccountId::operator std::string() const {
  return id;
}

bool CoreAccountId::empty() const {
  return id.empty();
}

bool operator<(const CoreAccountId& lhs, const CoreAccountId& rhs) {
  return lhs.id < rhs.id;
}

bool operator==(const CoreAccountId& lhs, const CoreAccountId& rhs) {
  return lhs.id == rhs.id;
}

bool operator!=(const CoreAccountId& lhs, const CoreAccountId& rhs) {
  return lhs.id != rhs.id;
}

std::ostream& operator<<(std::ostream& out, const CoreAccountId& a) {
  return out << a.id;
}
