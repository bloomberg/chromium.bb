// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_CONFIG_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_CONFIG_H_

// Clients can modify this file to customize the Nearby C++ codebase as per
// their particular constraints and environments.

// Note: Every entry in this file should conform to the following format, to
// give precedence to command-line options (-D) that set these symbols:
//
//   #ifndef XXX
//   #define XXX 0/1
//   #endif

#ifndef NEARBY_USE_STD_STRING
#define NEARBY_USE_STD_STRING 0
#endif

#ifndef NEARBY_USE_RTTI
#define NEARBY_USE_RTTI 0
#endif

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_CONFIG_H_
