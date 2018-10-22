// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_ORIGIN_UTIL_H_
#define IOS_WEB_PUBLIC_ORIGIN_UTIL_H_

class GURL;
@class WKSecurityOrigin;

namespace web {

// Returns true if the origin is trustworthy: that is, if its content can be
// said to have been transferred to the browser in a way that a network attacker
// cannot tamper with or observe.
//
// See https://www.w3.org/TR/powerful-features/#is-origin-trustworthy.
bool IsOriginSecure(const GURL& url);

// Converts WKSecurityOrigin to GURL origin.
// Returns empty url if |origin| is nil.
GURL GURLOriginWithWKSecurityOrigin(WKSecurityOrigin* origin);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_ORIGIN_UTIL_H_
