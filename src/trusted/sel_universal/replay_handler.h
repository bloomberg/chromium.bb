/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_REPLAY_HANDLER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_REPLAY_HANDLER_H_

#include <string>
#include <vector>

using std::string;
using std::vector;
class NaClCommandLoop;

bool HandlerReplay(NaClCommandLoop* ncl, const vector<string>& args);
bool HandlerReplayActivate(NaClCommandLoop* ncl, const vector<string>& args);
bool HandlerUnusedReplays(NaClCommandLoop* ncl, const vector<string>& args);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_REPLAY_HANDLER_H_ */
