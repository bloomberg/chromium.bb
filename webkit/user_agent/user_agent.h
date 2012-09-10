// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_USER_AGENT_USER_AGENT_H_
#define WEBKIT_USER_AGENT_USER_AGENT_H_

#include <string>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "webkit/user_agent/webkit_user_agent_export.h"

namespace webkit_glue {

// Sets the user agent.  Pass true for overriding if this is a custom
// user agent instead of the default one (in order to turn off any browser
// sniffing workarounds). This must be called before GetUserAgent() can
// be called.
WEBKIT_USER_AGENT_EXPORT void SetUserAgent(const std::string& user_agent,
                                           bool overriding);

// Returns the user agent to use for the given URL. SetUserAgent() must
// be called prior to calling this function.
WEBKIT_USER_AGENT_EXPORT const std::string& GetUserAgent(const GURL& url);

}  // namespace webkit_glue

#endif  // WEBKIT_USER_AGENT_USER_AGENT_H_
