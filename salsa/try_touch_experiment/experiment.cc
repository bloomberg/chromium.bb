// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experiment.h"

using base::SplitString;
using std::string;
using std::vector;

Experiment::Experiment() : is_valid_(false) {}

bool Experiment::valid() const {
  return is_valid_;
}

Experiment::Experiment(string const &experiment_string) {
  vector<string> treatment_strings;
  SplitString(experiment_string, '+', &treatment_strings);

  is_valid_ = true;
  for (vector<string>::const_iterator it = treatment_strings.begin();
       it != treatment_strings.end(); ++it) {
    treatments_.push_back(Treatment(*it));
    is_valid_ = is_valid_ && treatments_.back().valid();
  }
}

bool Experiment::Reset() const {
  if (!is_valid_)
    return false;
  return treatments_[0].Reset();
}

bool Experiment::ApplyTreatment(unsigned int treatment_num) const {
  if (treatment_num >= treatments_.size())
    return false;
  return treatments_[treatment_num].Apply();
}

int Experiment::Size() const {
  return treatments_.size();
}
