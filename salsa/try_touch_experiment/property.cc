// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "property.h"

using base::SplitString;
using base::StringPrintf;
using std::string;
using std::vector;

Property::Property() : is_valid_(false) {}

bool Property::valid() const {
  return is_valid_;
}

Property::Property(string const &property_string) {
  vector<string> parts;
  SplitString(property_string, ':', &parts);

  name_ = "";
  if (parts.size() == 2) {
    name_ = parts[0];
    value_ = atof(parts[1].c_str());
    device_ = GetDeviceNumber();
    property_number_ = GetPropertyNumber();
    old_value_ = GetCurrentValue();
    is_valid_ = true;
  }

  if (name_.length() == 0)
    is_valid_ = false;
}

bool Property::Reset() const {
  return SetValue(old_value_);
}

bool Property::Apply() const {
  return SetValue(value_);
}

bool Property::SetValue(double new_value) const {
  string command = StringPrintf("DISPLAY=:0 xinput set-prop %d %d %f",
                                device_, property_number_, new_value);

  double current_value = GetCurrentValue();
  for (int i = 0; i < MAX_RETRIES && current_value != new_value; i++) {
    RunCommand(command);
    current_value = GetCurrentValue();
  }

  return (abs(current_value - new_value) <= MAX_ALLOWABLE_DIFFERENCE);
}

int Property::GetDeviceNumber() const {
  return atoi(RunCommand("/opt/google/touchpad/tpcontrol listdev").c_str());
}

int Property::GetPropertyNumber() const {
  string command = StringPrintf("DISPLAY=:0 xinput list-props %d"
                                " | grep '%s'"
                                " | sed -e 's/[^(]*(\\([0-9]*\\)):.*/\\1/'",
                                device_,
                                name_.c_str());
  return atoi(RunCommand(command).c_str());
}

double Property::GetCurrentValue() const {
  string command = StringPrintf("DISPLAY=:0 xinput list-props %d"
                                " | grep '%s'"
                                " | sed -e 's/[^:]*:\\s*\\([0-9.]*\\)$/\\1/'",
                                device_,
                                name_.c_str());
  return atoi(RunCommand(command).c_str());
}

string Property::RunCommand(string const &command) const {
  // Run a command from a shell and return the contents of stdout
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe)
    return "";

  char buffer[256];
  string result = "";
  while (!feof(pipe)) {
    if (fgets(buffer, 256, pipe) != NULL)
      result += buffer;
  }

  pclose(pipe);

  TrimWhitespaceASCII(result, TRIM_ALL, &result);
  return result;
}
