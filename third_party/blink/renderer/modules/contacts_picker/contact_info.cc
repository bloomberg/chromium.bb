// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/contacts_picker/contact_info.h"

namespace blink {

ContactInfo::ContactInfo(base::Optional<Vector<String>> name,
                         base::Optional<Vector<String>> email,
                         base::Optional<Vector<String>> tel)
    : name_(std::move(name)), email_(std::move(email)), tel_(std::move(tel)) {}

ContactInfo::~ContactInfo() = default;

const Vector<String> ContactInfo::name(bool& is_null) const {
  is_null = name_.has_value();
  return is_null ? Vector<String>() : name_.value();
}

const Vector<String> ContactInfo::email(bool& is_null) const {
  is_null = email_.has_value();
  return is_null ? Vector<String>() : email_.value();
}

const Vector<String> ContactInfo::tel(bool& is_null) const {
  is_null = tel_.has_value();
  return is_null ? Vector<String>() : tel_.value();
}

}  // namespace blink
