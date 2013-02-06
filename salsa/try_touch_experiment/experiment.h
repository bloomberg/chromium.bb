// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_SALSA_TRY_TOUCH_EXPERIMENT_EXPERIMENT_H_
#define GESTURES_SALSA_TRY_TOUCH_EXPERIMENT_EXPERIMENT_H_

#include <string>
#include <vector>
#include <base/string_split.h>
#include "treatment.h"

class Experiment {
  public:
    Experiment();
    explicit Experiment(const std::string &experiment_string);

    bool ApplyTreatment(unsigned int treatment_num) const;
    bool Reset() const;

    int Size() const;
    bool valid() const;

  private:
    std::vector<Treatment> treatments_;
    bool is_valid_;
};

#endif  // GESTURES_SALSA_TRY_TOUCH_EXPERIMENT_EXPERIMENT_H_
