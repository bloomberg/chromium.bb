// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_PARSER_UTILS_COMMAND_LINE_ARGUMENTS_SANITIZER_H_
#define CHROME_CHROME_CLEANER_PARSERS_PARSER_UTILS_COMMAND_LINE_ARGUMENTS_SANITIZER_H_

#include <cstring>
#include <vector>

#include "base/strings/string16.h"

namespace chrome_cleaner {

// Receives a string of space separated command line arguments, sanitizes
// each one of them and returns them inside a vector.
std::vector<base::string16> SanitizeArguments(const base::string16& arguments);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_PARSER_UTILS_COMMAND_LINE_ARGUMENTS_SANITIZER_H_
