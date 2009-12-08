// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_PROFILER_AGENT_IMPL_H_
#define WEBKIT_GLUE_DEVTOOLS_PROFILER_AGENT_IMPL_H_

#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

#include "base/basictypes.h"
#include "v8.h"
#include "webkit/glue/devtools/profiler_agent.h"

class ProfilerAgentImpl : public ProfilerAgent {
 public:
  ProfilerAgentImpl(ProfilerAgentDelegate* delegate)
      : delegate_(delegate) {}
  virtual ~ProfilerAgentImpl() {}

  // ProfilerAgent implementation.

  // This method is called on IO thread.
  virtual void GetActiveProfilerModules();

  // This method is called on IO thread.
  virtual void GetLogLines(int position);

 private:
  ProfilerAgentDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerAgentImpl);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_PROFILER_AGENT_IMPL_H_
