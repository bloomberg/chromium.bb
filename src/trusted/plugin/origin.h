/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_ORIGIN_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_ORIGIN_H_

#include <string>

namespace nacl {

/*
 * Given an URL, chop off the pathspec part, returning a canonicalized
 * representation of the protospec, host, and optional
 * username/password parts of the URL.  If there are any errors, an
 * empty string is returned.
 *
 * For example, this function would perform the following mapping
 *
 * "HTTP://WWW.Google.Com/foo/bar" -> "http://www.google.com"
 * "hTtP://News.gOoGle.cOm/news/section?pz=1&ned=us&topic=t"
 *      -> "http://news.google.com"
 * "file:///path/to/file" -> "file://localhost"
 */
std::string UrlToOrigin(std::string url);

/*
 * Given an origin string as returned above, return if it is in our
 * internal whitelist.  The current implementation just uses a
 * statically compiled-in array of allowed origin strings; future
 * implementations may use a file or browser config: data and provide
 * a GUI to manage it.
 */
bool OriginIsInWhitelist(std::string origin);

}

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_ORIGIN_H_
