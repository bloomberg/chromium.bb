// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GENERATED FROM THE SCHEMA DEFINITION AND DESCRIPTION IN
//   fieldtrial_testing_config_schema.json
//   test_config.json
// DO NOT EDIT.

#include "test_ouput.h"


const FieldTrialGroupParams array_kFieldTrialConfig_params[] = {
    {
      "x",
      "1",
    },
    {
      "y",
      "2",
    },
};
const FieldTrialTestingGroup array_kFieldTrialConfig_groups[] = {
  {
    "TestStudy1",
    "TestGroup1",
    NULL,
    0,
  },
  {
    "TestStudy2",
    "TestGroup2",
    array_kFieldTrialConfig_params,
    2,
  },
};
const FieldTrialTestingConfig kFieldTrialConfig = {
  array_kFieldTrialConfig_groups,
  2,
};
