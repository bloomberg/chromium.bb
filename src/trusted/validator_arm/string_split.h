/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_STRING_SPLIT_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_STRING_SPLIT_H__

#include <string>
#include <vector>

// ----------------------------------------------------------------------
// SplitStringUsing()
//    Split a string using a character delimiter. Append the components
//    to 'result'.  If there are consecutive delimiters, this function skips
//    over all of them.
// ----------------------------------------------------------------------
void SplitStringUsing(const std::string& full, const char* delim,
                      std::vector<std::string>* res);

#endif  // NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_STRING_SPLIT_H__
