// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include "salsa_experiment_runner.h"

using std::cout;
using std::endl;

int main(int argc, char** argv) {
  if (argc != 2) {
    cout << "Error: You must supply exactly ONE parameter" << endl;
    cout << "Usage: " << argv[0] << " SALSA_EXPERIMENT_CODE" << endl;
    return 1;
  }

  SalsaExperimentRunner runner;
  if (!runner.LoadExperiment(argv[1])) {
    cout << "Error: Unable to load that experiment.  Please" << endl;
    cout << "ensure you copy/pasted it correctly and try again." << endl;
    return 1;
  }

  runner.run();
  return 0;
}
