// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BATTOR_AGENT_BATTOR_ERROR_H_
#define TOOLS_BATTOR_AGENT_BATTOR_ERROR_H_

namespace battor {

// A BattOrError is an error that occurs when communicating with a BattOr.
enum BattOrError {
  BATTOR_ERROR_NONE,
  BATTOR_ERROR_CONNECTION_FAILED,
  BATTOR_ERROR_TIMEOUT,
};

}

#endif  // TOOLS_BATTOR_AGENT_BATTOR_ERROR_H_
