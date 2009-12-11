// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DEVTOOLS_APU_AGENT_DELEGATE_H_
#define WEBKIT_GLUE_DEVTOOLS_APU_AGENT_DELEGATE_H_

#include "webkit/glue/devtools/devtools_rpc.h"

#define APU_AGENT_DELEGATE_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, MEHTOD4, METHOD5) \
  /* Sends a json object to apu. */ \
  METHOD1(DispatchToApu, String /* data */)

DEFINE_RPC_CLASS(ApuAgentDelegate, APU_AGENT_DELEGATE_STRUCT)

#endif  // WEBKIT_GLUE_DEVTOOLS_APU_AGENT_DELEGATE_H_
