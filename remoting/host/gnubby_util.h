// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_GNUBBY_UTIL_H_
#define REMOTING_HOST_GNUBBY_UTIL_H_

#include <string>

#include "base/basictypes.h"

namespace base {

class DictionaryValue;

}  // namespace base

namespace remoting {

// Get a gnubbyd sign request json string from a request blob.
bool GetJsonFromGnubbyRequest(const char* data,
                              int data_len,
                              std::string* json);

// Get response blob data from a gnubbyd sign reply json string.
void GetGnubbyResponseFromJson(const std::string& json, std::string* data);

}  // namespace remoting

#endif  // REMOTING_HOST_GNUBBY_UTIL_H_
