// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_PROFILER_AGENT_H_
#define WEBKIT_GLUE_DEVTOOLS_PROFILER_AGENT_H_

#include "webkit/glue/devtools/devtools_rpc.h"

// Profiler agent provides API for retrieving profiler data.
// These methods are handled on the IO thread, so profiler can
// operate while a script on a page performs heavy work.
#define PROFILER_AGENT_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3) \
  /* Requests current profiler state. */                          \
  METHOD0(GetActiveProfilerModules)                               \
                                                                  \
  /* Retrieves portion of profiler log. */                        \
  METHOD1(GetLogLines, int /* position */)

DEFINE_RPC_CLASS(ProfilerAgent, PROFILER_AGENT_STRUCT)

#define PROFILER_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3) \
  /* Response to GetActiveProfilerModules. */                           \
  METHOD1(DidGetActiveProfilerModules, int /* flags */)                 \
                                                                        \
  /* Response to GetLogLines. */                                        \
  METHOD2(DidGetLogLines, int /* position */, String /* log */)

DEFINE_RPC_CLASS(ProfilerAgentDelegate, PROFILER_AGENT_DELEGATE_STRUCT)

#endif  // WEBKIT_GLUE_DEVTOOLS_PROFILER_AGENT_H_
