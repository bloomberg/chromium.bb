/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_MULTIMEDIA_HANDLER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_MULTIMEDIA_HANDLER_H_

#include <string>
#include <vector>

class NaClCommandLoop;

// Log all PPAPI events to given file for later playback.
// This will take effect after HandlerSDLEventLoop is called.
// Note: the emitted file will contain non-standard PPAPI events
//       for synchronization purposes.
void RecordPPAPIEvents(std::string filename);

// Ignore user events and instead play back PPAPI events from the given file,
// which must have been generared via RecordPPAPIEvents()
// This will take effect after HandlerSDLEventLoop is called
void ReplayPPAPIEvents(std::string filename);


// initialize multimedia (SDL) subsystem
bool HandlerSDLInitialize(NaClCommandLoop* ncl,
                          const std::vector<std::string>& args);

// start multimedia event loop
bool HandlerSDLEventLoop(NaClCommandLoop* ncl,
                         const std::vector<std::string>& args);


#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_MULTIMEDIA_HANDLER_H_ */
