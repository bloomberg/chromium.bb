// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/user_agent/user_agent.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"

namespace webkit_glue {

namespace {

class UserAgentState {
 public:
  UserAgentState();
  ~UserAgentState();

  void Set(const std::string& user_agent);
  const std::string& Get() const;

 private:
  mutable std::string user_agent_;

  mutable bool user_agent_requested_;

  // This object can be accessed from multiple threads, so use a lock around
  // accesses to the data members.
  mutable base::Lock lock_;
};

UserAgentState::UserAgentState()
    : user_agent_requested_(false) {
}

UserAgentState::~UserAgentState() {
}

void UserAgentState::Set(const std::string& user_agent) {
  base::AutoLock auto_lock(lock_);
  if (user_agent == user_agent_) {
    // We allow the user agent to be set multiple times as long as it
    // is set to the same value, in order to simplify unit testing
    // given g_user_agent is a global.
    return;
  }
  DCHECK(!user_agent.empty());
  DCHECK(!user_agent_requested_) << "Setting the user agent after someone has "
      "already requested it can result in unexpected behavior.";
  user_agent_ = user_agent;
}

const std::string& UserAgentState::Get() const {
  base::AutoLock auto_lock(lock_);
  user_agent_requested_ = true;

  DCHECK(!user_agent_.empty());

  return user_agent_;
}

base::LazyInstance<UserAgentState> g_user_agent = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void SetUserAgent(const std::string& user_agent) {
  g_user_agent.Get().Set(user_agent);
}

const std::string& GetUserAgent(const GURL& url) {
  return g_user_agent.Get().Get();
}

}  // namespace webkit_glue
