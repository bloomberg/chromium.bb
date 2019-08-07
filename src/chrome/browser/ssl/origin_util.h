// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_ORIGIN_UTIL_H_
#define CHROME_BROWSER_SSL_ORIGIN_UTIL_H_

class PrefService;
class GURL;

// A chrome/browser/ wrapper for content::IsOriginSecure() which additionally
// takes a reference to the current Profile's PrefService. This is used to look
// up and apply enterprise policies to determine whether an origin should be
// considered "secure". This should be preferred in chrome/browser/ over using
// content::IsOriginSecure().
bool IsOriginSecure(const GURL& url, PrefService* prefs);

#endif  // CHROME_BROWSER_SSL_ORIGIN_UTIL_H_
