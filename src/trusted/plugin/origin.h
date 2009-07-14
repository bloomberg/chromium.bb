/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
