// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_SALSA_TRY_TOUCH_EXPERIMENT_RUNNER_H_
#define GESTURES_SALSA_TRY_TOUCH_EXPERIMENT_RUNNER_H_

#include <ncurses.h>
#include <string>
#include <base/string_util.h>
#include "experiment.h"

class SalsaExperimentRunner {
  public:
    bool LoadExperiment(const std::string &exp_string);
    void run() const;

  private:
    static void StartCurses();
    static void EndCurses();
    std::string Decode(const std::string &exp_string) const;

    Experiment exp_;
};

#endif  // GESTURES_SALSA_TRY_TOUCH_EXPERIMENT_RUNNER_H_
