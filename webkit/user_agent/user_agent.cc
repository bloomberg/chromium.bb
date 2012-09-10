// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/user_agent/user_agent.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "webkit/user_agent/user_agent_util.h"

namespace webkit_glue {

namespace {

class UserAgentState {
 public:
  UserAgentState();
  ~UserAgentState();

  void Set(const std::string& user_agent, bool overriding);
  const std::string& Get(const GURL& url) const;

 private:
  mutable std::string user_agent_;
  // The UA string when we're pretending to be Mac Safari or Win Firefox.
  mutable std::string user_agent_for_spoofing_hack_;

  mutable bool user_agent_requested_;
  bool user_agent_is_overridden_;

  // This object can be accessed from multiple threads, so use a lock around
  // accesses to the data members.
  mutable base::Lock lock_;
};

UserAgentState::UserAgentState()
    : user_agent_requested_(false),
      user_agent_is_overridden_(false) {
}

UserAgentState::~UserAgentState() {
}

void UserAgentState::Set(const std::string& user_agent, bool overriding) {
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
  user_agent_is_overridden_ = overriding;
  user_agent_ = user_agent;
}

bool IsMicrosoftSiteThatNeedsSpoofingForSilverlight(const GURL& url) {
#if defined(OS_MACOSX)
  // The landing page for updating Silverlight gives a confusing experience
  // in browsers that Silverlight doesn't officially support; spoof as
  // Safari to reduce the chance that users won't complete updates.
  // Should be removed if the sniffing is removed: http://crbug.com/88211
  if (url.host() == "www.microsoft.com" &&
      StartsWithASCII(url.path(), "/getsilverlight", false)) {
    return true;
  }
#endif
  return false;
}

bool IsYahooSiteThatNeedsSpoofingForSilverlight(const GURL& url) {
#if defined(OS_MACOSX) || defined(OS_WIN)
  // The following Yahoo! JAPAN pages erroneously judge that Silverlight does
  // not support Chromium. Until the pages are fixed, spoof the UA.
  // http://crbug.com/104426
  if (url.host() == "headlines.yahoo.co.jp" &&
      StartsWithASCII(url.path(), "/videonews/", true)) {
    return true;
  }
#endif
#if defined(OS_MACOSX)
  if ((url.host() == "downloads.yahoo.co.jp" &&
      StartsWithASCII(url.path(), "/docs/silverlight/", true)) ||
      url.host() == "gyao.yahoo.co.jp") {
    return true;
  }
#elif defined(OS_WIN)
  if ((url.host() == "weather.yahoo.co.jp" &&
        StartsWithASCII(url.path(), "/weather/zoomradar/", true)) ||
      url.host() == "promotion.shopping.yahoo.co.jp" ||
      url.host() == "pokemon.kids.yahoo.co.jp") {
    return true;
  }
#endif
  return false;
}

const std::string& UserAgentState::Get(const GURL& url) const {
  base::AutoLock auto_lock(lock_);
  user_agent_requested_ = true;

  DCHECK(!user_agent_.empty());

  // Workarounds for sites that use misguided UA sniffing.
  if (!user_agent_is_overridden_) {
    if (IsMicrosoftSiteThatNeedsSpoofingForSilverlight(url) ||
        IsYahooSiteThatNeedsSpoofingForSilverlight(url)) {
      if (user_agent_for_spoofing_hack_.empty()) {
#if defined(OS_MACOSX)
        user_agent_for_spoofing_hack_ =
            BuildUserAgentFromProduct("Version/5.1.1 Safari/534.51.22");
#elif defined(OS_WIN)
        // Pretend to be Firefox. Silverlight doesn't support Win Safari.
        base::StringAppendF(
            &user_agent_for_spoofing_hack_,
            "Mozilla/5.0 (%s) Gecko/20100101 Firefox/8.0",
            webkit_glue::BuildOSCpuInfo().c_str());
#endif
      }
      DCHECK(!user_agent_for_spoofing_hack_.empty());
      return user_agent_for_spoofing_hack_;
    }
  }

  return user_agent_;
}

base::LazyInstance<UserAgentState> g_user_agent = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void SetUserAgent(const std::string& user_agent, bool overriding) {
  g_user_agent.Get().Set(user_agent, overriding);
}

const std::string& GetUserAgent(const GURL& url) {
  return g_user_agent.Get().Get(url);
}

}  // namespace webkit_glue
