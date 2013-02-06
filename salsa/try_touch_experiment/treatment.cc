// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "treatment.h"

using base::SplitString;
using std::string;
using std::vector;

Treatment::Treatment() : is_valid_(false) {}

bool Treatment::valid() const {
  return is_valid_;
}

Treatment::Treatment(string const &treatment_string) {
  vector<string> property_strings;
  SplitString(treatment_string, ',', &property_strings);

  is_valid_ = true;
  for (vector<string>::const_iterator it = property_strings.begin();
       it != property_strings.end(); ++it) {
    properties_.push_back(Property(*it));
    is_valid_ = is_valid_ && properties_.back().valid();
  }
}

bool Treatment::Reset() const {
  for (vector<Property>::const_iterator it = properties_.begin();
       it != properties_.end(); ++it) {
    if (!it->Reset())
      return false;
  }
  return true;
}

bool Treatment::Apply() const {
  for (vector<Property>::const_iterator it = properties_.begin();
       it != properties_.end(); ++it) {
    if (!it->Apply())
      return false;
  }
  return true;
}
