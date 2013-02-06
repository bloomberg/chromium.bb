// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_SALSA_TRY_TOUCH_EXPERIMENT_PROPERTY_H_
#define GESTURES_SALSA_TRY_TOUCH_EXPERIMENT_PROPERTY_H_

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <base/stringprintf.h>
#include <base/string_split.h>
#include <base/string_util.h>

#define MAX_RETRIES 5
#define MAX_ALLOWABLE_DIFFERENCE 0.0001

class Property {
  public:
    Property();
    explicit Property(const std::string &property_string);

    bool Apply() const;
    bool Reset() const;

    bool valid() const;

  private:
    double GetCurrentValue() const;
    int GetPropertyNumber() const;
    int GetDeviceNumber() const;
    std::string RunCommand(std::string const &command) const;
    bool SetValue(double new_value) const;

    std::string name_;
    double value_;
    double old_value_;
    int device_;
    int property_number_;

    bool is_valid_;
};




#endif  // GESTURES_SALSA_TRY_TOUCH_EXPERIMENT_PROPERTY_H_
