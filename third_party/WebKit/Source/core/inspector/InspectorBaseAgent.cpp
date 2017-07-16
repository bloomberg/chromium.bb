// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorBaseAgent.h"

#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Vector.h"

namespace blink {

using SessionInitCallbackVector = Vector<InspectorAgent::SessionInitCallback>;
SessionInitCallbackVector& GetSessionInitCallbackVector() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(SessionInitCallbackVector,
                                  session_init_vector, ());
  return session_init_vector;
}

void InspectorAgent::RegisterSessionInitCallback(SessionInitCallback callback) {
  GetSessionInitCallbackVector().push_back(callback);
}

void InspectorAgent::CallSessionInitCallbacks(InspectorSession* session,
                                              bool allow_view_agents,
                                              InspectorDOMAgent* dom_agent,
                                              InspectedFrames* inspected_frames,
                                              Page* page) {
  for (auto& callback : GetSessionInitCallbackVector()) {
    callback(session, allow_view_agents, dom_agent, inspected_frames, page);
  }
}

}  // namespace blink
